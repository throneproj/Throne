#include "include/ui/setting/route/RouteRuleEditor.h"

#include "include/global/Configs.hpp"
#include "include/ui/setting/ProcessPicker.h"
#include "include/ui/setting/ThemeManager.hpp"
#include "include/ui/setting/route/RouteEditorCommon.h"
#include "include/ui/setting/route/RouteEditorWidgets.h"
#include "include/ui/setting/route/RouteThenPicker.h"
#include "include/ui/utils/NoWheelComboBox.h"

#include <srslist.h>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QClipboard>
#include <QCompleter>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QFocusEvent>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QStringListModel>
#include <QStyle>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace {
    constexpr char ruleEditorSelectProperty[] = "routeRuleEditorSelect";
    constexpr char ruleEditorLeafProperty[] = "routeRuleEditorLeaf";
    constexpr char ruleEditorBaseCountProperty[] = "routeRuleEditorBaseCount";
    // A condition with more values than this plus two shows this many and a "N more" button.
    constexpr int ruleEditorFoldAfter = 10;
    // A Fragment TLS choice's data: the bits of the flags it turns on.
    constexpr int ruleEditorFragmentHandshake = 1;
    constexpr int ruleEditorFragmentRecords = 2;

    const QStringList& ruleEditorOptionAttrs() {
        static const QStringList attrs = {"override_address", "override_port", "tls_fragment", "tls_fragment_fallback_delay",
                                          "tls_record_fragment", "tls_spoof", "tls_spoof_method"};
        return attrs;
    }

    // Word-wrapped, a little smaller than the text around it, and hidden until there is something to say.
    QLabel* ruleEditorIssuesLabel(QWidget* parent) {
        auto* label = new QLabel(parent);
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(true);
        QFont font = label->font();
        if (font.pointSizeF() > 0) font.setPointSizeF(font.pointSizeF() * 0.95);
        else if (font.pixelSize() > 0) font.setPixelSize(qMax(8, qRound(font.pixelSize() * 0.95)));
        label->setFont(font);
        label->hide();
        return label;
    }

    QToolButton* ruleEditorFlatButton(const QString& text, QWidget* parent) {
        auto* button = new QToolButton(parent);
        button->setText(text);
        button->setAutoRaise(true);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setCursor(Qt::PointingHandCursor);
        return button;
    }

    QToolButton* ruleEditorDisclosure(const QString& text, QWidget* parent) {
        auto* button = new QToolButton(parent);
        button->setText(text);
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setArrowType(Qt::RightArrow);
        return button;
    }

    void ruleEditorRestyle(QWidget* widget) {
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
        widget->update();
    }

    // Errors in the danger colour, warnings in the warning colour; hidden when there is nothing to say.
    void ruleEditorShowIssues(QLabel* label, const QStringList& texts, bool error) {
        label->setVisible(!texts.isEmpty());
        label->setText(texts.join(QLatin1Char('\n')));
        const QString role = error ? QStringLiteral("danger") : QString();
        const QString sheet = texts.isEmpty() || error ? QString() : QStringLiteral("color: %1;").arg(themeManager()->tokens.warning.name(QColor::HexArgb));
        if (label->styleSheet() != sheet) label->setStyleSheet(sheet);
        if (label->property("colorRole").toString() != role) {
            label->setProperty("colorRole", role);
            ruleEditorRestyle(label);
        }
    }

    void ruleEditorSelectData(QComboBox* combo, const QString& value) {
        const int base = combo->property(ruleEditorBaseCountProperty).toInt();
        while (combo->count() > base) combo->removeItem(combo->count() - 1);
        int index = combo->findData(value);
        if (index < 0) {
            combo->addItem(value, value);
            index = combo->count() - 1;
        }
        combo->setCurrentIndex(index);
    }

    void ruleEditorSealCombo(QComboBox* combo) {
        combo->setProperty(ruleEditorBaseCountProperty, combo->count());
        combo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    }

    QStringList ruleEditorValueOf(const Configs::RouteRule& rule, const QString& attr) {
        if (Configs::RouteRule::get_input_type(attr) == Configs::trufalse) return {rule.get_current_value_bool(attr)};
        return rule.get_current_value_string(attr);
    }

    bool ruleEditorIsList(const QString& attr) {
        return Configs::RouteRule::get_input_type(attr) == Configs::text && Configs::RouteRule::is_list_attribute(attr);
    }

    bool ruleEditorHasApps(const QString& attr) {
        return attr == QLatin1String("process_name") || attr == QLatin1String("process_path") || attr == QLatin1String("process_path_regex");
    }

    ProcessPicker::Match ruleEditorMatchOf(const QString& attr) {
        if (attr == QLatin1String("process_name")) return ProcessPicker::Match::Name;
        if (attr == QLatin1String("process_path")) return ProcessPicker::Match::Path;
        return ProcessPicker::Match::Folder;
    }

    QAbstractItemModel* ruleEditorRuleSetModel() {
        static auto* model = [] {
            QStringList items;
            items.reserve(static_cast<qsizetype>(ruleSetList.size()));
            for (const auto& item : ruleSetList) items.append(QString::fromUtf8(item.first.data(), static_cast<qsizetype>(item.first.size())));
            return new QStringListModel(items, QCoreApplication::instance());
        }();
        return model;
    }

    RouteEditor::ValueChip* ruleEditorChip(const QString& text, QWidget* parent) {
        auto* chip = new RouteEditor::ValueChip(text, parent);
        chip->setCompact(true);
        return chip;
    }

    // A rounded outline around a group of settings.
    class RuleEditorPanel : public QWidget {
    public:
        using QWidget::QWidget;

    protected:
        void paintEvent(QPaintEvent*) override {
            const auto& tokens = themeManager()->tokens;
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(QPen(RouteEditor::Mix(tokens.onSurface, tokens.surface, 0.16), 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 7, 7);
        }
    };

    // Wide enough for a typical value, without raising the line's minimum width.
    class RuleEditorInlineEdit : public QLineEdit {
    public:
        RuleEditorInlineEdit(int chars, QWidget* parent) : QLineEdit(parent), wideChars(chars) {}

        [[nodiscard]] QSize sizeHint() const override {
            QSize size = QLineEdit::sizeHint();
            size.setWidth(qMax(size.width(), fontMetrics().averageCharWidth() * wideChars + 16));
            return size;
        }

    private:
        int wideChars;
    };

    // A square flat "+", as tall as a compact chip.
    class RuleEditorAddButton : public QToolButton {
    public:
        RuleEditorAddButton(const QString& tip, QWidget* parent) : QToolButton(parent) {
            setText(QStringLiteral("+"));
            setToolTip(tip);
            setAccessibleName(tip);
            setAutoRaise(true);
            setToolButtonStyle(Qt::ToolButtonTextOnly);
            setCursor(Qt::PointingHandCursor);
        }

        [[nodiscard]] QSize sizeHint() const override {
            const int side = fontMetrics().height() + 4;
            return {side, side};
        }

        [[nodiscard]] QSize minimumSizeHint() const override { return sizeHint(); }
    };

    // RuleEditorAddButton's twin with painted text lines, for Edit as text.
    class RuleEditorTextButton : public QToolButton {
    public:
        RuleEditorTextButton(const QString& tip, QWidget* parent) : QToolButton(parent) {
            setToolTip(tip);
            setAccessibleName(tip);
            setAutoRaise(true);
            setToolButtonStyle(Qt::ToolButtonTextOnly);
            setCursor(Qt::PointingHandCursor);
        }

        [[nodiscard]] QSize sizeHint() const override {
            const int side = fontMetrics().height() + 4;
            return {side, side};
        }

        [[nodiscard]] QSize minimumSizeHint() const override { return sizeHint(); }

    protected:
        void paintEvent(QPaintEvent* event) override {
            QToolButton::paintEvent(event);
            const auto& tokens = themeManager()->tokens;
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(QPen(underMouse() || isDown() ? tokens.onSurface : tokens.muted, 1.3, Qt::SolidLine, Qt::RoundCap));
            const qreal side = qMin(width(), height()) * 0.5;
            const QRectF box(QPointF((width() - side) / 2.0, (height() - side) / 2.0), QSizeF(side, side));
            static constexpr qreal lengths[] = {1.0, 0.65, 1.0, 0.5};
            const qreal start = isRightToLeft() ? box.right() : box.left();
            const qreal dir = isRightToLeft() ? -1 : 1;
            for (int i = 0; i < 4; ++i) {
                const qreal y = box.top() + box.height() * i / 3.0;
                painter.drawLine(QPointF(start, y), QPointF(start + dir * box.width() * lengths[i], y));
            }
        }
    };

    // As tall as its lines, between 3 and 14 of them, and as wide as the longest up to 64 characters.
    class RuleEditorTextBox : public QPlainTextEdit {
    public:
        explicit RuleEditorTextBox(QWidget* parent) : QPlainTextEdit(parent) {
            setLineWrapMode(QPlainTextEdit::NoWrap);
            setTabChangesFocus(true);
            setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            connect(this, &QPlainTextEdit::textChanged, this, [this] {
                measured = false;
                updateGeometry();
            });
        }

        [[nodiscard]] QSize sizeHint() const override {
            if (!measured) {
                const QFontMetrics metrics = fontMetrics();
                int widest = metrics.averageCharWidth() * 36;
                int lines = 0;
                for (QTextBlock block = document()->begin(); block.isValid(); block = block.next(), ++lines)
                    widest = qMax(widest, RouteEditor::TextWidth(metrics, block.text()));
                const int limit = metrics.averageCharWidth() * 64;
                const int chrome = 2 * frameWidth() + qRound(2 * document()->documentMargin());
                const int bar = style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, this);
                const int width = qMin(widest, limit) + chrome + bar;
                int height = qBound(3, lines, 14) * metrics.lineSpacing() + chrome;
                if (widest > limit) height += bar;
                hint = {width, height};
                measured = true;
            }
            return hint;
        }

        [[nodiscard]] QSize minimumSizeHint() const override { return {fontMetrics().averageCharWidth() * 20, sizeHint().height()}; }

    private:
        mutable QSize hint;
        mutable bool measured = false;
    };

    // Tints each line holding one of failed; returns a cursor at the first one, null when none.
    QTextCursor ruleEditorMarkLines(QPlainTextEdit* box, const QStringList& failed) {
        QList<QTextEdit::ExtraSelection> marks;
        QTextCursor first;
        QColor tint = themeManager()->tokens.danger;
        tint.setAlphaF(0.18f);
        for (QTextBlock block = box->document()->begin(); block.isValid() && !failed.isEmpty(); block = block.next()) {
            const QString text = block.text();
            const bool bad = std::any_of(failed.cbegin(), failed.cend(), [&text](const QString& value) { return text.contains(value, Qt::CaseInsensitive); });
            if (!bad) continue;
            QTextEdit::ExtraSelection mark;
            mark.format.setBackground(tint);
            mark.format.setProperty(QTextFormat::FullWidthSelection, true);
            mark.cursor = QTextCursor(block);
            if (first.isNull()) first = mark.cursor;
            marks << mark;
        }
        box->setExtraSelections(marks);
        return first;
    }

    // The condition tree's last branch: a dashed outline with muted text.
    class RuleEditorGhostButton : public QPushButton {
    public:
        RuleEditorGhostButton(const QString& text, QWidget* parent) : QPushButton(text, parent) {
            setAutoDefault(false);
            setAttribute(Qt::WA_Hover);
            setCursor(Qt::PointingHandCursor);
            setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        }

        [[nodiscard]] QSize sizeHint() const override {
            const QFontMetrics metrics = fontMetrics();
            return {RouteEditor::TextWidth(metrics, text()) + 14, metrics.height() + 6};
        }

        [[nodiscard]] QSize minimumSizeHint() const override { return sizeHint(); }

    protected:
        void paintEvent(QPaintEvent*) override {
            const auto& tokens = themeManager()->tokens;
            const bool hot = underMouse() || isDown();
            // As styles do: a focus mark only after keyboard navigation.
            const bool focusMark = hasFocus() && window()->testAttribute(Qt::WA_KeyboardFocusChange);
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing, true);
            QPen pen(focusMark ? tokens.accent : RouteEditor::Mix(tokens.onSurface, tokens.surface, 0.28), 1, Qt::SolidLine, Qt::FlatCap);
            if (!focusMark) pen.setDashPattern({3.0, 3.0});
            painter.setPen(pen);
            painter.setBrush(hot ? QBrush(RouteEditor::Mix(tokens.onSurface, tokens.surface, isDown() ? 0.1 : 0.05)) : QBrush(Qt::NoBrush));
            painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 5, 5);
            painter.setPen(hot ? tokens.onSurface : tokens.muted);
            painter.drawText(rect(), Qt::AlignCenter, text());
        }
    };

    // The muted arrow before the Then picker, on the condition tree's spine.
    class RuleEditorThenArrow : public QWidget {
    public:
        RuleEditorThenArrow(RouteEditor::TreeNode* conditions, QWidget* parent) : QWidget(parent), tree(conditions) {
            setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        }

        // As wide as the tree's indent, so the picker lines up with its branches.
        [[nodiscard]] QSize sizeHint() const override { return {tree != nullptr ? tree->indent() : 0, 10}; }
        [[nodiscard]] QSize minimumSizeHint() const override { return sizeHint(); }

    protected:
        void paintEvent(QPaintEvent*) override {
            if (tree == nullptr) return;
            const qreal x = mapFrom(window(), tree->mapTo(window(), QPoint(tree->spineX(), 0))).x() + 0.5;
            const qreal y = height() / 2.0;
            const qreal dir = isRightToLeft() ? -1 : 1;
            QPainterPath arrow;
            arrow.moveTo(x - dir * 5.5, y);
            arrow.lineTo(x + dir * 5.5, y);
            arrow.moveTo(x + dir * 2, y - 3.5);
            arrow.lineTo(x + dir * 5.5, y);
            arrow.lineTo(x + dir * 2, y + 3.5);
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(QPen(themeManager()->tokens.muted, 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(arrow);
        }

    private:
        QPointer<RouteEditor::TreeNode> tree;
    };
} // namespace

RouteRuleEditor::RouteRuleEditor(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    nameEdit = new QLineEdit(this);
    nameEdit->setPlaceholderText(tr("Rule name"));
    nameEdit->setAccessibleName(tr("Rule name"));
    layout->addWidget(nameEdit);
    connect(nameEdit, &QLineEdit::textEdited, this, [this](const QString& text) {
        if (loading || current == nullptr) return;
        current->name = text;
        emitChanged();
    });

    tree = new RouteEditor::TreeNode(tr("AND"), this);
    invertCheck = new QCheckBox(RouteEditor::ConditionLabel(QStringLiteral("invert")), tree);
    invertCheck->setToolTip(tr("Invert: match only what this rule would not match"));
    tree->head()->addWidget(invertCheck);
    addConditionButton = new RuleEditorGhostButton(tr("+ Add condition"), tree);
    auto* conditionMenu = new QMenu(addConditionButton);
    for (const auto& group : RouteEditor::ConditionMenu()) {
        conditionMenu->addSection(group.title);
        for (const QString& attr : group.attrs) {
            connect(conditionMenu->addAction(RouteEditor::ConditionLabel(attr)), &QAction::triggered, this, [this, attr] { addCondition(attr); });
        }
    }
    addConditionButton->setMenu(conditionMenu);
    tree->setTail(addConditionButton);
    tree->setLinesShown(false);
    layout->addWidget(tree);
    connect(invertCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (loading || current == nullptr) return;
        current->invert = on;
        refreshIssues();
        emitChanged();
    });

    buildThenSection(layout);
    auto* drawers = new QVBoxLayout;
    drawers->setSpacing(8);
    // Side by side, wrapping rather than widening the editor.
    auto* toggles = new RouteEditor::FlowLayout(nullptr, 14, 4);
    drawers->addLayout(toggles);
    buildOptions(drawers, toggles);
    buildJson(drawers, toggles);
    layout->addLayout(drawers);
    layout->addStretch(1);

    // Warnings carry their colour in a style sheet of their own.
    connect(themeManager(), &ThemeManager::themeChanged, this, [this] {
        refreshIssues();
        update();
    });
    setEnabled(false);
}

RouteRuleEditor::~RouteRuleEditor() = default;

QLabel* RouteRuleEditor::addSettingIssues(const QStringList& attrs, QWidget* parent) {
    auto* label = ruleEditorIssuesLabel(parent);
    settings.append(Setting{attrs, label});
    return label;
}

void RouteRuleEditor::buildThenSection(QVBoxLayout* layout) {
    auto* section = new QVBoxLayout;
    section->setSpacing(8);
    thenPicker = new RouteThenPicker(this);
    auto* pickerRow = new QHBoxLayout;
    pickerRow->setSpacing(0);
    pickerRow->addWidget(new RuleEditorThenArrow(tree, this));
    pickerRow->addWidget(thenPicker);
    pickerRow->addStretch(1);
    section->addLayout(pickerRow);
    connect(thenPicker, &RouteThenPicker::picked, this, &RouteRuleEditor::onThenPicked);
    thenIssues = ruleEditorIssuesLabel(this);
    section->addWidget(thenIssues);

    auto* extras = new RuleEditorPanel(this);
    extrasFrame = extras;
    auto* extrasLayout = new QVBoxLayout(extras);
    extrasLayout->setContentsMargins(10, 8, 10, 8);
    extrasLayout->setSpacing(6);

    rejectGroup = new QWidget(extras);
    auto* rejectGrid = new QGridLayout(rejectGroup);
    rejectGrid->setContentsMargins(0, 0, 0, 0);
    rejectGrid->setHorizontalSpacing(10);
    rejectGrid->setVerticalSpacing(4);
    auto* rejectLabel = new QLabel(RouteEditor::ConditionLabel(QStringLiteral("method")), rejectGroup);
    rejectCombo = new NoWheelComboBox(rejectGroup);
    rejectCombo->addItem(tr("Default"), QString());
    rejectCombo->addItem(tr("Drop"), QStringLiteral("drop"));
    rejectCombo->addItem(tr("Reply"), QStringLiteral("reply"));
    ruleEditorSealCombo(rejectCombo);
    rejectLabel->setBuddy(rejectCombo);
    rejectGrid->addWidget(rejectLabel, 0, 0);
    rejectGrid->addWidget(rejectCombo, 0, 1, Qt::AlignLeft);
    noDropCheck = new QCheckBox(RouteEditor::ConditionLabel(QStringLiteral("no_drop")), rejectGroup);
    noDropCheck->setToolTip(tr("Otherwise sing-box drops instead after 50 rejects in 30 s."));
    rejectGrid->addWidget(noDropCheck, 1, 0, 1, 2);
    rejectGrid->addWidget(addSettingIssues({QStringLiteral("method"), QStringLiteral("no_drop")}, rejectGroup), 2, 0, 1, 2);
    rejectGrid->setColumnStretch(1, 1);
    extrasLayout->addWidget(rejectGroup);
    connect(rejectCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (!loading && current != nullptr) setScopedText(QStringLiteral("method"), rejectCombo->currentData().toString());
    });
    connect(noDropCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (!loading && current != nullptr) setScopedFlag(QStringLiteral("no_drop"), on);
    });

    sniffGroup = new QWidget(extras);
    auto* sniffGrid = new QGridLayout(sniffGroup);
    sniffGrid->setContentsMargins(0, 0, 0, 0);
    sniffGrid->setHorizontalSpacing(10);
    sniffGrid->setVerticalSpacing(4);
    sniffOverrideCheck = new QCheckBox(RouteEditor::ConditionLabel(QStringLiteral("override_destination")), sniffGroup);
    sniffGrid->addWidget(sniffOverrideCheck, 0, 0);
    sniffGrid->addWidget(addSettingIssues({QStringLiteral("override_destination")}, sniffGroup), 1, 0);
    sniffGrid->setColumnStretch(0, 1);
    extrasLayout->addWidget(sniffGroup);
    connect(sniffOverrideCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (!loading && current != nullptr) setScopedFlag(QStringLiteral("override_destination"), on);
    });

    resolveGroup = new QWidget(extras);
    auto* resolveGrid = new QGridLayout(resolveGroup);
    resolveGrid->setContentsMargins(0, 0, 0, 0);
    resolveGrid->setHorizontalSpacing(10);
    resolveGrid->setVerticalSpacing(4);
    auto* strategyLabel = new QLabel(RouteEditor::ConditionLabel(QStringLiteral("strategy")), resolveGroup);
    strategyCombo = new NoWheelComboBox(resolveGroup);
    const QHash<QString, QString> strategies = {
        {QStringLiteral("ipv4_only"), tr("IPv4 only")},
        {QStringLiteral("ipv6_only"), tr("IPv6 only")},
        {QStringLiteral("prefer_ipv4"), tr("Prefer IPv4")},
        {QStringLiteral("prefer_ipv6"), tr("Prefer IPv6")},
    };
    for (const QString& value : Configs::RouteRule::get_values_for_field(QStringLiteral("strategy"))) {
        strategyCombo->addItem(value.isEmpty() ? tr("Default") : strategies.value(value, value), value);
    }
    ruleEditorSealCombo(strategyCombo);
    strategyLabel->setBuddy(strategyCombo);
    resolveGrid->addWidget(strategyLabel, 0, 0);
    resolveGrid->addWidget(strategyCombo, 0, 1, Qt::AlignLeft);
    resolveGrid->addWidget(addSettingIssues({QStringLiteral("strategy")}, resolveGroup), 1, 0, 1, 2);
    resolveGrid->setColumnStretch(1, 1);
    extrasLayout->addWidget(resolveGroup);
    connect(strategyCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (!loading && current != nullptr) setScopedText(QStringLiteral("strategy"), strategyCombo->currentData().toString());
    });

    section->addWidget(extras);
    layout->addLayout(section);
    settingWidgets.insert(QStringLiteral("method"), rejectCombo);
    settingWidgets.insert(QStringLiteral("no_drop"), noDropCheck);
    settingWidgets.insert(QStringLiteral("override_destination"), sniffOverrideCheck);
    settingWidgets.insert(QStringLiteral("strategy"), strategyCombo);
}

void RouteRuleEditor::buildOptions(QVBoxLayout* layout, QLayout* toggles) {
    optionsToggle = ruleEditorDisclosure(tr("Connection options"), this);
    optionsToggle->setToolTip(tr("Override, TLS fragment, TLS spoof"));
    toggles->addWidget(optionsToggle);
    connect(optionsToggle, &QToolButton::toggled, this, [this](bool open) { setOptionsOpen(open); });

    optionsFrame = new RuleEditorPanel(this);
    auto* grid = new QGridLayout(optionsFrame);
    grid->setContentsMargins(10, 8, 10, 8);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(4);
    int row = 0;
    const auto finishRow = [this, grid, &row](const QStringList& attrs) {
        grid->addWidget(addSettingIssues(attrs, optionsFrame), row + 1, 0, 1, 2);
        row += 2;
    };
    const auto bindText = [this](QLineEdit* edit, const QString& attr) {
        connect(edit, &QLineEdit::textEdited, this, [this, attr](const QString& text) {
            if (loading || current == nullptr) return;
            current->set_field_value(attr, {text});
            emitChanged();
        });
        connect(edit, &QLineEdit::editingFinished, this, [this] { refreshIssues(); });
        settingWidgets.insert(attr, edit);
    };

    auto* addressLabel = new QLabel(RouteEditor::ConditionLabel(QStringLiteral("override_address")), optionsFrame);
    overrideAddressEdit = new QLineEdit(optionsFrame);
    overrideAddressEdit->setPlaceholderText(tr("Keep the original"));
    addressLabel->setBuddy(overrideAddressEdit);
    grid->addWidget(addressLabel, row, 0);
    grid->addWidget(overrideAddressEdit, row, 1);
    bindText(overrideAddressEdit, QStringLiteral("override_address"));
    finishRow({QStringLiteral("override_address")});

    auto* portLabel = new QLabel(RouteEditor::ConditionLabel(QStringLiteral("override_port")), optionsFrame);
    overridePortEdit = new QLineEdit(optionsFrame);
    overridePortEdit->setPlaceholderText(tr("Keep the original"));
    portLabel->setBuddy(overridePortEdit);
    grid->addWidget(portLabel, row, 0);
    grid->addWidget(overridePortEdit, row, 1);
    bindText(overridePortEdit, QStringLiteral("override_port"));
    finishRow({QStringLiteral("override_port")});

    auto* fragmentLabel = new QLabel(RouteEditor::ConditionLabel(QStringLiteral("tls_fragment")), optionsFrame);
    fragmentCombo = new NoWheelComboBox(optionsFrame);
    fragmentCombo->addItem(tr("Off"), 0);
    fragmentCombo->addItem(tr("Handshake"), ruleEditorFragmentHandshake);
    fragmentCombo->addItem(tr("Records"), ruleEditorFragmentRecords);
    ruleEditorSealCombo(fragmentCombo);
    fragmentLabel->setBuddy(fragmentCombo);
    fragmentDelayEdit = new QLineEdit(optionsFrame);
    fragmentDelayEdit->setPlaceholderText(tr("Fallback delay, such as 500ms"));
    fragmentDelayEdit->setAccessibleName(RouteEditor::ConditionLabel(QStringLiteral("tls_fragment_fallback_delay")));
    auto* fragmentRow = new QHBoxLayout;
    fragmentRow->setSpacing(6);
    fragmentRow->addWidget(fragmentCombo);
    fragmentRow->addWidget(fragmentDelayEdit, 1);
    fragmentRow->addStretch();
    grid->addWidget(fragmentLabel, row, 0);
    grid->addLayout(fragmentRow, row, 1);
    bindText(fragmentDelayEdit, QStringLiteral("tls_fragment_fallback_delay"));
    finishRow({QStringLiteral("tls_fragment"), QStringLiteral("tls_record_fragment"), QStringLiteral("tls_fragment_fallback_delay")});
    connect(fragmentCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (loading || current == nullptr) return;
        const int mode = fragmentCombo->currentData().toInt();
        setFragment((mode & ruleEditorFragmentHandshake) != 0, (mode & ruleEditorFragmentRecords) != 0);
    });

    auto* spoofLabel = new QLabel(RouteEditor::ConditionLabel(QStringLiteral("tls_spoof")), optionsFrame);
    spoofEdit = new QLineEdit(optionsFrame);
    spoofEdit->setPlaceholderText(tr("Server name to send"));
    spoofLabel->setBuddy(spoofEdit);
    spoofMethodCombo = new NoWheelComboBox(optionsFrame);
    spoofMethodCombo->setAccessibleName(RouteEditor::ConditionLabel(QStringLiteral("tls_spoof_method")));
    for (const QString& value : Configs::RouteRule::get_values_for_field(QStringLiteral("tls_spoof_method"))) {
        spoofMethodCombo->addItem(value.isEmpty() ? tr("Default method") : value, value);
    }
    ruleEditorSealCombo(spoofMethodCombo);
    auto* spoofRow = new QHBoxLayout;
    spoofRow->setSpacing(6);
    spoofRow->addWidget(spoofEdit, 1);
    spoofRow->addWidget(spoofMethodCombo);
    grid->addWidget(spoofLabel, row, 0);
    grid->addLayout(spoofRow, row, 1);
    bindText(spoofEdit, QStringLiteral("tls_spoof"));
    finishRow({QStringLiteral("tls_spoof"), QStringLiteral("tls_spoof_method")});
    connect(spoofMethodCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        if (!loading && current != nullptr) setScopedText(QStringLiteral("tls_spoof_method"), spoofMethodCombo->currentData().toString());
    });

    grid->setColumnStretch(1, 1);
    optionsFrame->hide();
    layout->addWidget(optionsFrame);
    settingWidgets.insert(QStringLiteral("tls_fragment"), fragmentCombo);
    settingWidgets.insert(QStringLiteral("tls_record_fragment"), fragmentCombo);
    settingWidgets.insert(QStringLiteral("tls_spoof_method"), spoofMethodCombo);
}

void RouteRuleEditor::buildJson(QVBoxLayout* layout, QLayout* toggles) {
    jsonToggle = ruleEditorDisclosure(tr("sing-box JSON"), this);
    toggles->addWidget(jsonToggle);
    jsonView = new QPlainTextEdit(this);
    jsonView->setReadOnly(true);
    jsonView->setLineWrapMode(QPlainTextEdit::NoWrap);
    jsonView->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    jsonView->setFixedHeight(QFontMetrics(jsonView->font()).lineSpacing() * 12 + 2 * jsonView->frameWidth() + 8);
    jsonView->setAccessibleName(tr("sing-box JSON"));
    jsonView->hide();
    layout->addWidget(jsonView);
    connect(jsonToggle, &QToolButton::toggled, this, [this](bool open) { setJsonOpen(open); });
}

void RouteRuleEditor::setRule(const std::shared_ptr<Configs::RouteRule>& rule) {
    if (rule != nullptr && rule == current) {
        refresh();
        return;
    }
    detachInline(false);
    inlineState = Inline{};
    pending.clear();
    expanded.clear();
    refillAttrs.clear();
    focusInline = false;
    focusAttr.clear();
    focusIndex = -1;
    menuAttr.clear();
    current = rule;
    bool anyOption = false;
    if (current != nullptr) {
        for (const QString& attr : ruleEditorOptionAttrs()) anyOption = anyOption || !Configs::RouteRule::is_attribute_at_default(*current, attr);
    }
    setEnabled(current != nullptr);
    loadRule();
    setOptionsOpen(anyOption);
}

void RouteRuleEditor::focusName() {
    nameEdit->setFocus(Qt::OtherFocusReason);
    nameEdit->selectAll();
}

void RouteRuleEditor::refresh() {
    if (current == nullptr) return;
    loading = true;
    if (nameEdit->text() != current->name) nameEdit->setText(current->name);
    invertCheck->setChecked(current->invert);
    loadThenSection();
    loading = false;
    if (shownAttrs() != builtAttrs) rebuildBoxes();
    else refreshIssues();
    refreshJson();
}

void RouteRuleEditor::loadRule() {
    loading = true;
    nameEdit->setText(current != nullptr ? current->name : QString());
    invertCheck->setChecked(current != nullptr && current->invert);
    thenPicker->setRule(current);
    loadThenSection();
    loading = false;
    rebuildBoxes();
    refreshJson();
}

void RouteRuleEditor::loadThenSection() {
    if (current == nullptr) {
        thenIssues->hide();
        extrasFrame->hide();
        optionsToggle->hide();
        optionsFrame->hide();
        return;
    }
    const RouteEditor::Then then = RouteEditor::ThenOf(*current);
    thenPicker->refresh();
    const bool reject = RouteEditor::ThenUses(then, QStringLiteral("method"));
    const bool sniff = RouteEditor::ThenUses(then, QStringLiteral("override_destination"));
    const bool resolve = RouteEditor::ThenUses(then, QStringLiteral("strategy"));
    rejectGroup->setVisible(reject);
    sniffGroup->setVisible(sniff);
    resolveGroup->setVisible(resolve);
    extrasFrame->setVisible(reject || sniff || resolve);
    const bool options = RouteEditor::ThenUses(then, QStringLiteral("override_address"));
    optionsToggle->setVisible(options);
    optionsFrame->setVisible(options && optionsToggle->isChecked());

    const bool wasLoading = std::exchange(loading, true);
    QString method = current->rejectMethod.trimmed().toLower();
    if (method == QLatin1String("default")) method.clear();
    ruleEditorSelectData(rejectCombo, method);
    noDropCheck->setChecked(current->no_drop);
    sniffOverrideCheck->setChecked(current->sniffOverrideDest);
    ruleEditorSelectData(strategyCombo, current->strategy.trimmed());
    const auto setText = [](QLineEdit* edit, const QString& text) {
        if (edit->text() != text) edit->setText(text);
    };
    setText(overrideAddressEdit, current->override_address);
    setText(overridePortEdit, current->override_port);
    loadFragment();
    setText(spoofEdit, current->tls_spoof);
    ruleEditorSelectData(spoofMethodCombo, current->tls_spoof_method.trimmed());
    loading = wasLoading;
}

void RouteRuleEditor::loadFragment() {
    const bool wasLoading = std::exchange(loading, true);
    const int mode = (current->tls_fragment ? ruleEditorFragmentHandshake : 0) | (current->tls_record_fragment ? ruleEditorFragmentRecords : 0);
    const int both = ruleEditorFragmentHandshake | ruleEditorFragmentRecords;
    const int base = fragmentCombo->property(ruleEditorBaseCountProperty).toInt();
    // route-options takes only one of them, so it offers both only to a rule that already has both.
    const bool offerBoth = mode == both || RouteEditor::ThenOf(*current) != RouteEditor::Then::RouteOptions;
    const bool listed = fragmentCombo->count() > base;
    if (listed && !offerBoth) fragmentCombo->removeItem(base);
    else if (!listed && offerBoth) fragmentCombo->addItem(tr("Handshake + Records"), both);
    fragmentCombo->setCurrentIndex(fragmentCombo->findData(mode));
    if (fragmentDelayEdit->text() != current->tls_fragment_fallback_delay) fragmentDelayEdit->setText(current->tls_fragment_fallback_delay);
    fragmentDelayEdit->setVisible(current->tls_fragment || !current->tls_fragment_fallback_delay.trimmed().isEmpty());
    loading = wasLoading;
}

QStringList RouteRuleEditor::shownAttrs() const {
    if (current == nullptr) return {};
    QStringList attrs = RouteEditor::SetConditions(*current);
    for (const QString& attr : pending) {
        if (!attrs.contains(attr)) attrs << attr;
    }
    return attrs;
}

void RouteRuleEditor::rebuildBoxes() {
    refillAttrs.clear();
    detachInline(true);
    // Later, as this can run from a signal of a chip inside them.
    tree->clearBranches();
    lines.clear();
    builtAttrs = shownAttrs();
    if (inlineState.open && !builtAttrs.contains(inlineState.attr)) inlineState = Inline{};
    // sing-box ORs the attributes of a box; an OR group with one of them is a plain leaf.
    const QList<RouteEditor::Box> boxes = RouteEditor::BoxesFor(builtAttrs);
    const auto isGroup = [](const RouteEditor::Box& box) { return box.grouped && box.attrs.size() > 1; };
    QStringList rootLabels;
    for (const RouteEditor::Box& box : boxes) {
        if (!isGroup(box)) rootLabels << RouteEditor::ShortConditionLabel(box.attrs.first());
    }
    for (const RouteEditor::Box& box : boxes) {
        if (!isGroup(box)) {
            buildLeaf(tree, box.attrs.first(), rootLabels);
            continue;
        }
        auto* group = new RouteEditor::TreeNode(tr("OR"), tree);
        QStringList labels;
        for (const QString& attr : box.attrs) labels << RouteEditor::ShortConditionLabel(attr);
        for (const QString& attr : box.attrs) buildLeaf(group, attr, labels);
        tree->addBranch(group);
    }
    tree->setLinesShown(boxes.size() > 1);
    refreshIssues();
}

void RouteRuleEditor::buildLeaf(RouteEditor::TreeNode* node, const QString& attr, const QStringList& labels) {
    auto* leaf = new RouteEditor::TreeLeaf(node);
    const QString label = RouteEditor::ShortConditionLabel(attr);
    const QString full = RouteEditor::ConditionLabel(attr);
    leaf->setLabel(label, label != full ? full : QString());
    leaf->setLabelColumn(labels);
    leaf->setProperty(ruleEditorLeafProperty, attr);
    if (Configs::RouteRule::get_input_type(attr) == Configs::select) leaf->setProperty(ruleEditorSelectProperty, attr);
    leaf->installEventFilter(this);
    auto* issues = ruleEditorIssuesLabel(node);
    node->addBranch(leaf, issues);
    lines.insert(attr, Line{leaf, leaf->flow(), nullptr, {}, issues});
    fillLine(attr);
}

void RouteRuleEditor::fillLine(const QString& attr) {
    const auto it = lines.find(attr);
    if (it == lines.end() || current == nullptr) return;
    Line& line = it.value();
    if (inlineState.attr == attr) detachInline(true);
    line.flow->clear();
    line.chips.clear();
    line.add = nullptr;
    QWidget* host = line.host;
    const Configs::inputType input = Configs::RouteRule::get_input_type(attr);

    if (input == Configs::trufalse) {
        if (current->get_current_value_bool(attr) == QLatin1String("true")) {
            auto* chip = ruleEditorChip(RouteEditor::ValueText(attr, QStringLiteral("true")), host);
            connect(chip, &RouteEditor::ValueChip::removeRequested, this, [this, attr] { removeValue(attr, {}); });
            line.flow->addWidget(chip);
            line.chips << chip;
        }
        return;
    }

    if (input == Configs::select) {
        const QString value = current->get_current_value_string(attr).value(0).trimmed();
        if (!value.isEmpty()) {
            auto* chip = ruleEditorChip(RouteEditor::ValueText(attr, value), host);
            connect(chip, &RouteEditor::ValueChip::removeRequested, this, [this, attr] { removeValue(attr, {}); });
            connect(chip, &RouteEditor::ValueChip::editRequested, this, [this, attr, chip] { openSelectMenu(attr, chip); });
            line.flow->addWidget(chip);
            line.chips << chip;
        } else {
            auto* add = new RuleEditorAddButton(tr("Add a value"), host);
            connect(add, &QToolButton::clicked, this, [this, attr, add] { openSelectMenu(attr, add); });
            line.flow->addWidget(add);
            line.add = add;
        }
        return;
    }

    const bool editing = inlineState.open && inlineState.attr == attr;
    if (editing && inlineState.asText) {
        line.flow->addWidget(makeTextBox(attr, host));
        auto* done = ruleEditorFlatButton(tr("Done"), host);
        done->setToolTip(tr("%1 applies, Esc reverts").arg(QKeySequence(Qt::CTRL | Qt::Key_Return).toString(QKeySequence::NativeText)));
        connect(done, &QToolButton::clicked, this, [this] { commitInline(false, true); });
        line.flow->addWidget(done);
        return;
    }
    const QStringList values = current->get_current_value_string(attr);
    const bool fold = values.size() > ruleEditorFoldAfter + 2 && !expanded.contains(attr) && !(editing && inlineState.replacing);
    const int shown = fold ? ruleEditorFoldAfter : static_cast<int>(values.size());
    bool placed = false;
    for (int i = 0; i < shown; ++i) {
        const QString& value = values.at(i);
        if (editing && inlineState.replacing && !placed && value == inlineState.original) {
            line.flow->addWidget(makeInlineEdit(attr, host));
            placed = true;
            continue;
        }
        auto* chip = ruleEditorChip(value, host);
        connect(chip, &RouteEditor::ValueChip::removeRequested, this, [this, attr, value] { removeValue(attr, value); });
        connect(chip, &RouteEditor::ValueChip::editRequested, this, [this, attr, value] { startInline(attr, value, true); });
        line.flow->addWidget(chip);
        line.chips << chip;
    }
    if (fold) {
        auto* more = ruleEditorFlatButton(tr("%n more", nullptr, static_cast<int>(values.size()) - shown), host);
        more->setToolTip(tr("Show all values"));
        connect(more, &QToolButton::clicked, this, [this, attr] {
            expanded.insert(attr);
            focusAttr = attr;
            focusIndex = ruleEditorFoldAfter;
            scheduleRefill(attr);
        });
        line.flow->addWidget(more);
    }
    if (editing && !placed) {
        // The value being edited is gone, so the field goes on as a new value.
        inlineState.replacing = false;
        inlineState.original.clear();
        line.flow->addWidget(makeInlineEdit(attr, host));
    } else {
        auto* add = new RuleEditorAddButton(tr("Add a value"), host);
        connect(add, &QToolButton::clicked, this, [this, attr] { startInline(attr, {}, false); });
        line.flow->addWidget(add);
        line.add = add;
    }
    if (ruleEditorIsList(attr)) {
        auto* asText = new RuleEditorTextButton(tr("Edit as text"), host);
        connect(asText, &QToolButton::clicked, this, [this, attr] { startText(attr); });
        line.flow->addWidget(asText);
    }
    if (ruleEditorHasApps(attr)) {
        auto* choose = ruleEditorFlatButton(tr("Choose app"), host);
        choose->setToolTip(tr("Pick a running program, or one that has used the network"));
        connect(choose, &QToolButton::clicked, this, [this, attr] { chooseApps(attr); });
        line.flow->addWidget(choose);
    }
}

QLineEdit* RouteRuleEditor::makeInlineEdit(const QString& attr, QWidget* host) {
    const bool wide = attr == QLatin1String("process_path") || attr == QLatin1String("process_path_regex") || attr == QLatin1String("domain_regex") ||
                      attr == QLatin1String("rule_set");
    auto* edit = new RuleEditorInlineEdit(wide ? 36 : 22, host);
    edit->setText(inlineState.text);
    edit->setPlaceholderText(tr("Type or paste values"));
    edit->setAccessibleName(RouteEditor::ConditionLabel(attr));
    edit->setProperty("invalid", !inlineState.error.isEmpty());
    QAbstractItemModel* model = nullptr;
    if (attr == QLatin1String("rule_set")) {
        model = ruleEditorRuleSetModel();
    } else if (attr == QLatin1String("process_name") || attr == QLatin1String("process_path")) {
        ProcessCompletionModel::Layout layout;
        layout.names = attr == QLatin1String("process_name");
        layout.paths = !layout.names;
        model = new ProcessCompletionModel(layout, edit);
    }
    if (model != nullptr) {
        auto* completer = new QCompleter(model, edit);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        completer->popup()->setTextElideMode(Qt::ElideMiddle);
        edit->setCompleter(completer);
    }
    edit->installEventFilter(this);
    // A paste from the context menu can bring several lines at once.
    connect(edit, &QLineEdit::textChanged, this, [this, edit](const QString& text) {
        if (edit != inlineState.edit.data()) return;
        if (text.contains(QLatin1Char('\n')) || text.contains(QLatin1Char('\r'))) commitInline(true, true);
    });
    inlineState.edit = edit;
    return edit;
}

QPlainTextEdit* RouteRuleEditor::makeTextBox(const QString& attr, QWidget* host) {
    auto* box = new RuleEditorTextBox(host);
    box->setPlainText(inlineState.text);
    box->setAccessibleName(RouteEditor::ConditionLabel(attr));
    box->setProperty("invalid", !inlineState.error.isEmpty());
    ruleEditorMarkLines(box, inlineState.failed);
    connect(box, &QPlainTextEdit::textChanged, box, [box] {
        if (!box->extraSelections().isEmpty()) box->setExtraSelections({});
    });
    box->installEventFilter(this);
    inlineState.box = box;
    return box;
}

QWidget* RouteRuleEditor::inlineWidget() const {
    if (inlineState.box != nullptr) return inlineState.box.data();
    return inlineState.edit.data();
}

QString RouteRuleEditor::inlineText() const {
    if (inlineState.box != nullptr) return inlineState.box->toPlainText();
    if (inlineState.edit != nullptr) return inlineState.edit->text();
    return inlineState.text;
}

void RouteRuleEditor::focusInlineWidget() {
    if (QLineEdit* edit = inlineState.edit.data()) {
        edit->setFocus(Qt::OtherFocusReason);
        edit->end(false);
    } else if (QPlainTextEdit* box = inlineState.box.data()) {
        box->setFocus(Qt::OtherFocusReason);
        const QList<QTextEdit::ExtraSelection> marks = box->extraSelections();
        QTextCursor cursor = marks.isEmpty() ? QTextCursor(box->document()) : marks.first().cursor;
        if (marks.isEmpty()) cursor.movePosition(QTextCursor::End);
        else cursor.clearSelection();
        box->setTextCursor(cursor);
        box->ensureCursorVisible();
    }
}

void RouteRuleEditor::detachInline(bool keepText) {
    QWidget* widget = inlineWidget();
    if (widget == nullptr) return;
    if (keepText) inlineState.text = inlineText();
    widget->removeEventFilter(this);
    inlineState.edit = nullptr;
    inlineState.box = nullptr;
}

void RouteRuleEditor::scheduleRefill(const QString& attr) {
    if (!attr.isEmpty()) refillAttrs.insert(attr);
    if (applyQueued) return;
    applyQueued = true;
    // reveal() may have run it already.
    QTimer::singleShot(0, this, [this] {
        if (applyQueued) applyScheduled();
    });
}

void RouteRuleEditor::applyScheduled() {
    applyQueued = false;
    if (current == nullptr) {
        refillAttrs.clear();
        return;
    }
    if (shownAttrs() != builtAttrs) {
        rebuildBoxes();
    } else {
        const QSet<QString> attrs = std::exchange(refillAttrs, {});
        for (const QString& attr : attrs) fillLine(attr);
        refreshIssues();
    }
    if (!menuAttr.isEmpty()) openSelectMenu(std::exchange(menuAttr, {}), addConditionButton);
    if (inlineState.open && inlineWidget() != nullptr) {
        if (std::exchange(focusInline, false)) focusInlineWidget();
        focusAttr.clear();
    } else if (!focusAttr.isEmpty()) {
        focusLine(std::exchange(focusAttr, {}), std::exchange(focusIndex, -1));
    }
    focusInline = false;
}

// index < 0 prefers the line's "+" button; otherwise the chip there, or the last one.
void RouteRuleEditor::focusLine(const QString& attr, int index) {
    const auto it = lines.constFind(attr);
    if (it == lines.cend()) return;
    QWidget* target = nullptr;
    if (index < 0 && it->add != nullptr) target = it->add;
    else if (!it->chips.isEmpty()) target = it->chips.at(qBound(0, index, static_cast<int>(it->chips.size()) - 1));
    else target = it->add;
    if (target != nullptr) target->setFocus(Qt::OtherFocusReason);
}

bool RouteRuleEditor::closeInline() {
    if (!inlineState.open) return true;
    if (inlineState.asText || !inlineText().trimmed().isEmpty()) {
        commitInline(false, false);
        // Its lines failed: that field stays open and keeps the focus.
        if (inlineState.open) {
            if (inlineWidget() != nullptr) focusInlineWidget();
            else focusInline = true;
            return false;
        }
        return true;
    }
    const QString previous = inlineState.attr;
    detachInline(false);
    inlineState = Inline{};
    scheduleRefill(previous);
    return true;
}

void RouteRuleEditor::startInline(const QString& attr, const QString& value, bool replacing) {
    if (current == nullptr || !closeInline()) return;
    inlineState = Inline{};
    inlineState.open = true;
    inlineState.attr = attr;
    inlineState.replacing = replacing;
    inlineState.original = replacing ? value : QString();
    inlineState.text = replacing ? value : QString();
    focusInline = true;
    scheduleRefill(attr);
}

void RouteRuleEditor::startText(const QString& attr) {
    if (current == nullptr) return;
    if (inlineState.open && inlineState.asText && inlineState.attr == attr) {
        focusInlineWidget();
        return;
    }
    if (!closeInline()) return;
    inlineState = Inline{};
    inlineState.open = true;
    inlineState.attr = attr;
    inlineState.asText = true;
    inlineState.text = current->get_current_value_string(attr).join(QLatin1Char('\n'));
    focusInline = true;
    scheduleRefill(attr);
}

// Each value goes where ShapeAttr sends it, so a pasted list can fill several conditions of one box.
void RouteRuleEditor::commitInline(bool keepOpen, bool focusAfter, const QString& extra) {
    if (!inlineState.open || current == nullptr) return;
    const QString attr = inlineState.attr;
    const bool whole = inlineState.asText;
    QString text = inlineText();
    if (!extra.isEmpty()) text = text.trimmed().isEmpty() ? extra : text + QLatin1Char('\n') + extra;

    QMap<QString, QStringList> lists;
    const auto load = [&](const QString& key) {
        if (!lists.contains(key)) lists.insert(key, current->get_current_value_string(key));
    };
    load(attr);
    if (whole) lists[attr].clear();
    int replaceAt = inlineState.replacing ? static_cast<int>(lists[attr].indexOf(inlineState.original)) : -1;
    int focusAt = replaceAt;
    QStringList failed;
    QString error;
    for (const QString& parsed : RouteEditor::SplitValues(attr, text)) {
        const QString target = RouteEditor::ShapeAttr(attr, parsed);
        const QString value = target == attr ? parsed : RouteEditor::TidyValue(target, parsed);
        const Configs::RuleIssue issue = Configs::RouteRule::check_value(target, value);
        if (!issue.message.isEmpty() && issue.severity == Validation::Severity::Error) {
            failed << value;
            if (error.isEmpty()) error = QStringLiteral("\"%1\": %2").arg(RouteEditor::ElideValue(value), issue.message);
            continue;
        }
        if (replaceAt >= 0) {
            QStringList& own = lists[attr];
            const int duplicate = target == attr ? static_cast<int>(own.indexOf(value)) : -1;
            if (target != attr || (duplicate >= 0 && duplicate != replaceAt)) {
                own.removeAt(replaceAt);
                focusAt = duplicate < 0 ? -1 : duplicate > replaceAt ? duplicate - 1 : duplicate;
            } else {
                own[replaceAt] = value;
                focusAt = replaceAt;
            }
            replaceAt = -1;
            inlineState.replacing = false;
            inlineState.original.clear();
            if (target == attr) continue;
        }
        load(target);
        QStringList& values = lists[target];
        // Duplicates are ignored.
        if (!values.contains(value)) values << value;
    }

    // A text box applies all of it or nothing, and keeps its text and focus while a line fails.
    if (whole && !failed.isEmpty()) {
        inlineState.text = text;
        inlineState.error = error;
        inlineState.failed = failed;
        if (QPlainTextEdit* box = inlineState.box.data()) {
            ruleEditorMarkLines(box, failed);
            if (focusAfter) focusInlineWidget();
        } else {
            focusInline = focusAfter;
            scheduleRefill(attr);
        }
        refreshIssues();
        return;
    }
    detachInline(false);

    QStringList touched;
    for (auto it = lists.cbegin(); it != lists.cend(); ++it) {
        if (it.value() == current->get_current_value_string(it.key())) continue;
        current->set_field_value(it.key(), it.value());
        pending.removeAll(it.key());
        touched << it.key();
    }
    inlineState.text = failed.join(QLatin1Char('\n'));
    inlineState.error = error;
    inlineState.failed.clear();
    if (failed.isEmpty() && (!keepOpen || whole)) inlineState = Inline{};
    else if (failed.isEmpty()) {
        inlineState.replacing = false;
        inlineState.original.clear();
    }
    // Every value went elsewhere or was removed: the empty condition goes with its field.
    const bool gone = !inlineState.open && Configs::RouteRule::is_attribute_at_default(*current, attr);
    if (gone) pending.removeAll(attr);
    focusInline = inlineState.open && focusAfter;
    if (!inlineState.open && focusAfter) {
        const auto other = std::find_if(touched.cbegin(), touched.cend(), [&attr](const QString& key) { return key != attr; });
        focusAttr = !gone ? attr : other != touched.cend() ? *other : QString();
        focusIndex = gone ? -1 : focusAt;
        if (focusAttr.isEmpty()) addConditionButton->setFocus(Qt::OtherFocusReason);
    }
    if (!touched.isEmpty()) emitChanged();
    for (const QString& key : std::as_const(touched)) scheduleRefill(key);
    scheduleRefill(attr);
}

void RouteRuleEditor::cancelInline(bool focusAfter) {
    if (!inlineState.open) return;
    const QString attr = inlineState.attr;
    const QString original = inlineState.replacing ? inlineState.original : QString();
    detachInline(false);
    inlineState = Inline{};
    // A condition just added and still empty goes with its field.
    const bool dropped = current != nullptr && pending.contains(attr) && Configs::RouteRule::is_attribute_at_default(*current, attr);
    if (dropped) pending.removeAll(attr);
    if (focusAfter && current != nullptr) {
        if (dropped) {
            addConditionButton->setFocus(Qt::OtherFocusReason);
        } else {
            focusAttr = attr;
            focusIndex = original.isEmpty() ? -1 : static_cast<int>(current->get_current_value_string(attr).indexOf(original));
        }
    }
    scheduleRefill(attr);
}

void RouteRuleEditor::removeValue(const QString& attr, const QString& value) {
    if (current == nullptr) return;
    int index = -1;
    switch (Configs::RouteRule::get_input_type(attr)) {
        case Configs::trufalse: current->set_field_value(attr, {QStringLiteral("false")}); break;
        case Configs::select: current->set_field_value(attr, {}); break;
        case Configs::text: {
            QStringList values = current->get_current_value_string(attr);
            index = static_cast<int>(values.indexOf(value));
            if (index < 0) return;
            values.removeAt(index);
            current->set_field_value(attr, values);
            break;
        }
    }
    pending.removeAll(attr);
    focusAttr = attr;
    focusIndex = index;
    emitChanged();
    scheduleRefill(attr);
}

void RouteRuleEditor::openSelectMenu(const QString& attr, QWidget* anchor) {
    if (current == nullptr || anchor == nullptr) return;
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    const QString selected = current->get_current_value_string(attr).value(0).trimmed();
    for (const QString& value : Configs::RouteRule::get_values_for_field(attr)) {
        if (value.trimmed().isEmpty()) continue;
        QAction* action = menu->addAction(RouteEditor::ValueText(attr, value));
        action->setCheckable(true);
        action->setChecked(value == selected);
        connect(action, &QAction::triggered, this, [this, attr, value] { setSelectValue(attr, value); });
    }
    menu->popup(anchor->mapToGlobal(QPoint(0, anchor->height())));
}

void RouteRuleEditor::openLeafMenu(const QString& attr, const QPoint& globalPos, const QString& value) {
    if (current == nullptr) return;
    QStringList values;
    for (const QString& item : current->get_current_value_string(attr)) {
        if (!item.trimmed().isEmpty()) values << item;
    }
    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    if (!value.isEmpty()) {
        menu->addAction(tr("Copy \"%1\"").arg(RouteEditor::ElideValue(value)), this, [value] { QGuiApplication::clipboard()->setText(value); });
    }
    QAction* all = menu->addAction(tr("Copy all values"), this, [values] { QGuiApplication::clipboard()->setText(values.join(QLatin1Char('\n'))); });
    all->setEnabled(!values.isEmpty());
    if (ruleEditorIsList(attr)) {
        menu->addSeparator();
        menu->addAction(tr("Edit as text"), this, [this, attr] { startText(attr); });
    }
    menu->popup(globalPos);
}

void RouteRuleEditor::setSelectValue(const QString& attr, const QString& value) {
    if (current == nullptr || current->get_current_value_string(attr).value(0) == value) return;
    current->set_field_value(attr, {value});
    pending.removeAll(attr);
    focusAttr = attr;
    focusIndex = 0;
    emitChanged();
    scheduleRefill(attr);
}

void RouteRuleEditor::addCondition(const QString& attr) {
    if (current == nullptr) return;
    const Configs::inputType input = Configs::RouteRule::get_input_type(attr);
    const bool has = !Configs::RouteRule::is_attribute_at_default(*current, attr);
    if (input == Configs::trufalse) {
        if (!has) {
            current->set_field_value(attr, {QStringLiteral("true")});
            emitChanged();
        }
        focusAttr = attr;
        focusIndex = 0;
        scheduleRefill(attr);
        return;
    }
    if (!has && !pending.contains(attr)) pending << attr;
    if (input == Configs::select) {
        menuAttr = attr;
        focusAttr = attr;
        focusIndex = 0;
        scheduleRefill(attr);
        return;
    }
    startInline(attr, {}, false);
}

void RouteRuleEditor::chooseApps(const QString& attr) {
    // Copied, as the button that asked can be refilled away while the picker runs.
    const QString key = attr;
    const auto rule = current;
    if (rule == nullptr) return;
    const QStringList picked = ProcessPicker::Pick(this, {ruleEditorMatchOf(key)});
    if (picked.isEmpty() || rule != current) return;
    QStringList values = rule->get_current_value_string(key);
    bool added = false;
    for (const QString& value : picked) {
        if (value.trimmed().isEmpty() || values.contains(value)) continue;
        values << value;
        added = true;
    }
    if (!added) return;
    rule->set_field_value(key, values);
    pending.removeAll(key);
    emitChanged();
    scheduleRefill(key);
}

void RouteRuleEditor::onThenPicked() {
    if (current == nullptr) return;
    applyStash();
    loadThenSection();
    refreshIssues();
    emitChanged();
}

// Clears the scoped settings the new action ignores, keeping them for this rule, and puts back the ones it uses again.
void RouteRuleEditor::applyStash() {
    const RouteEditor::Then then = RouteEditor::ThenOf(*current);
    Stash& stash = stashes[current.get()];
    stash.rule = current;
    for (const auto& entry : Configs::RouteRule::attribute_table()) {
        const QString& attr = entry.name;
        if (entry.scope.isEmpty() || attr == QLatin1String("outbound")) continue;
        const bool uses = RouteEditor::ThenUses(then, attr);
        const bool unset = Configs::RouteRule::is_attribute_at_default(*current, attr);
        if (!uses && !unset) {
            stash.values.insert(attr, ruleEditorValueOf(*current, attr));
            current->clear_attribute_value(attr);
        } else if (uses && unset && stash.values.contains(attr)) {
            current->set_field_value(attr, stash.values.take(attr));
        }
    }
}

void RouteRuleEditor::setScopedText(const QString& attr, const QString& text) {
    current->set_field_value(attr, {text});
    refreshIssues();
    emitChanged();
}

void RouteRuleEditor::setScopedFlag(const QString& attr, bool on) {
    current->set_field_value(attr, {on ? QStringLiteral("true") : QStringLiteral("false")});
    refreshIssues();
    emitChanged();
}

// The core reads the fallback delay only with tls_fragment: a choice without the handshake sets it aside until one with it is picked.
void RouteRuleEditor::setFragment(bool handshake, bool records) {
    Stash& stash = stashes[current.get()];
    stash.rule = current;
    QString& delay = current->tls_fragment_fallback_delay;
    if (!handshake && !delay.trimmed().isEmpty()) stash.fragmentDelay = std::exchange(delay, {});
    else if (handshake && delay.trimmed().isEmpty() && !stash.fragmentDelay.isEmpty()) delay = std::exchange(stash.fragmentDelay, {});
    current->tls_fragment = handshake;
    current->tls_record_fragment = records;
    loadFragment();
    refreshIssues();
    emitChanged();
}

void RouteRuleEditor::setOptionsOpen(bool open) {
    const QSignalBlocker blocker(optionsToggle);
    optionsToggle->setChecked(open);
    optionsToggle->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
    // The toggle is hidden for actions without these options.
    optionsFrame->setVisible(open && !optionsToggle->isHidden());
    refreshIssues();
}

void RouteRuleEditor::setJsonOpen(bool open) {
    const QSignalBlocker blocker(jsonToggle);
    jsonToggle->setChecked(open);
    jsonToggle->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
    jsonView->setVisible(open);
    refreshJson();
}

void RouteRuleEditor::refreshIssues() {
    if (current == nullptr) return;
    const QList<Configs::RuleIssue> problems = RouteEditor::RuleProblems(*current);
    QList<bool> placed(problems.size(), false);
    const auto describe = [](const Configs::RuleIssue& issue, bool labelled) {
        const QString text = issue.value.isEmpty() ? issue.message : QStringLiteral("\"%1\": %2").arg(RouteEditor::ElideValue(issue.value), issue.message);
        return labelled ? QCoreApplication::translate("Validation", "%1: %2").arg(RouteEditor::ConditionLabel(issue.attr), text) : text;
    };
    const auto collect = [&](const QStringList& attrs, bool labelled, QStringList* texts) {
        bool error = false;
        for (int i = 0; i < problems.size(); ++i) {
            const Configs::RuleIssue& issue = problems.at(i);
            if (!attrs.contains(issue.attr)) continue;
            placed[i] = true;
            *texts << describe(issue, labelled);
            error = error || issue.severity == Validation::Severity::Error;
        }
        return error;
    };

    // A leaf holds one condition, so the issues under it need no label.
    for (auto it = lines.cbegin(); it != lines.cend(); ++it) {
        QStringList texts;
        bool error = false;
        if (inlineState.open && !inlineState.error.isEmpty() && inlineState.attr == it.key()) {
            texts << inlineState.error;
            error = true;
        }
        error = collect({it.key()}, false, &texts) || error;
        ruleEditorShowIssues(it->issues, texts, error);
    }
    for (const Setting& setting : std::as_const(settings)) {
        QStringList texts;
        // A hidden row's issues go under Then instead.
        const bool error = setting.issues->parentWidget()->isVisibleTo(this) && collect(setting.attrs, setting.attrs.size() > 1, &texts);
        ruleEditorShowIssues(setting.issues, texts, error);
    }
    QStringList rest;
    bool restError = false;
    for (int i = 0; i < problems.size(); ++i) {
        if (placed.at(i)) continue;
        const Configs::RuleIssue& issue = problems.at(i);
        const bool whole = issue.attr.isEmpty() || issue.attr == QLatin1String("action") || issue.attr == QLatin1String("outbound");
        rest << describe(issue, !whole);
        restError = restError || issue.severity == Validation::Severity::Error;
    }
    ruleEditorShowIssues(thenIssues, rest, restError);

    // A turned-off rule never blocks saving, so its values are only flagged.
    for (auto it = lines.cbegin(); it != lines.cend(); ++it) {
        if (!ruleEditorIsList(it.key())) continue;
        for (RouteEditor::ValueChip* chip : it->chips) {
            const Configs::RuleIssue issue = Configs::RouteRule::check_value(it.key(), chip->text());
            chip->setIssue(current->disabled ? Validation::Severity::Warning : issue.severity, issue.message);
        }
    }
    if (QWidget* widget = inlineWidget()) {
        const bool invalid = !inlineState.error.isEmpty();
        if (widget->property("invalid").toBool() != invalid) {
            widget->setProperty("invalid", invalid);
            ruleEditorRestyle(widget);
        }
    }
}

void RouteRuleEditor::refreshJson() {
    if (current == nullptr || jsonView->isHidden()) return;
    const bool routes = current->action == QLatin1String("route") || current->action == QLatin1String("bypass");
    if (routes && current->outboundID >= 0 && !RouteEditor::ProfileExists(current->outboundID)) {
        jsonView->setPlainText(tr("This rule's profile no longer exists. Pick another one to see the JSON."));
        return;
    }
    // get_rule_json rewrites a route to block as a reject, so it runs on a copy.
    Configs::RouteRule copy(*current);
    jsonView->setPlainText(QJsonObject2QString(copy.get_rule_json(true), false));
}

void RouteRuleEditor::emitChanged() {
    if (loading) return;
    refreshJson();
    emit changed();
}

QWidget* RouteRuleEditor::reveal(const QString& attr, const QString& value) {
    if (current == nullptr) return nullptr;
    if (applyQueued) applyScheduled();
    if (attr == QLatin1String("name")) return nameEdit;
    if (attr == QLatin1String("invert")) return invertCheck;
    if (attr.isEmpty() || attr == QLatin1String("action") || attr == QLatin1String("outbound")) return thenPicker;
    if (const auto setting = settingWidgets.constFind(attr); setting != settingWidgets.cend()) {
        if (ruleEditorOptionAttrs().contains(attr)) setOptionsOpen(true);
        return setting.value()->isVisibleTo(this) ? setting.value() : thenPicker;
    }
    if (!lines.contains(attr)) return thenPicker;
    if (inlineState.open && inlineState.attr == attr && inlineWidget() != nullptr) return inlineWidget();
    if (!value.isEmpty() && !expanded.contains(attr) && current->get_current_value_string(attr).indexOf(value) >= ruleEditorFoldAfter) {
        expanded.insert(attr);
        fillLine(attr);
        refreshIssues();
    }
    const auto line = lines.constFind(attr);
    if (!value.isEmpty()) {
        for (RouteEditor::ValueChip* chip : line->chips) {
            if (chip->text() == value) return chip;
        }
    }
    if (!line->chips.isEmpty()) return line->chips.first();
    return line->add != nullptr ? line->add : line->host;
}

Configs::RuleIssue RouteRuleEditor::pendingIssue() const {
    Configs::RuleIssue issue;
    if (current == nullptr || !inlineState.open || inlineState.error.isEmpty()) return issue;
    issue.severity = current->disabled ? Validation::Severity::Warning : Validation::Severity::Error;
    issue.attr = inlineState.attr;
    issue.message = inlineState.error;
    return issue;
}

bool RouteRuleEditor::eventFilter(QObject* watched, QEvent* event) {
    if (inlineState.edit != nullptr && watched == inlineState.edit.data()) {
        QLineEdit* edit = inlineState.edit.data();
        if (event->type() == QEvent::KeyPress) {
            auto* key = static_cast<QKeyEvent*>(event);
            const bool popupOpen = edit->completer() != nullptr && edit->completer()->popup() != nullptr && edit->completer()->popup()->isVisible();
            if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && !popupOpen) {
                // Accepted here, or the dialog would take Enter for its default button.
                if (!inlineState.replacing && edit->text().trimmed().isEmpty()) cancelInline(true);
                else commitInline(!inlineState.replacing, true);
                return true;
            }
            if (key->key() == Qt::Key_Escape && !popupOpen) {
                cancelInline(true);
                return true;
            }
            if (key->matches(QKeySequence::Paste)) {
                const QString clip = QGuiApplication::clipboard()->text();
                if (clip.contains(QLatin1Char('\n')) || clip.contains(QLatin1Char('\r'))) {
                    commitInline(true, true, clip);
                    return true;
                }
            }
        } else if (event->type() == QEvent::FocusOut) {
            const Qt::FocusReason reason = static_cast<QFocusEvent*>(event)->reason();
            if (reason != Qt::PopupFocusReason && reason != Qt::ActiveWindowFocusReason) {
                if (edit->text().trimmed().isEmpty()) cancelInline(false);
                else commitInline(false, false);
            }
        }
        return QWidget::eventFilter(watched, event);
    }
    if (inlineState.box != nullptr && watched == inlineState.box.data()) {
        if (event->type() == QEvent::KeyPress) {
            const auto* key = static_cast<QKeyEvent*>(event);
            if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && key->modifiers().testFlag(Qt::ControlModifier)) {
                commitInline(false, true);
                return true;
            }
            if (key->key() == Qt::Key_Escape) {
                cancelInline(true);
                return true;
            }
        } else if (event->type() == QEvent::FocusOut) {
            const Qt::FocusReason reason = static_cast<QFocusEvent*>(event)->reason();
            if (reason != Qt::PopupFocusReason && reason != Qt::ActiveWindowFocusReason) commitInline(false, false);
        }
        return QWidget::eventFilter(watched, event);
    }
    if (event->type() == QEvent::ContextMenu) {
        auto* host = qobject_cast<QWidget*>(watched);
        const QString attr = host != nullptr ? host->property(ruleEditorLeafProperty).toString() : QString();
        if (!attr.isEmpty() && Configs::RouteRule::get_input_type(attr) == Configs::text) {
            const QPoint global = static_cast<QContextMenuEvent*>(event)->globalPos();
            const auto* chip = qobject_cast<RouteEditor::ValueChip*>(host->childAt(host->mapFromGlobal(global)));
            openLeafMenu(attr, global, chip != nullptr ? chip->text() : QString());
            return true;
        }
    }
    if (event->type() == QEvent::MouseButtonPress) {
        auto* host = qobject_cast<QWidget*>(watched);
        const QString attr = host != nullptr ? host->property(ruleEditorSelectProperty).toString() : QString();
        const auto* press = static_cast<QMouseEvent*>(event);
        // A press on a select's chip outside its remove button reaches its line: it opens the value menu.
        if (!attr.isEmpty() && press->button() == Qt::LeftButton) {
            if (auto* chip = qobject_cast<RouteEditor::ValueChip*>(host->childAt(press->position().toPoint()))) {
                openSelectMenu(attr, chip);
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}
