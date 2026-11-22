#include "include/ui/setting/route/RouteQuickLists.h"

#include "include/ui/setting/ProcessPicker.h"
#include "include/ui/setting/ThemeManager.hpp"
#include "include/ui/setting/route/RouteEditorWidgets.h"

#include <srslist.h>

#include <QAbstractListModel>
#include <QApplication>
#include <QCompleter>
#include <QCoreApplication>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QHostAddress>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <functional>
#include <string_view>
#include <vector>

namespace {
    struct QuickAttr {
        const char* prefix;
        const char* attr;
        QList<QString> Configs::RouteRule::* member;
    };

    const std::array<QuickAttr, 8>& quickAttrs() {
        static const std::array<QuickAttr, 8> attrs = {{
            {"domain", "domain", &Configs::RouteRule::domain},
            {"suffix", "domain_suffix", &Configs::RouteRule::domain_suffix},
            {"keyword", "domain_keyword", &Configs::RouteRule::domain_keyword},
            {"regex", "domain_regex", &Configs::RouteRule::domain_regex},
            {"ruleset", "rule_set", &Configs::RouteRule::rule_set},
            {"ip", "ip_cidr", &Configs::RouteRule::ip_cidr},
            {"processName", "process_name", &Configs::RouteRule::process_name},
            {"processPath", "process_path", &Configs::RouteRule::process_path},
        }};
        return attrs;
    }

    const QuickAttr* quickAttr(const QString& prefix) {
        for (const auto& attr : quickAttrs()) {
            if (prefix == QLatin1String(attr.prefix)) return &attr;
        }
        return nullptr;
    }

    bool storedLowercase(const QString& prefix) {
        return prefix == QLatin1String("domain") || prefix == QLatin1String("suffix") || prefix == QLatin1String("keyword");
    }

    RouteEditor::EntryKind kindOfPrefix(const QString& prefix) {
        if (prefix == QLatin1String("processName")) return RouteEditor::EntryKind::ProcessName;
        if (prefix == QLatin1String("processPath")) return RouteEditor::EntryKind::ProcessPath;
        return RouteEditor::EntryKind::Address;
    }

    bool inList(const std::shared_ptr<Configs::RouteRule>& rule, Configs::simpleAction list) {
        return rule != nullptr && RouteEditor::IsQuickListType(rule->type) && RouteEditor::ListOfType(rule->type) == list;
    }

    void appendEntries(QList<QuickLists::Entry>& entries, const std::shared_ptr<Configs::RouteRule>& rule) {
        if (rule == nullptr || !RouteEditor::IsQuickListType(rule->type)) return;
        for (const auto& attr : quickAttrs()) {
            const QString prefix = QString::fromLatin1(attr.prefix);
            for (const QString& value : std::as_const((*rule).*(attr.member))) {
                entries.append({prefix, value, rule, QString::fromLatin1(attr.attr),
                                Configs::RouteProfile::CheckSimpleRule(prefix + QLatin1Char(':') + value)});
            }
        }
    }

    bool knownRuleSet(const QString& name) {
        const QByteArray utf8 = name.toUtf8();
        const std::string_view key(utf8.constData(), static_cast<size_t>(utf8.size()));
        const auto it = std::lower_bound(ruleSetList.begin(), ruleSetList.end(), key,
                                         [](const auto& entry, std::string_view k) { return entry.first < k; });
        return it != ruleSetList.end() && it->first == key;
    }

    bool ruleSetUrl(const QString& text) {
        if (!text.startsWith(QLatin1String("http://"), Qt::CaseInsensitive) && !text.startsWith(QLatin1String("https://"), Qt::CaseInsensitive))
            return false;
        const QUrl url(text, QUrl::StrictMode);
        return url.isValid() && !url.host().isEmpty() && url.path().endsWith(QLatin1String(".srs"), Qt::CaseInsensitive);
    }

    bool ipShape(const QString& text) {
        const QString address = text.section(QLatin1Char('/'), 0, 0);
        // QHostAddress also reads shorthands such as 10.1, which the core does not.
        if (!address.contains(QLatin1Char(':')) && address.count(QLatin1Char('.')) != 3) return false;
        QHostAddress host;
        return host.setAddress(address);
    }

    bool pathShape(const QString& text) {
        if (text.contains(QLatin1Char('\\')) || text.contains(QLatin1Char('/'))) return true;
        return text.size() >= 2 && text.at(1) == QLatin1Char(':') && text.at(0).isLetter();
    }

    // The domain suffix a host name stands for, lowercased; empty when text is not one.
    QString hostSuffix(const QString& text) {
        const QString host = text.startsWith(QLatin1String("*.")) ? text.mid(1) : text;
        if (!host.contains(QLatin1Char('.'))) return {};
        bool named = false;
        for (const QChar c : host) {
            if (c.isLetterOrNumber()) named = true;
            else if (c != QLatin1Char('-') && c != QLatin1Char('_') && c != QLatin1Char('.')) return {};
        }
        return named ? host.toLower() : QString();
    }

    QString uniqueRuleName(const Configs::RouteProfile& profile, const QString& base) {
        QSet<QString> taken;
        for (const auto& rule : profile.Rules) taken.insert(rule->name);
        QString name = base;
        for (int n = 2; taken.contains(name); ++n) name = base + QLatin1Char(' ') + QString::number(n);
        return name;
    }

    // scope: the indexes of the rules a value may join; a new rule goes in at insertAt.
    bool insertLine(Configs::RouteProfile& profile, Configs::simpleAction list, const QList<int>& scope, int insertAt, const QString& line,
                    QString* error) {
        QString problem;
        const QString canonical = QuickLists::Canonical(line, &problem);
        if (error != nullptr) *error = problem;
        if (canonical.isEmpty()) return false;
        const qsizetype colon = canonical.indexOf(QLatin1Char(':'));
        const QString prefix = canonical.left(colon);
        const QuickAttr* attr = quickAttr(prefix);
        if (attr == nullptr) return false;
        QString value = canonical.mid(colon + 1);
        if (storedLowercase(prefix)) value = value.toLower();
        const auto type = RouteEditor::QuickListType(list, kindOfPrefix(prefix));
        for (const auto& rule : std::as_const(profile.Rules)) {
            if (rule->type == type && ((*rule).*(attr->member)).contains(value)) {
                if (error != nullptr) *error = QCoreApplication::translate("QuickLists", "Already in this list.");
                return false;
            }
        }
        std::shared_ptr<Configs::RouteRule> target;
        bool allOff = !scope.isEmpty();
        for (const int index : scope) {
            const auto& rule = profile.Rules.at(index);
            if (!rule->disabled) allOff = false;
            if (target == nullptr && rule->type == type) target = rule;
        }
        if (target == nullptr) {
            target = Configs::RouteProfile::NewSimpleRule(type);
            if (target == nullptr) return false;
            target->disabled = allOff;
            target->name = uniqueRuleName(profile, target->name);
            profile.Rules.insert(qBound(0, insertAt, static_cast<int>(profile.Rules.size())), target);
        }
        ((*target).*(attr->member)).append(value);
        return true;
    }
} // namespace

namespace QuickLists {
    QList<Entry> Entries(const Configs::RouteProfile& profile, int first, int count) {
        QList<Entry> entries;
        const int end = qMin(first + count, static_cast<int>(profile.Rules.size()));
        for (int i = qMax(0, first); i < end; ++i) appendEntries(entries, profile.Rules.at(i));
        return entries;
    }

    QList<Entry> ListEntries(const Configs::RouteProfile& profile, Configs::simpleAction list) {
        QList<Entry> entries;
        for (const auto& rule : profile.Rules) {
            if (inList(rule, list)) appendEntries(entries, rule);
        }
        return entries;
    }

    QString Canonical(const QString& line, QString* error) {
        if (error != nullptr) error->clear();
        const QString text = line.trimmed();
        if (text.isEmpty()) return {};
        QString result;
        const qsizetype colon = text.indexOf(QLatin1Char(':'));
        const QString prefix = colon > 0 ? text.left(colon).trimmed() : QString();
        if (quickAttr(prefix) != nullptr) {
            result = prefix + QLatin1Char(':') + text.mid(colon + 1).trimmed();
        } else if (ipShape(text)) {
            result = QStringLiteral("ip:") + text;
        } else if (knownRuleSet(text) || ruleSetUrl(text)) {
            result = QStringLiteral("ruleset:") + text;
        } else if (text.contains(QLatin1String("://"))) {
            // Its slashes would otherwise make it a process path.
            if (error != nullptr)
                *error = QCoreApplication::translate("Validation", "Write only the domain, without a scheme such as https://; as written, this never matches.");
            return {};
        } else if (pathShape(text)) {
            result = QStringLiteral("processPath:") + text;
        } else if (text.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive)) {
            result = QStringLiteral("processName:") + text;
        } else if (const QString suffix = hostSuffix(text); !suffix.isEmpty()) {
            result = QStringLiteral("suffix:") + suffix;
        } else {
            if (error != nullptr) *error = QCoreApplication::translate("QuickLists", "Add a prefix such as keyword: or processName: to say what this is.");
            return {};
        }
        const auto issue = Configs::RouteProfile::CheckSimpleRule(result);
        if (issue.severity == Validation::Severity::Error && !issue.message.isEmpty()) {
            if (error != nullptr) *error = issue.message;
            return {};
        }
        return result;
    }

    bool Add(Configs::RouteProfile& profile, const RouteEditor::Row& row, const QString& line, QString* error) {
        QList<int> scope;
        const int end = qMin(row.first + row.count, static_cast<int>(profile.Rules.size()));
        for (int i = qMax(0, row.first); i < end; ++i) scope << i;
        return insertLine(profile, row.list, scope, end, line, error);
    }

    bool AddToList(Configs::RouteProfile& profile, Configs::simpleAction list, const QString& line, QString* error) {
        QList<int> scope;
        for (int i = 0; i < profile.Rules.size(); ++i) {
            if (inList(profile.Rules.at(i), list)) scope << i;
        }
        const int insertAt = scope.isEmpty() ? static_cast<int>(profile.Rules.size()) : scope.last() + 1;
        return insertLine(profile, list, scope, insertAt, line, error);
    }

    void Remove(const Entry& entry) {
        if (entry.rule == nullptr) return;
        if (const QuickAttr* attr = quickAttr(entry.prefix)) ((*entry.rule).*(attr->member)).removeOne(entry.value);
    }

    void EnsureLists(Configs::RouteProfile& profile) {
        for (const auto list : RouteEditor::QuickListOrder()) {
            if (std::any_of(profile.Rules.cbegin(), profile.Rules.cend(), [list](const auto& rule) { return inList(rule, list); })) continue;
            auto rule = Configs::RouteProfile::NewSimpleRule(RouteEditor::QuickListType(list, RouteEditor::EntryKind::Address));
            if (rule == nullptr) continue;
            rule->name = uniqueRuleName(profile, rule->name);
            profile.Rules << rule;
        }
    }

    QString TypeLabel(const QString& prefix) {
        if (prefix == QLatin1String("domain")) return QCoreApplication::translate("QuickLists", "domain");
        if (prefix == QLatin1String("suffix")) return QCoreApplication::translate("QuickLists", "suffix");
        if (prefix == QLatin1String("keyword")) return QCoreApplication::translate("QuickLists", "keyword");
        if (prefix == QLatin1String("regex")) return QCoreApplication::translate("QuickLists", "regex");
        if (prefix == QLatin1String("ruleset")) return QCoreApplication::translate("QuickLists", "rule-set");
        if (prefix == QLatin1String("ip")) return QCoreApplication::translate("QuickLists", "ip");
        if (prefix == QLatin1String("processName")) return QCoreApplication::translate("QuickLists", "process");
        if (prefix == QLatin1String("processPath")) return QCoreApplication::translate("QuickLists", "path");
        return prefix;
    }

    int ErrorCount(const QList<Entry>& entries) {
        return static_cast<int>(std::count_if(entries.cbegin(), entries.cend(), [](const Entry& entry) {
            return entry.issue.severity == Validation::Severity::Error && !entry.issue.message.isEmpty();
        }));
    }

    QString Summary(const QList<Entry>& entries) {
        if (entries.isEmpty()) return QCoreApplication::translate("QuickLists", "Empty, left out of the config");
        QStringList parts;
        parts << (entries.size() == 1 ? QCoreApplication::translate("QuickLists", "1 entry")
                                      : QCoreApplication::translate("QuickLists", "%1 entries").arg(entries.size()));
        if (const int errors = ErrorCount(entries); errors > 0) {
            parts << (errors == 1 ? QCoreApplication::translate("QuickLists", "1 error")
                                  : QCoreApplication::translate("QuickLists", "%1 errors").arg(errors));
        }
        QStringList prefixes;
        for (const auto& entry : entries) {
            if (!prefixes.contains(entry.prefix)) prefixes << entry.prefix;
        }
        QStringList labels;
        for (const auto& prefix : std::as_const(prefixes)) {
            if (const QString label = TypeLabel(prefix); !labels.contains(label)) labels << label;
        }
        parts << labels.join(QStringLiteral(", "));
        return parts.join(QStringLiteral(" · "));
    }
} // namespace QuickLists

namespace QuickListsDetail {
    constexpr int collapsedRows = 6;
    constexpr int expandedRows = 14;

    enum class Tone { Text, Muted, Danger };

    QColor colorOf(Tone tone) {
        const auto& tokens = themeManager()->tokens;
        return tone == Tone::Danger ? tokens.danger : tone == Tone::Muted ? tokens.muted : tokens.onSurface;
    }

    QLabel* makeLabel(QWidget* parent, Tone tone, bool wrap, const QString& text = {}) {
        auto* label = new QLabel(text, parent);
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(wrap);
        if (tone != Tone::Text) {
            const auto apply = [label, tone] { label->setStyleSheet(QStringLiteral("color: %1;").arg(colorOf(tone).name(QColor::HexArgb))); };
            apply();
            QObject::connect(themeManager(), &ThemeManager::themeChanged, label, apply);
        }
        return label;
    }

    void boldFont(QWidget* widget, qreal scale) {
        QFont font = widget->font();
        font.setBold(true);
        if (scale != 1.0 && font.pointSizeF() > 0) font.setPointSizeF(font.pointSizeF() * scale);
        widget->setFont(font);
    }

    QString entryCount(int count) {
        return count == 1 ? QCoreApplication::translate("QuickLists", "1 entry") : QCoreApplication::translate("QuickLists", "%1 entries").arg(count);
    }

    QString noWarpHint() {
        return QCoreApplication::translate("QuickLists", "Only differs from Proxy while WARP or Tor is on: these sites skip that layer.");
    }

    const QStringList& ruleSetLines() {
        static const QStringList lines = [] {
            QStringList items;
            items.reserve(static_cast<qsizetype>(ruleSetList.size()));
            for (const auto& item : ruleSetList) {
                items.append(QStringLiteral("ruleset:") + QString::fromUtf8(item.first.data(), static_cast<qsizetype>(item.first.size())));
            }
            return items;
        }();
        return lines;
    }

    ProcessCompletionModel* completionModel(QObject* parent) {
        return new ProcessCompletionModel({.head = {QStringLiteral("domain:"), QStringLiteral("suffix:"), QStringLiteral("regex:"), QStringLiteral("keyword:"),
                                                    QStringLiteral("ip:"), QStringLiteral("processName:"), QStringLiteral("processPath:"),
                                                    QStringLiteral("ruleset:")},
                                           .names = true,
                                           .namePrefix = QStringLiteral("processName:"),
                                           .paths = true,
                                           .pathPrefix = QStringLiteral("processPath:"),
                                           .tail = ruleSetLines()},
                                          parent);
    }

    void attachCompleter(QLineEdit* edit, QAbstractItemModel* model) {
        auto* completer = new QCompleter(model, edit);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        completer->popup()->setTextElideMode(Qt::ElideMiddle);
        edit->setCompleter(completer);
    }

    // The row of the entry a canonical line was stored as, -1 when none.
    int entryRow(const QList<QuickLists::Entry>& entries, const QString& canonical) {
        const qsizetype colon = canonical.indexOf(QLatin1Char(':'));
        if (colon < 0) return -1;
        const QString prefix = canonical.left(colon);
        QString value = canonical.mid(colon + 1);
        if (storedLowercase(prefix)) value = value.toLower();
        for (int i = 0; i < entries.size(); ++i) {
            if (entries.at(i).prefix == prefix && entries.at(i).value == value) return i;
        }
        return -1;
    }

    // Adds each line of the field; lines that fail stay in it, and the first failure shows under it.
    bool submitLines(QLineEdit* input, QLabel* errorLabel, const std::function<bool(const QString&, QString*)>& add, QString* lastAdded) {
        static const QRegularExpression breaks(QStringLiteral("[\\r\\n\\x{2028}\\x{2029}]"));
        QStringList failed;
        QString firstError;
        bool added = false;
        for (const QString& raw : input->text().split(breaks, Qt::SkipEmptyParts)) {
            const QString line = raw.trimmed();
            if (line.isEmpty()) continue;
            QString error;
            if (add(line, &error)) {
                added = true;
                *lastAdded = QuickLists::Canonical(line, nullptr);
                continue;
            }
            failed << line;
            if (firstError.isEmpty()) firstError = error;
        }
        input->setText(failed.join(QLatin1Char('\n')));
        errorLabel->setText(firstError);
        errorLabel->setVisible(!firstError.isEmpty());
        return added;
    }

    void putLine(QLineEdit* input, const QString& line) {
        const QString text = input->text().trimmed();
        input->setText(text.isEmpty() ? line : text + QLatin1Char('\n') + line);
        input->setFocus(Qt::OtherFocusReason);
        input->end(false);
    }

    class EntryModel : public QAbstractListModel {
    public:
        struct Item {
            QString label;
            bool middleElide = false;
            bool issue = false;
            bool error = false;
        };

        using QAbstractListModel::QAbstractListModel;

        void setEntries(QList<QuickLists::Entry> list) {
            beginResetModel();
            entries = std::move(list);
            items.clear();
            items.reserve(entries.size());
            QHash<QString, QString> labels;
            for (const auto& entry : std::as_const(entries)) {
                auto label = labels.constFind(entry.prefix);
                if (label == labels.cend()) label = labels.insert(entry.prefix, QuickLists::TypeLabel(entry.prefix));
                const bool issue = !entry.issue.message.isEmpty();
                items.append({label.value(), entry.prefix == QLatin1String("processPath") || entry.prefix == QLatin1String("ruleset"), issue,
                              issue && entry.issue.severity == Validation::Severity::Error});
            }
            endResetModel();
        }

        // At most that many rows, -1 for all.
        void setLimit(int rows) {
            if (limit == rows) return;
            beginResetModel();
            limit = rows;
            endResetModel();
        }

        [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override {
            if (parent.isValid()) return 0;
            const int size = static_cast<int>(entries.size());
            return limit < 0 ? size : qMin(size, limit);
        }

        [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override {
            if (!index.isValid() || index.row() >= entries.size()) return {};
            const auto& entry = entries.at(index.row());
            if (role == Qt::DisplayRole) return entry.value;
            if (role == Qt::ToolTipRole) {
                QString tip = entry.value.toHtmlEscaped();
                if (!entry.issue.message.isEmpty()) tip += QStringLiteral("<br>") + entry.issue.message.toHtmlEscaped();
                return QStringLiteral("<qt>") + tip + QStringLiteral("</qt>");
            }
            return {};
        }

        QList<QuickLists::Entry> entries;
        QList<Item> items;
        int limit = -1;
    };

    struct EntryMetrics {
        QFont small;
        int pad = 0;
        int vpad = 0;
        int line = 0;
        int chipWidth = 0;
        int chipHeight = 0;
        int removeSize = 0;
        int messageHeight = 0;
    };

    class EntryView;

    class EntryDelegate : public QStyledItemDelegate {
    public:
        explicit EntryDelegate(EntryView* owner);

        const EntryMetrics& metrics(const QFont& font) const {
            if (valid && font == metricsFont) return cached;
            const QFontMetrics fm(font);
            EntryMetrics m;
            m.small = font;
            if (font.pointSizeF() > 0) m.small.setPointSizeF(font.pointSizeF() * 0.88);
            else if (font.pixelSize() > 0) m.small.setPixelSize(qMax(8, qRound(font.pixelSize() * 0.88)));
            const QFontMetrics sm(m.small);
            m.pad = qMax(4, fm.height() / 3);
            m.vpad = qMax(2, fm.height() / 6);
            m.removeSize = fm.height();
            m.line = fm.height();
            m.chipHeight = qMin(m.line, sm.height() + 2);
            int widest = 0;
            for (const auto& attr : quickAttrs()) widest = qMax(widest, sm.horizontalAdvance(QuickLists::TypeLabel(QString::fromLatin1(attr.prefix))));
            m.chipWidth = widest + m.pad * 2;
            m.messageHeight = sm.height();
            metricsFont = font;
            cached = m;
            valid = true;
            return cached;
        }

        static QRect removeRect(const QRect& item, const EntryMetrics& m, Qt::LayoutDirection direction) {
            const QRect rect(item.right() - m.pad - m.removeSize + 1, item.top() + m.vpad + (m.line - m.removeSize) / 2, m.removeSize, m.removeSize);
            return QStyle::visualRect(direction, item, rect);
        }

        [[nodiscard]] int rowHeight(const QFont& font, bool issue) const {
            const auto& m = metrics(font);
            return m.line + m.vpad * 2 + (issue ? m.messageHeight + 1 : 0);
        }

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

        [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
            const auto* model = static_cast<const EntryModel*>(index.model());
            const bool issue = model != nullptr && index.row() < model->items.size() && model->items.at(index.row()).issue;
            const auto& m = metrics(option.font);
            return {m.chipWidth + m.removeSize + m.pad * 4, rowHeight(option.font, issue)};
        }

    private:
        EntryView* view;
        mutable QFont metricsFont;
        mutable EntryMetrics cached;
        mutable bool valid = false;
    };

    // Painted entries: × or Delete removes one, a double-click takes it back into the add field.
    class EntryView : public QListView {
    public:
        explicit EntryView(QWidget* parent) : QListView(parent), delegate(new EntryDelegate(this)) {
            setItemDelegate(delegate);
            setMouseTracking(true);
            setSelectionMode(QAbstractItemView::SingleSelection);
            setEditTriggers(QAbstractItemView::NoEditTriggers);
            setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        }

        std::function<void(int)> onRemove;
        std::function<void(int)> onEdit;
        QString emptyText;
        int hoverRow = -1;
        bool hoverRemove = false;

        // Fixes the height to that many rows; -1 lets the layout size it.
        void setVisibleRows(int rows) {
            visibleRows = rows;
            setSizePolicy(QSizePolicy::Expanding, rows > 0 ? QSizePolicy::Fixed : QSizePolicy::Expanding);
            updateGeometry();
        }

        [[nodiscard]] QSize sizeHint() const override {
            QSize size = QListView::sizeHint();
            if (visibleRows > 0) size.setHeight(heightFor(visibleRows));
            return size;
        }

        [[nodiscard]] QSize minimumSizeHint() const override {
            QSize size = QListView::minimumSizeHint();
            if (visibleRows > 0) size.setHeight(heightFor(visibleRows));
            return size;
        }

        // Restores the current row and the scroll position after the model was reset.
        void keepPosition(int current, int scroll) {
            executeDelayedItemsLayout();
            const int count = model() != nullptr ? model()->rowCount() : 0;
            if (current >= 0 && count > 0) setCurrentIndex(model()->index(qMin(current, count - 1), 0));
            verticalScrollBar()->setValue(scroll);
        }

        void selectRow(int row) {
            if (model() == nullptr || row < 0 || row >= model()->rowCount()) return;
            setCurrentIndex(model()->index(row, 0));
            scrollTo(currentIndex());
        }

        void reset() override {
            hoverRow = -1;
            hoverRemove = false;
            pressedRemove = -1;
            QListView::reset();
            if (visibleRows > 0) updateGeometry();
        }

    protected:
        bool event(QEvent* event) override {
            // Delete must reach keyPressEvent even when a Delete shortcut is set further up.
            if (event->type() == QEvent::ShortcutOverride && isRemoveKey(static_cast<QKeyEvent*>(event)) && currentIndex().isValid()) {
                event->accept();
                return true;
            }
            return QListView::event(event);
        }

        void keyPressEvent(QKeyEvent* event) override {
            if (isRemoveKey(event) && currentIndex().isValid()) {
                defer(&EntryView::onRemove, currentIndex());
                event->accept();
                return;
            }
            QListView::keyPressEvent(event);
        }

        void mouseMoveEvent(QMouseEvent* event) override {
            updateHover(event->position().toPoint());
            QListView::mouseMoveEvent(event);
        }

        void mousePressEvent(QMouseEvent* event) override {
            pressedRemove = -1;
            if (event->button() == Qt::LeftButton) {
                const QModelIndex index = indexAt(event->position().toPoint());
                if (index.isValid() && removeRectOf(index).contains(event->position().toPoint())) {
                    pressedRemove = index.row();
                    event->accept();
                    return;
                }
            }
            QListView::mousePressEvent(event);
        }

        void mouseReleaseEvent(QMouseEvent* event) override {
            if (pressedRemove >= 0 && event->button() == Qt::LeftButton) {
                const int pressed = pressedRemove;
                pressedRemove = -1;
                const QModelIndex index = indexAt(event->position().toPoint());
                if (index.isValid() && index.row() == pressed && removeRectOf(index).contains(event->position().toPoint())) defer(&EntryView::onRemove, index);
                event->accept();
                return;
            }
            QListView::mouseReleaseEvent(event);
        }

        void mouseDoubleClickEvent(QMouseEvent* event) override {
            const QModelIndex index = indexAt(event->position().toPoint());
            if (event->button() == Qt::LeftButton && index.isValid() && !removeRectOf(index).contains(event->position().toPoint())) {
                defer(&EntryView::onEdit, index);
                event->accept();
                return;
            }
            QListView::mouseDoubleClickEvent(event);
        }

        void leaveEvent(QEvent* event) override {
            setHover(-1, false);
            QListView::leaveEvent(event);
        }

        void paintEvent(QPaintEvent* event) override {
            QListView::paintEvent(event);
            if (model() == nullptr || model()->rowCount() > 0 || emptyText.isEmpty()) return;
            const auto& m = delegate->metrics(font());
            QPainter painter(viewport());
            painter.setPen(themeManager()->tokens.muted);
            painter.drawText(viewport()->rect().adjusted(m.pad, m.vpad, -m.pad, -m.vpad), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, emptyText);
        }

    private:
        EntryDelegate* delegate;
        int visibleRows = -1;
        int pressedRemove = -1;

        static bool isRemoveKey(const QKeyEvent* event) {
            const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeyboardModifiers(Qt::KeypadModifier);
            return (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) && modifiers == Qt::NoModifier;
        }

        [[nodiscard]] int heightFor(int rows) const {
            const int count = model() != nullptr ? model()->rowCount() : 0;
            const int shown = qMin(count, rows);
            int height = 0;
            for (int i = 0; i < shown; ++i) height += sizeHintForRow(i);
            if (shown == 0) height = delegate->rowHeight(font(), false);
            return height + frameWidth() * 2;
        }

        [[nodiscard]] QRect removeRectOf(const QModelIndex& index) const {
            return EntryDelegate::removeRect(visualRect(index), delegate->metrics(font()), layoutDirection());
        }

        void setHover(int row, bool onRemove) {
            if (row == hoverRow && onRemove == hoverRemove) return;
            if (model() != nullptr && hoverRow >= 0) viewport()->update(visualRect(model()->index(hoverRow, 0)));
            hoverRow = row;
            hoverRemove = onRemove;
            if (model() != nullptr && hoverRow >= 0) viewport()->update(visualRect(model()->index(hoverRow, 0)));
        }

        void updateHover(const QPoint& pos) {
            const QModelIndex index = indexAt(pos);
            setHover(index.isValid() ? index.row() : -1, index.isValid() && removeRectOf(index).contains(pos));
        }

        // Runs after the event that asked for it, since the handler resets this view's model.
        void defer(std::function<void(int)> EntryView::* handler, const QModelIndex& index) {
            const QPersistentModelIndex target(index);
            QTimer::singleShot(0, this, [this, handler, target] {
                if (target.isValid() && (this->*handler)) (this->*handler)(target.row());
            });
        }
    };

    EntryDelegate::EntryDelegate(EntryView* owner) : QStyledItemDelegate(owner), view(owner) {}

    void EntryDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
        const auto* model = static_cast<const EntryModel*>(index.model());
        if (model == nullptr || index.row() >= model->items.size()) return;
        const auto& item = model->items.at(index.row());
        const auto& tokens = themeManager()->tokens;
        const auto& m = metrics(option.font);
        const bool selected = option.state & QStyle::State_Selected;
        const auto visual = [&option](const QRect& rect) { return QStyle::visualRect(option.direction, option.rect, rect); };

        painter->save();
        if (item.issue && !selected) {
            QColor tint = item.error ? tokens.danger : tokens.warning;
            tint.setAlphaF(0.10f);
            painter->fillRect(option.rect, tint);
        }
        QStyleOptionViewItem panel(option);
        initStyleOption(&panel, index);
        const QStyle* style = option.widget != nullptr ? option.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &panel, painter, option.widget);
        painter->setRenderHint(QPainter::Antialiasing);

        const QColor highlighted = option.palette.color(QPalette::HighlightedText);
        const QRect line(option.rect.left() + m.pad, option.rect.top() + m.vpad, option.rect.width() - m.pad * 2, m.line);

        const QRect chip(line.left(), line.top() + (m.line - m.chipHeight) / 2, m.chipWidth, m.chipHeight);
        QColor chipFill = selected ? highlighted : tokens.onSurface;
        chipFill.setAlphaF(selected ? 0.18f : 0.08f);
        painter->setPen(Qt::NoPen);
        painter->setBrush(chipFill);
        painter->drawRoundedRect(QRectF(visual(chip)), 3, 3);
        painter->setFont(m.small);
        painter->setPen(selected ? highlighted : tokens.muted);
        painter->drawText(visual(chip), Qt::AlignCenter, item.label);

        const QRect remove = EntryDelegate::removeRect(option.rect, m, option.direction);
        const bool hover = view != nullptr && view->hoverRow == index.row() && view->hoverRemove;
        if (hover) {
            QColor ring = selected ? highlighted : tokens.onSurface;
            ring.setAlphaF(0.14f);
            painter->setPen(Qt::NoPen);
            painter->setBrush(ring);
            painter->drawEllipse(QRectF(remove));
        }
        const QColor cross = selected ? highlighted : hover ? tokens.onSurface : tokens.muted;
        painter->setPen(QPen(cross, 1.4, Qt::SolidLine, Qt::RoundCap));
        const qreal inset = remove.width() * 0.32;
        const QRectF mark = QRectF(remove).adjusted(inset, inset, -inset, -inset);
        painter->drawLine(mark.topLeft(), mark.bottomRight());
        painter->drawLine(mark.topRight(), mark.bottomLeft());

        const int valueLeft = chip.right() + 1 + m.pad;
        const int valueRight = option.rect.right() - m.pad * 2 - m.removeSize;
        const QRect valueRect(valueLeft, line.top(), qMax(0, valueRight - valueLeft + 1), m.line);
        painter->setFont(option.font);
        painter->setPen(selected ? highlighted : item.error ? tokens.danger : item.issue ? tokens.warning : tokens.onSurface);
        const QString value = model->entries.at(index.row()).value;
        painter->drawText(visual(valueRect), Qt::AlignLeft | Qt::AlignVCenter,
                          option.fontMetrics.elidedText(value, item.middleElide ? Qt::ElideMiddle : Qt::ElideRight, valueRect.width()));

        if (item.issue) {
            const QRect messageRect(valueLeft, line.bottom() + 2, qMax(0, line.right() - valueLeft + 1), m.messageHeight);
            painter->setFont(m.small);
            painter->setPen(selected ? highlighted : item.error ? tokens.danger : tokens.warning);
            const QString message = model->entries.at(index.row()).issue.message;
            painter->drawText(visual(messageRect), Qt::AlignLeft | Qt::AlignVCenter,
                              QFontMetrics(m.small).elidedText(message, Qt::ElideRight, messageRect.width()));
        }
        painter->restore();
    }

    // Takes Enter for itself, so a dialog's default button never fires from here.
    class EntryInput : public QLineEdit {
    public:
        using QLineEdit::QLineEdit;

        std::function<void()> onSubmit;

    protected:
        void keyPressEvent(QKeyEvent* event) override {
            if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
                // With the popup open, Enter picks the highlighted completion.
                if (completer() != nullptr && completer()->popup() != nullptr && completer()->popup()->isVisible()) {
                    QLineEdit::keyPressEvent(event);
                    return;
                }
                event->accept();
                if (onSubmit) onSubmit();
                return;
            }
            QLineEdit::keyPressEvent(event);
        }
    };

    class BannerPanel : public QWidget {
    public:
        explicit BannerPanel(QWidget* parent) : QWidget(parent), label(makeLabel(this, Tone::Text, true)) {
            auto* layout = new QHBoxLayout(this);
            const int margin = fontMetrics().height() / 2;
            layout->setContentsMargins(margin + 4, margin, margin + 4, margin);
            layout->addWidget(label);
        }

        QLabel* label;

    protected:
        void paintEvent(QPaintEvent*) override {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            QColor fill = themeManager()->tokens.info;
            QColor border = fill;
            fill.setAlphaF(0.14f);
            border.setAlphaF(0.35f);
            painter.setPen(QPen(border, 1));
            painter.setBrush(fill);
            painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
        }
    };
} // namespace QuickListsDetail

using namespace QuickListsDetail;

struct QuickListEditor::Private {
    std::shared_ptr<Configs::RouteProfile> profile;
    RouteEditor::Row row;
    QList<RouteEditor::Row> rows;
    // Finds the row again after rules were inserted before it.
    std::shared_ptr<Configs::RouteRule> firstRule;
    QLabel* title = nullptr;
    RouteEditor::ThenPill* pill = nullptr;
    QLabel* intro = nullptr;
    QLabel* offNote = nullptr;
    QLabel* rowsNote = nullptr;
    EntryModel* model = nullptr;
    EntryView* view = nullptr;
    EntryInput* input = nullptr;
    QLabel* addError = nullptr;

    void refreshRow() {
        rows = RouteEditor::BuildRows(*profile);
        const int at = RouteEditor::RowOfRule(rows, static_cast<int>(profile->Rules.indexOf(firstRule)));
        if (at >= 0) row = rows.at(at);
    }
};

QuickListEditor::QuickListEditor(std::shared_ptr<Configs::RouteProfile> profile, QWidget* parent) : QWidget(parent), d(std::make_unique<Private>()) {
    d->profile = std::move(profile);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* header = new QHBoxLayout();
    d->title = makeLabel(this, Tone::Text, false);
    boldFont(d->title, 1.3);
    d->pill = new RouteEditor::ThenPill(this);
    header->addWidget(d->title);
    header->addWidget(d->pill);
    header->addStretch(1);
    layout->addLayout(header);

    d->intro = makeLabel(this, Tone::Muted, true);
    layout->addWidget(d->intro);
    d->offNote = makeLabel(this, Tone::Muted, true, tr("Off: kept in the profile but left out of the config."));
    layout->addWidget(d->offNote);
    d->rowsNote = makeLabel(this, Tone::Muted, true);
    layout->addWidget(d->rowsNote);

    d->model = new EntryModel(this);
    d->view = new EntryView(this);
    d->view->setModel(d->model);
    d->view->emptyText = tr("No entries. An empty list is left out of the config.");
    d->view->onRemove = [this](int row) { removeEntry(row); };
    d->view->onEdit = [this](int row) { editEntry(row); };
    layout->addWidget(d->view, 1);

    auto* addRow = new QHBoxLayout();
    d->input = new EntryInput(this);
    d->input->setPlaceholderText(tr("example.com, 10.0.0.0/8, app.exe or ruleset:name"));
    d->input->setClearButtonEnabled(true);
    attachCompleter(d->input, completionModel(this));
    d->input->onSubmit = [this] { submit(); };
    auto* addButton = new QPushButton(tr("Add"), this);
    addButton->setAutoDefault(false);
    connect(addButton, &QPushButton::clicked, this, [this] { submit(); });
    addRow->addWidget(d->input, 1);
    addRow->addWidget(addButton);
    layout->addLayout(addRow);

    d->addError = makeLabel(this, Tone::Danger, true);
    d->addError->hide();
    connect(d->input, &QLineEdit::textEdited, d->addError, [this] {
        d->addError->clear();
        d->addError->hide();
    });
    layout->addWidget(d->addError);
}

QuickListEditor::~QuickListEditor() = default;

void QuickListEditor::setRow(const RouteEditor::Row& row, const QList<RouteEditor::Row>& rows) {
    const auto first = row.first >= 0 && row.first < d->profile->Rules.size() ? d->profile->Rules.at(row.first) : nullptr;
    const bool same = first != nullptr && first == d->firstRule;
    d->row = row;
    d->rows = rows;
    d->firstRule = first;
    if (!same) {
        d->addError->clear();
        d->addError->hide();
    }
    reload(same ? d->view->currentIndex().row() : -1, same ? d->view->verticalScrollBar()->value() : 0);
}

QWidget* QuickListEditor::revealEntry(const QuickLists::Entry& entry) {
    const auto& entries = d->model->entries;
    for (int i = 0; i < entries.size(); ++i) {
        const auto& shown = entries.at(i);
        if (shown.rule == entry.rule && shown.prefix == entry.prefix && shown.value == entry.value) {
            d->view->selectRow(i);
            break;
        }
    }
    return d->view;
}

void QuickListEditor::reload(int current, int scroll) {
    const auto list = d->row.list;
    const auto then = RouteEditor::ThenOfList(list);
    d->title->setText(tr("%1 list").arg(RouteEditor::ListName(list)));
    d->pill->setPill(RouteEditor::ThenLabel(then), RouteEditor::ToneOf(then));
    // The pill already says where entries go; only WARP bypass needs a word on when it differs from Proxy.
    d->intro->setText(list == Configs::warpBypass ? noWarpHint() : QString());
    d->intro->setVisible(list == Configs::warpBypass);

    const int end = qMin(d->row.first + d->row.count, static_cast<int>(d->profile->Rules.size()));
    bool allOff = d->row.first < end;
    for (int i = qMax(0, d->row.first); i < end; ++i) {
        if (!d->profile->Rules.at(i)->disabled) allOff = false;
    }
    d->offNote->setVisible(allOff);

    QStringList others;
    for (int i = 0; i < d->rows.size(); ++i) {
        const auto& other = d->rows.at(i);
        if (other.kind == RouteEditor::RowKind::QuickList && other.list == list && other.first != d->row.first)
            others << QLatin1Char('#') + QString::number(i + 1);
    }
    d->rowsNote->setText(others.isEmpty() ? QString() : tr("The %1 list also has rows at %2.").arg(RouteEditor::ListName(list), others.join(QStringLiteral(", "))));
    d->rowsNote->setVisible(!others.isEmpty());

    d->model->setEntries(QuickLists::Entries(*d->profile, d->row.first, d->row.count));
    d->view->keepPosition(current, scroll);
}

void QuickListEditor::submit() {
    QString lastAdded;
    const bool added = submitLines(d->input, d->addError, [this](const QString& line, QString* error) {
        if (!QuickLists::Add(*d->profile, d->row, line, error)) return false;
        // A new rule after the row joins it, and the next line must find it there.
        d->refreshRow();
        return true;
    }, &lastAdded);
    if (!added) return;
    reload(-1, 0);
    d->view->selectRow(entryRow(d->model->entries, lastAdded));
    emit changed();
}

void QuickListEditor::removeEntry(int row) {
    if (row < 0 || row >= d->model->entries.size()) return;
    int current = d->view->currentIndex().row();
    if (current > row) --current;
    QuickLists::Remove(d->model->entries.at(row));
    reload(current, d->view->verticalScrollBar()->value());
    emit changed();
}

void QuickListEditor::editEntry(int row) {
    if (row < 0 || row >= d->model->entries.size()) return;
    const auto entry = d->model->entries.at(row);
    QuickLists::Remove(entry);
    reload(-1, d->view->verticalScrollBar()->value());
    putLine(d->input, entry.prefix + QLatin1Char(':') + entry.value);
    emit changed();
}

struct QuickListsView::Card {
    Configs::simpleAction list = Configs::proxy;
    QFrame* frame = nullptr;
    QLabel* count = nullptr;
    EntryModel* model = nullptr;
    EntryView* view = nullptr;
    QToolButton* more = nullptr;
    EntryInput* input = nullptr;
    QLabel* error = nullptr;
    bool expanded = false;
};

struct QuickListsView::Private {
    std::shared_ptr<Configs::RouteProfile> profile;
    QScrollArea* scroll = nullptr;
    BannerPanel* banner = nullptr;
    std::vector<std::unique_ptr<QuickListsView::Card>> cards;
};

QuickListsView::QuickListsView(std::shared_ptr<Configs::RouteProfile> profile, QWidget* parent) : QWidget(parent), d(std::make_unique<Private>()) {
    d->profile = std::move(profile);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    d->scroll = new QScrollArea(this);
    d->scroll->setWidgetResizable(true);
    d->scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget(d->scroll);
    auto* layout = new QVBoxLayout(content);

    d->banner = new BannerPanel(content);
    layout->addWidget(d->banner);

    auto* grid = new QGridLayout();
    auto* completions = completionModel(this);
    const auto& order = RouteEditor::QuickListOrder();
    for (int i = 0; i < order.size(); ++i) {
        auto card = std::make_unique<Card>();
        Card* c = card.get();
        c->list = order.at(i);
        c->frame = new QFrame(content);
        c->frame->setFrameShape(QFrame::StyledPanel);
        auto* box = new QVBoxLayout(c->frame);

        auto* header = new QHBoxLayout();
        auto* name = makeLabel(c->frame, Tone::Text, false, RouteEditor::ListName(c->list));
        boldFont(name, 1.0);
        c->count = makeLabel(c->frame, Tone::Muted, false);
        auto* pill = new RouteEditor::ThenPill(c->frame);
        const auto then = RouteEditor::ThenOfList(c->list);
        pill->setPill(RouteEditor::ThenLabel(then), RouteEditor::ToneOf(then));
        header->addWidget(name);
        header->addWidget(c->count);
        header->addStretch(1);
        header->addWidget(pill);
        box->addLayout(header);
        if (c->list == Configs::warpBypass) box->addWidget(makeLabel(c->frame, Tone::Muted, true, noWarpHint()));

        c->model = new EntryModel(c->frame);
        c->view = new EntryView(c->frame);
        c->view->setModel(c->model);
        c->view->emptyText = tr("Empty.");
        c->view->onRemove = [this, c](int row) { removeEntry(*c, row); };
        c->view->onEdit = [this, c](int row) { editEntry(*c, row); };
        box->addWidget(c->view);

        c->more = new QToolButton(c->frame);
        c->more->setAutoRaise(true);
        c->more->setToolButtonStyle(Qt::ToolButtonTextOnly);
        connect(c->more, &QToolButton::clicked, this, [this, c] { setExpanded(*c, !c->expanded); });
        box->addWidget(c->more, 0, Qt::AlignLeft);

        auto* addRow = new QHBoxLayout();
        c->input = new EntryInput(c->frame);
        c->input->setPlaceholderText(tr("Add an entry"));
        c->input->setClearButtonEnabled(true);
        attachCompleter(c->input, completions);
        c->input->onSubmit = [this, c] { submit(*c); };
        auto* add = new QPushButton(tr("Add"), c->frame);
        add->setAutoDefault(false);
        connect(add, &QPushButton::clicked, this, [this, c] { submit(*c); });
        addRow->addWidget(c->input, 1);
        addRow->addWidget(add);
        box->addLayout(addRow);

        c->error = makeLabel(c->frame, Tone::Danger, true);
        c->error->hide();
        connect(c->input, &QLineEdit::textEdited, c->error, [c] {
            c->error->clear();
            c->error->hide();
        });
        box->addWidget(c->error);
        box->addStretch(1);

        grid->addWidget(c->frame, i / 2, i % 2);
        d->cards.push_back(std::move(card));
    }
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    layout->addLayout(grid);

    layout->addWidget(makeLabel(content, Tone::Muted, true,
                                tr("A line without a prefix is typed by its shape: example.com is a domain suffix, 10.0.0.0/8 an IP range, app.exe a process. "
                                   "The prefixes domain:, keyword:, regex:, ruleset: and processPath: still work.")));
    layout->addStretch(1);
    d->scroll->setWidget(content);
    outer->addWidget(d->scroll);

    reload();
}

QuickListsView::~QuickListsView() = default;

void QuickListsView::reload() {
    for (const auto& card : d->cards) reloadCard(*card, -1, 0);
    updateBanner();
}

QList<Validation::FormIssue> QuickListsView::issues() {
    QList<Validation::FormIssue> result;
    const QPointer<QuickListsView> self(this);
    for (const auto& card : d->cards) {
        const auto& entries = card->model->entries;
        for (int i = 0; i < entries.size(); ++i) {
            const auto& entry = entries.at(i);
            if (entry.issue.message.isEmpty()) continue;
            Validation::FormIssue issue;
            // A turned-off list never blocks saving.
            issue.severity = entry.rule != nullptr && entry.rule->disabled ? Validation::Severity::Warning : entry.issue.severity;
            issue.label = tr("%1 list").arg(RouteEditor::ListName(card->list));
            issue.message = tr("Entry %1 (\"%2\"): %3").arg(QString::number(i + 1), entry.value, entry.issue.message);
            issue.reveal = [self, list = card->list, i]() -> QWidget* { return self.isNull() ? nullptr : self->reveal(list, i); };
            result << issue;
        }
    }
    return result;
}

void QuickListsView::reloadCard(Card& card, int current, int scroll) {
    card.model->setEntries(QuickLists::ListEntries(*d->profile, card.list));
    const int count = static_cast<int>(card.model->entries.size());
    card.count->setText(count > 0 ? entryCount(count) : QString());
    setExpanded(card, card.expanded);
    card.view->keepPosition(current, scroll);
}

void QuickListsView::updateBanner() {
    const auto rows = RouteEditor::BuildRows(*d->profile);
    int before = 0;
    int after = 0;
    bool seenList = false;
    for (const auto& row : rows) {
        if (row.kind == RouteEditor::RowKind::QuickList) seenList = true;
        else if (seenList) ++after;
        else ++before;
    }
    d->banner->label->setText(tr("Advanced mode shows other rules too. Before these lists: %1. After them: %2.").arg(before).arg(after));
    d->banner->setVisible(before + after > 0);
}

void QuickListsView::submit(Card& card) {
    QString lastAdded;
    const bool added = submitLines(card.input, card.error, [this, &card](const QString& line, QString* error) {
        return QuickLists::AddToList(*d->profile, card.list, line, error);
    }, &lastAdded);
    if (!added) return;
    reloadCard(card, -1, card.view->verticalScrollBar()->value());
    card.view->selectRow(entryRow(card.model->entries, lastAdded));
    updateBanner();
    emit changed();
}

void QuickListsView::removeEntry(Card& card, int row) {
    if (row < 0 || row >= card.model->entries.size()) return;
    int current = card.view->currentIndex().row();
    if (current > row) --current;
    QuickLists::Remove(card.model->entries.at(row));
    reloadCard(card, current, card.view->verticalScrollBar()->value());
    emit changed();
}

void QuickListsView::editEntry(Card& card, int row) {
    if (row < 0 || row >= card.model->entries.size()) return;
    const auto entry = card.model->entries.at(row);
    QuickLists::Remove(entry);
    reloadCard(card, -1, card.view->verticalScrollBar()->value());
    putLine(card.input, entry.prefix + QLatin1Char(':') + entry.value);
    emit changed();
}

void QuickListsView::setExpanded(Card& card, bool expanded) {
    const int count = static_cast<int>(card.model->entries.size());
    card.expanded = expanded && count > collapsedRows;
    card.model->setLimit(card.expanded ? -1 : collapsedRows);
    card.view->setVerticalScrollBarPolicy(card.expanded ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
    card.view->setVisibleRows(card.expanded ? expandedRows : collapsedRows);
    card.more->setVisible(count > collapsedRows);
    card.more->setText(card.expanded ? tr("Show fewer") : tr("%1 more").arg(count - collapsedRows));
}

QWidget* QuickListsView::reveal(Configs::simpleAction list, int index) {
    for (const auto& card : d->cards) {
        if (card->list != list) continue;
        if (index >= collapsedRows && !card->expanded) setExpanded(*card, true);
        card->view->selectRow(index);
        if (auto* content = d->scroll->widget(); content != nullptr && content->layout() != nullptr) content->layout()->activate();
        d->scroll->ensureWidgetVisible(card->view);
        return card->view;
    }
    return nullptr;
}
