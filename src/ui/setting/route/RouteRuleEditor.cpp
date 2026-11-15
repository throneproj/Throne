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
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QStringListModel>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <utility>

namespace {
    constexpr char ruleEditorSelectProperty[] = "routeRuleEditorSelect";
    constexpr char ruleEditorBaseCountProperty[] = "routeRuleEditorBaseCount";

    const QStringList& ruleEditorOptionAttrs() {
        static const QStringList attrs = {"override_address", "override_port", "tls_fragment", "tls_fragment_fallback_delay",
                                          "tls_record_fragment", "tls_spoof", "tls_spoof_method"};
        return attrs;
    }

    QFont ruleEditorFont(const QFont& base, qreal scale, bool bold) {
        QFont font = base;
        if (scale != 1.0) {
            if (font.pointSizeF() > 0) font.setPointSizeF(font.pointSizeF() * scale);
            else if (font.pixelSize() > 0) font.setPixelSize(qMax(8, qRound(font.pixelSize() * scale)));
        }
        if (bold) font.setWeight(QFont::DemiBold);
        return font;
    }

    QLabel* ruleEditorLabel(const QString& text, QWidget* parent, const char* role = nullptr, qreal scale = 1.0, bool bold = false, bool wrap = false) {
        auto* label = new QLabel(text, parent);
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(wrap);
        if (role != nullptr) label->setProperty("colorRole", QString::fromLatin1(role));
        if (scale != 1.0 || bold) label->setFont(ruleEditorFont(label->font(), scale, bold));
        return label;
    }

    QLabel* ruleEditorHeading(const QString& text, QWidget* parent) {
        auto* label = ruleEditorLabel(text, parent, "muted", 0.85, true);
        QFont font = label->font();
        font.setCapitalization(QFont::AllUppercase);
        label->setFont(font);
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

    // A rounded frame around a box or a settings group, with an optional rule under its header.
    class RuleEditorPanel : public QWidget {
    public:
        RuleEditorPanel(QWidget* parent, bool filled) : QWidget(parent), fill(filled) {}

        void setDivider(QWidget* widget) {
            divider = widget;
            update();
        }

    protected:
        void paintEvent(QPaintEvent*) override {
            const auto& tokens = themeManager()->tokens;
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing, true);
            const QColor line = RouteEditor::Mix(tokens.onSurface, tokens.surface, 0.16);
            painter.setPen(QPen(line, 1));
            painter.setBrush(fill ? QBrush(RouteEditor::Mix(tokens.onSurface, tokens.surface, 0.025)) : QBrush(Qt::NoBrush));
            painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 7, 7);
            if (divider != nullptr && divider->isVisible()) {
                const qreal y = divider->geometry().bottom() + 4.5;
                painter.drawLine(QPointF(1, y), QPointF(width() - 1, y));
            }
        }

    private:
        QPointer<QWidget> divider;
        bool fill;
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
} // namespace

RouteRuleEditor::RouteRuleEditor(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* nameRow = new QHBoxLayout;
    nameRow->setSpacing(8);
    auto* nameLabel = ruleEditorLabel(tr("Name"), this, "muted");
    nameEdit = new QLineEdit(this);
    nameLabel->setBuddy(nameEdit);
    nameRow->addWidget(nameLabel);
    nameRow->addWidget(nameEdit, 1);
    layout->addLayout(nameRow);
    connect(nameEdit, &QLineEdit::textEdited, this, [this](const QString& text) {
        if (loading || current == nullptr) return;
        current->name = text;
        emitChanged();
    });

    layout->addSpacing(6);
    layout->addWidget(ruleEditorHeading(tr("When"), this));
    boxesHost = new QWidget(this);
    boxesLayout = new QVBoxLayout(boxesHost);
    boxesLayout->setContentsMargins(0, 0, 0, 0);
    boxesLayout->setSpacing(8);
    layout->addWidget(boxesHost);

    addConditionButton = new QPushButton(tr("+ Add condition"), this);
    addConditionButton->setAutoDefault(false);
    auto* conditionMenu = new QMenu(addConditionButton);
    for (const auto& group : RouteEditor::ConditionMenu()) {
        conditionMenu->addSection(group.title);
        for (const QString& attr : group.attrs) {
            connect(conditionMenu->addAction(RouteEditor::ConditionLabel(attr)), &QAction::triggered, this, [this, attr] { addCondition(attr); });
        }
    }
    addConditionButton->setMenu(conditionMenu);
    invertCheck = new QCheckBox(RouteEditor::ConditionLabel(QStringLiteral("invert")), this);
    invertCheck->setToolTip(tr("Invert: match only what this rule would not match"));
    auto* addRow = new QHBoxLayout;
    addRow->addWidget(addConditionButton);
    addRow->addStretch(1);
    addRow->addWidget(invertCheck);
    layout->addLayout(addRow);
    connect(invertCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (loading || current == nullptr) return;
        current->invert = on;
        refreshIssues();
        emitChanged();
    });

    layout->addSpacing(6);
    buildThenSection(layout);
    buildOptions(layout);
    buildJson(layout);
    layout->addStretch(1);

    // Warnings carry their colour in a style sheet of their own.
    connect(themeManager(), &ThemeManager::themeChanged, this, [this] { refreshIssues(); });
    setEnabled(false);
}

RouteRuleEditor::~RouteRuleEditor() = default;

QLabel* RouteRuleEditor::addSettingIssues(const QStringList& attrs, QWidget* parent) {
    auto* label = ruleEditorLabel({}, parent, nullptr, 0.95, false, true);
    label->hide();
    settings.append(Setting{attrs, label});
    return label;
}

void RouteRuleEditor::buildThenSection(QVBoxLayout* layout) {
    layout->addWidget(ruleEditorHeading(tr("Then"), this));
    thenPicker = new RouteThenPicker(this);
    auto* pickerRow = new QHBoxLayout;
    pickerRow->addWidget(thenPicker);
    pickerRow->addStretch(1);
    layout->addLayout(pickerRow);
    connect(thenPicker, &RouteThenPicker::picked, this, &RouteRuleEditor::onThenPicked);
    thenIssues = ruleEditorLabel({}, this, nullptr, 0.95, false, true);
    thenIssues->hide();
    layout->addWidget(thenIssues);

    auto* extras = new RuleEditorPanel(this, false);
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

    layout->addWidget(extras);
    settingWidgets.insert(QStringLiteral("method"), rejectCombo);
    settingWidgets.insert(QStringLiteral("no_drop"), noDropCheck);
    settingWidgets.insert(QStringLiteral("override_destination"), sniffOverrideCheck);
    settingWidgets.insert(QStringLiteral("strategy"), strategyCombo);
}

void RouteRuleEditor::buildOptions(QVBoxLayout* layout) {
    auto* frame = new RuleEditorPanel(this, false);
    optionsFrame = frame;
    auto* frameLayout = new QVBoxLayout(frame);
    frameLayout->setContentsMargins(4, 2, 10, 8);
    frameLayout->setSpacing(6);
    auto* head = new QHBoxLayout;
    head->setSpacing(8);
    optionsToggle = ruleEditorDisclosure(tr("Connection options"), frame);
    optionsToggle->setToolTip(tr("Override, TLS fragment, TLS spoof"));
    head->addWidget(optionsToggle);
    head->addStretch(1);
    frameLayout->addLayout(head);
    connect(optionsToggle, &QToolButton::toggled, this, [this](bool open) { setOptionsOpen(open); });

    optionsBody = new QWidget(frame);
    auto* grid = new QGridLayout(optionsBody);
    grid->setContentsMargins(6, 0, 0, 0);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(4);
    int row = 0;
    const auto finishRow = [this, grid, &row](const QStringList& attrs) {
        grid->addWidget(addSettingIssues(attrs, optionsBody), row + 1, 0, 1, 2);
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

    auto* addressLabel = new QLabel(RouteEditor::ConditionLabel(QStringLiteral("override_address")), optionsBody);
    overrideAddressEdit = new QLineEdit(optionsBody);
    overrideAddressEdit->setPlaceholderText(tr("Keep the original"));
    addressLabel->setBuddy(overrideAddressEdit);
    grid->addWidget(addressLabel, row, 0);
    grid->addWidget(overrideAddressEdit, row, 1);
    bindText(overrideAddressEdit, QStringLiteral("override_address"));
    finishRow({QStringLiteral("override_address")});

    auto* portLabel = new QLabel(RouteEditor::ConditionLabel(QStringLiteral("override_port")), optionsBody);
    overridePortEdit = new QLineEdit(optionsBody);
    overridePortEdit->setPlaceholderText(tr("Keep the original"));
    portLabel->setBuddy(overridePortEdit);
    grid->addWidget(portLabel, row, 0);
    grid->addWidget(overridePortEdit, row, 1);
    bindText(overridePortEdit, QStringLiteral("override_port"));
    finishRow({QStringLiteral("override_port")});

    fragmentCheck = new QCheckBox(RouteEditor::ConditionLabel(QStringLiteral("tls_fragment")), optionsBody);
    fragmentDelayEdit = new QLineEdit(optionsBody);
    fragmentDelayEdit->setPlaceholderText(tr("Fallback delay, such as 500ms"));
    fragmentDelayEdit->setAccessibleName(RouteEditor::ConditionLabel(QStringLiteral("tls_fragment_fallback_delay")));
    grid->addWidget(fragmentCheck, row, 0);
    grid->addWidget(fragmentDelayEdit, row, 1);
    bindText(fragmentDelayEdit, QStringLiteral("tls_fragment_fallback_delay"));
    finishRow({QStringLiteral("tls_fragment"), QStringLiteral("tls_fragment_fallback_delay")});
    connect(fragmentCheck, &QCheckBox::toggled, this, [this](bool on) {
        fragmentDelayEdit->setEnabled(on);
        if (!loading && current != nullptr) setScopedFlag(QStringLiteral("tls_fragment"), on);
    });

    recordFragmentCheck = new QCheckBox(RouteEditor::ConditionLabel(QStringLiteral("tls_record_fragment")), optionsBody);
    grid->addWidget(recordFragmentCheck, row, 0, 1, 2);
    finishRow({QStringLiteral("tls_record_fragment")});
    connect(recordFragmentCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (!loading && current != nullptr) setScopedFlag(QStringLiteral("tls_record_fragment"), on);
    });

    auto* spoofLabel = new QLabel(RouteEditor::ConditionLabel(QStringLiteral("tls_spoof")), optionsBody);
    spoofEdit = new QLineEdit(optionsBody);
    spoofEdit->setPlaceholderText(tr("Server name to send"));
    spoofLabel->setBuddy(spoofEdit);
    spoofMethodCombo = new NoWheelComboBox(optionsBody);
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
    frameLayout->addWidget(optionsBody);
    optionsBody->hide();
    layout->addWidget(frame);
    settingWidgets.insert(QStringLiteral("tls_fragment"), fragmentCheck);
    settingWidgets.insert(QStringLiteral("tls_record_fragment"), recordFragmentCheck);
    settingWidgets.insert(QStringLiteral("tls_spoof_method"), spoofMethodCombo);
}

void RouteRuleEditor::buildJson(QVBoxLayout* layout) {
    auto* head = new QHBoxLayout;
    head->setSpacing(8);
    jsonToggle = ruleEditorDisclosure(tr("sing-box JSON"), this);
    head->addWidget(jsonToggle);
    head->addStretch(1);
    layout->addSpacing(4);
    layout->addLayout(head);
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
    optionsFrame->setVisible(RouteEditor::ThenUses(then, QStringLiteral("override_address")));

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
    fragmentCheck->setChecked(current->tls_fragment);
    setText(fragmentDelayEdit, current->tls_fragment_fallback_delay);
    fragmentDelayEdit->setEnabled(current->tls_fragment);
    recordFragmentCheck->setChecked(current->tls_record_fragment);
    setText(spoofEdit, current->tls_spoof);
    ruleEditorSelectData(spoofMethodCombo, current->tls_spoof_method.trimmed());
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
    for (const BoxView& box : std::as_const(boxViews)) {
        box.frame->hide();
        box.frame->deleteLater();
    }
    boxViews.clear();
    lines.clear();
    builtAttrs = shownAttrs();
    if (inlineState.open && !builtAttrs.contains(inlineState.attr)) inlineState = Inline{};
    for (const RouteEditor::Box& box : RouteEditor::BoxesFor(builtAttrs)) buildBox(box.title, box.attrs, box.grouped);
    refreshIssues();
}

void RouteRuleEditor::buildBox(const QString& title, const QStringList& attrs, bool grouped) {
    auto* frame = new RuleEditorPanel(boxesHost, true);
    auto* grid = new QGridLayout(frame);
    grid->setContentsMargins(10, 7, 10, 8);
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(8);
    auto* header = new QWidget(frame);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(10);
    headerLayout->addWidget(ruleEditorLabel(title, header, nullptr, 1.0, true));
    headerLayout->addStretch(1);
    headerLayout->addWidget(ruleEditorLabel(grouped ? tr("Any of these") : tr("Any of these values"), header, "muted", 0.92));
    grid->addWidget(header, 0, 0, 1, 2);
    frame->setDivider(header);

    int row = 1;
    for (const QString& attr : attrs) {
        if (grouped) {
            auto* label = ruleEditorLabel(RouteEditor::ConditionLabel(attr), frame);
            label->setContentsMargins(0, 3, 0, 0);
            grid->addWidget(label, row, 0, Qt::AlignLeft | Qt::AlignTop);
        }
        auto* host = new QWidget(frame);
        auto* flow = new RouteEditor::FlowLayout(host, 5, 5);
        grid->addWidget(host, row, grouped ? 1 : 0, 1, grouped ? 1 : 2);
        if (Configs::RouteRule::get_input_type(attr) == Configs::select) {
            host->setProperty(ruleEditorSelectProperty, attr);
            host->installEventFilter(this);
        }
        lines.insert(attr, Line{host, flow, nullptr, {}});
        fillLine(attr);
        ++row;
    }
    grid->setColumnStretch(1, 1);
    auto* issues = ruleEditorLabel({}, frame, nullptr, 0.95, false, true);
    issues->hide();
    grid->addWidget(issues, row, 0, 1, 2);
    boxesLayout->addWidget(frame);
    boxViews.append(BoxView{attrs, grouped, frame, issues});
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
            auto* chip = new RouteEditor::ValueChip(RouteEditor::ValueText(attr, QStringLiteral("true")), host);
            connect(chip, &RouteEditor::ValueChip::removeRequested, this, [this, attr] { removeValue(attr, {}); });
            line.flow->addWidget(chip);
            line.chips << chip;
        }
        return;
    }

    if (input == Configs::select) {
        const QString value = current->get_current_value_string(attr).value(0).trimmed();
        if (!value.isEmpty()) {
            auto* chip = new RouteEditor::ValueChip(RouteEditor::ValueText(attr, value), host);
            connect(chip, &RouteEditor::ValueChip::removeRequested, this, [this, attr] { removeValue(attr, {}); });
            connect(chip, &RouteEditor::ValueChip::editRequested, this, [this, attr, chip] { openSelectMenu(attr, chip); });
            line.flow->addWidget(chip);
            line.chips << chip;
        } else {
            auto* add = ruleEditorFlatButton(tr("+ Add"), host);
            connect(add, &QToolButton::clicked, this, [this, attr, add] { openSelectMenu(attr, add); });
            line.flow->addWidget(add);
            line.add = add;
        }
        return;
    }

    const bool editing = inlineState.open && inlineState.attr == attr;
    bool placed = false;
    for (const QString& value : current->get_current_value_string(attr)) {
        if (editing && inlineState.replacing && !placed && value == inlineState.original) {
            line.flow->addWidget(makeInlineEdit(attr, host));
            placed = true;
            continue;
        }
        auto* chip = new RouteEditor::ValueChip(value, host);
        connect(chip, &RouteEditor::ValueChip::removeRequested, this, [this, attr, value] { removeValue(attr, value); });
        connect(chip, &RouteEditor::ValueChip::editRequested, this, [this, attr, value] { startInline(attr, value, true); });
        line.flow->addWidget(chip);
        line.chips << chip;
    }
    if (editing && !placed) {
        // The value being edited is gone, so the field goes on as a new value.
        inlineState.replacing = false;
        inlineState.original.clear();
        line.flow->addWidget(makeInlineEdit(attr, host));
    } else {
        auto* add = ruleEditorFlatButton(tr("+ Add"), host);
        connect(add, &QToolButton::clicked, this, [this, attr] { startInline(attr, {}, false); });
        line.flow->addWidget(add);
        line.add = add;
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
    edit->setPlaceholderText(RouteEditor::ConditionLabel(attr));
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

void RouteRuleEditor::detachInline(bool keepText) {
    if (inlineState.edit == nullptr) return;
    if (keepText) inlineState.text = inlineState.edit->text();
    inlineState.edit->removeEventFilter(this);
    inlineState.edit = nullptr;
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
    if (inlineState.open && inlineState.edit != nullptr) {
        if (std::exchange(focusInline, false)) {
            inlineState.edit->setFocus(Qt::OtherFocusReason);
            inlineState.edit->end(false);
        }
        focusAttr.clear();
    } else if (!focusAttr.isEmpty()) {
        focusLine(std::exchange(focusAttr, {}), std::exchange(focusIndex, -1));
    }
    focusInline = false;
}

// index < 0 prefers the line's "+ Add" button; otherwise the chip there, or the last one.
void RouteRuleEditor::focusLine(const QString& attr, int index) {
    const auto it = lines.constFind(attr);
    if (it == lines.cend()) return;
    QWidget* target = nullptr;
    if (index < 0 && it->add != nullptr) target = it->add;
    else if (!it->chips.isEmpty()) target = it->chips.at(qBound(0, index, static_cast<int>(it->chips.size()) - 1));
    else target = it->add;
    if (target != nullptr) target->setFocus(Qt::OtherFocusReason);
}

void RouteRuleEditor::startInline(const QString& attr, const QString& value, bool replacing) {
    if (current == nullptr) return;
    if (inlineState.open) {
        const QString typed = inlineState.edit != nullptr ? inlineState.edit->text() : inlineState.text;
        if (!typed.trimmed().isEmpty()) {
            commitInline(false, false);
            // Its lines failed: that field stays open and keeps the focus.
            if (inlineState.open) {
                focusInline = true;
                return;
            }
        } else {
            const QString previous = inlineState.attr;
            detachInline(false);
            inlineState = Inline{};
            scheduleRefill(previous);
        }
    }
    inlineState = Inline{};
    inlineState.open = true;
    inlineState.attr = attr;
    inlineState.replacing = replacing;
    inlineState.original = replacing ? value : QString();
    inlineState.text = replacing ? value : QString();
    focusInline = true;
    scheduleRefill(attr);
}

void RouteRuleEditor::commitInline(bool keepOpen, bool focusAfter, const QString& extra) {
    if (!inlineState.open || current == nullptr) return;
    const QString attr = inlineState.attr;
    QString text = inlineState.edit != nullptr ? inlineState.edit->text() : inlineState.text;
    detachInline(false);
    if (!extra.isEmpty()) text = text.trimmed().isEmpty() ? extra : text + QLatin1Char('\n') + extra;
    static const QRegularExpression breaks(QStringLiteral("[\\r\\n\\x{2028}\\x{2029}]"));

    QStringList values = current->get_current_value_string(attr);
    int replaceAt = inlineState.replacing ? static_cast<int>(values.indexOf(inlineState.original)) : -1;
    int focusAt = replaceAt;
    QStringList failed;
    QString error;
    bool edited = false;
    for (const QString& raw : text.split(breaks, Qt::SkipEmptyParts)) {
        const QString value = raw.trimmed();
        if (value.isEmpty()) continue;
        const Configs::RuleIssue issue = Configs::RouteRule::check_value(attr, value);
        if (!issue.message.isEmpty() && issue.severity == Validation::Severity::Error) {
            failed << value;
            if (error.isEmpty()) error = QStringLiteral("\"%1\": %2").arg(RouteEditor::ElideValue(value), issue.message);
            continue;
        }
        if (replaceAt >= 0) {
            if (values.at(replaceAt) != value) {
                const int duplicate = static_cast<int>(values.indexOf(value));
                if (duplicate >= 0) {
                    values.removeAt(replaceAt);
                    focusAt = duplicate > replaceAt ? duplicate - 1 : duplicate;
                } else {
                    values[replaceAt] = value;
                    focusAt = replaceAt;
                }
                edited = true;
            } else {
                focusAt = replaceAt;
            }
            replaceAt = -1;
            inlineState.replacing = false;
            inlineState.original.clear();
            continue;
        }
        // Duplicates are ignored.
        if (values.contains(value)) continue;
        values << value;
        edited = true;
    }
    if (edited) {
        current->set_field_value(attr, values);
        pending.removeAll(attr);
    }
    inlineState.text = failed.join(QLatin1Char('\n'));
    inlineState.error = error;
    if (failed.isEmpty() && !keepOpen) inlineState = Inline{};
    else if (failed.isEmpty()) {
        inlineState.replacing = false;
        inlineState.original.clear();
    }
    focusInline = inlineState.open && focusAfter;
    if (!inlineState.open && focusAfter) {
        focusAttr = attr;
        focusIndex = focusAt;
    }
    if (edited) emitChanged();
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

void RouteRuleEditor::setOptionsOpen(bool open) {
    const QSignalBlocker blocker(optionsToggle);
    optionsToggle->setChecked(open);
    optionsToggle->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
    optionsBody->setVisible(open);
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

    for (const BoxView& box : std::as_const(boxViews)) {
        QStringList texts;
        bool error = false;
        if (inlineState.open && !inlineState.error.isEmpty() && box.attrs.contains(inlineState.attr)) {
            texts << (box.grouped ? QCoreApplication::translate("Validation", "%1: %2").arg(RouteEditor::ConditionLabel(inlineState.attr), inlineState.error)
                                  : inlineState.error);
            error = true;
        }
        error = collect(box.attrs, box.grouped, &texts) || error;
        ruleEditorShowIssues(box.issues, texts, error);
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
    if (inlineState.edit != nullptr) {
        const bool invalid = !inlineState.error.isEmpty();
        if (inlineState.edit->property("invalid").toBool() != invalid) {
            inlineState.edit->setProperty("invalid", invalid);
            ruleEditorRestyle(inlineState.edit);
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
    const auto line = lines.constFind(attr);
    if (line == lines.cend()) return thenPicker;
    if (inlineState.open && inlineState.attr == attr && inlineState.edit != nullptr) return inlineState.edit;
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
