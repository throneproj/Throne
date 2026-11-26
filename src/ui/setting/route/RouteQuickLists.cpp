#include "include/ui/setting/route/RouteQuickLists.h"

#include "include/ui/setting/ProcessPicker.h"
#include "include/ui/setting/ThemeManager.hpp"
#include "include/ui/setting/route/RouteEditorWidgets.h"

#include <srslist.h>

#include <QAbstractListModel>
#include <QApplication>
#include <QClipboard>
#include <QCompleter>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <QHash>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSet>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTextBlock>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <functional>

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

    bool pathShape(const QString& text) {
        if (text.contains(QLatin1Char('\\')) || text.contains(QLatin1Char('/'))) return true;
        return text.size() >= 2 && text.at(1) == QLatin1Char(':') && text.at(0).isLetter();
    }

    // example.com/page or example.com:8080/page: slashes, but a host first.
    bool hostWithPath(const QString& text) {
        if (text.contains(QLatin1Char('\\')) || text.startsWith(QLatin1Char('/'))) return false;
        return !RouteEditor::HostSuffix(text.section(QLatin1Char('/'), 0, 0).section(QLatin1Char(':'), 0, 0)).isEmpty();
    }

    // An absolute process path, which may hold spaces; URLs and CIDR lists hold slashes too.
    bool wholePath(const QString& text) {
        if (text.contains(QLatin1String("://"))) return false;
        if (text.startsWith(QLatin1Char('/')) || text.startsWith(QLatin1Char('\\')) || text.startsWith(QLatin1String("~/"))) return true;
        return text.size() >= 3 && text.at(0).isLetter() && text.at(1) == QLatin1Char(':') && (text.at(2) == QLatin1Char('\\') || text.at(2) == QLatin1Char('/'));
    }

    bool wholeValue(const QString& prefix) {
        return prefix == QLatin1String("regex") || prefix == QLatin1String("processName") || prefix == QLatin1String("processPath");
    }

    QString clashPrefix(const QString& kind) {
        static const QHash<QString, QString> prefixes = {
            {"DOMAIN", "domain"}, {"DOMAIN-SUFFIX", "suffix"}, {"DOMAIN-KEYWORD", "keyword"}, {"DOMAIN-REGEX", "regex"},
            {"IP-CIDR", "ip"}, {"IP-CIDR6", "ip"}, {"PROCESS-NAME", "processName"}, {"PROCESS-PATH", "processPath"},
        };
        return prefixes.value(kind);
    }

    QString alreadyInList() {
        return QCoreApplication::translate("QuickLists", "Already in this list.");
    }

    // Entries compare by this: domain, suffix and keyword are stored lowercased.
    QString entryKey(const QString& prefix, const QString& value) {
        return prefix + QLatin1Char(':') + (storedLowercase(prefix) ? value.toLower() : value);
    }

    // The key of a "prefix:value" line; empty when it has no known prefix.
    QString lineKey(const QString& line) {
        const qsizetype colon = line.indexOf(QLatin1Char(':'));
        const QString prefix = colon > 0 ? line.left(colon).trimmed() : QString();
        return quickAttr(prefix) != nullptr ? entryKey(prefix, line.mid(colon + 1).trimmed()) : QString();
    }

    QString lineOf(const QuickLists::Entry& entry) {
        return entry.prefix + QLatin1Char(':') + entry.value;
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
                if (error != nullptr) *error = alreadyInList();
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

    QStringList SplitLines(const QString& text) {
        static const QRegularExpression yamlKey(QStringLiteral("^[A-Za-z_][\\w-]*:$"));
        QStringList lines;
        for (const QString& line : RouteEditor::ValueLines(text)) {
            QString kind;
            if (const QString value = RouteEditor::ClashRuleValue(line, &kind); !value.isEmpty()) {
                const QString prefix = clashPrefix(kind);
                lines << (prefix.isEmpty() ? value : prefix + QLatin1Char(':') + value);
                continue;
            }
            const qsizetype colon = line.indexOf(QLatin1Char(':'));
            const QString prefix = colon > 0 ? line.left(colon).trimmed() : QString();
            if (quickAttr(prefix) != nullptr) {
                const QString rest = line.mid(colon + 1);
                if (wholeValue(prefix)) {
                    lines << prefix + QLatin1Char(':') + RouteEditor::Unquote(rest, false);
                    continue;
                }
                const QStringList words = RouteEditor::SplitWords(rest);
                // A bare prefix still reaches Canonical, which says what is missing.
                if (words.isEmpty()) lines << prefix + QLatin1Char(':') + rest.trimmed();
                for (const QString& word : words) {
                    const bool prefixed = word.contains(QLatin1Char(':')) && quickAttr(word.section(QLatin1Char(':'), 0, 0)) != nullptr;
                    lines << (prefixed ? word : prefix + QLatin1Char(':') + word);
                }
                continue;
            }
            // A Clash rule provider's "payload:".
            if (yamlKey.match(line).hasMatch()) continue;
            if (const QString whole = RouteEditor::Unquote(line, false); wholePath(whole)) {
                lines << whole;
                continue;
            }
            QStringList words = RouteEditor::SplitWords(line);
            RouteEditor::DropHostsAddress(words);
            lines << words;
        }
        return lines;
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
        } else if (RouteEditor::IpShape(text)) {
            result = QStringLiteral("ip:") + text;
        } else if (RouteEditor::RuleSetShape(text)) {
            result = QStringLiteral("ruleset:") + text;
        } else {
            const bool url = text.contains(QLatin1String("://"));
            // Paths and .exe names keep their case, so only the host checks read the tidied text.
            const QString host = RouteEditor::TidyValue(QStringLiteral("domain_suffix"), text);
            if (!url && pathShape(text) && !hostWithPath(text)) {
                result = QStringLiteral("processPath:") + text;
            } else if (!url && text.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive)) {
                result = QStringLiteral("processName:") + text;
            } else if (RouteEditor::IpShape(host)) {
                result = QStringLiteral("ip:") + host;
            } else if (const QString suffix = RouteEditor::HostSuffix(host); !suffix.isEmpty()) {
                result = QStringLiteral("suffix:") + suffix;
            } else if (url) {
                if (error != nullptr)
                    *error = QCoreApplication::translate("Validation", "Write only the domain, without a scheme such as https://; as written, this never matches.");
                return {};
            } else {
                if (error != nullptr) *error = QCoreApplication::translate("QuickLists", "Add a prefix such as keyword: or processName: to say what this is.");
                return {};
            }
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

    QString noWarpHint() {
        return QCoreApplication::translate("QuickLists", "Only differs from Proxy while an exit is on (Routing Settings > Exit): these sites skip it and leave through the selected profile.");
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

    // Adds each value of the field; values that fail stay in it, and the first failure shows under it.
    bool submitLines(QLineEdit* input, QLabel* errorLabel, const std::function<bool(const QString&, QString*)>& add, QString* lastAdded) {
        const QStringList lines = QuickLists::SplitLines(input->text());
        // Pasted lists repeat values; only a single typed duplicate gets a word.
        const bool bulk = lines.size() > 1;
        QStringList failed;
        QString firstError;
        bool added = false;
        for (const QString& line : lines) {
            QString error;
            if (add(line, &error)) {
                added = true;
                *lastAdded = QuickLists::Canonical(line, nullptr);
                continue;
            }
            if (bulk && error == alreadyInList()) continue;
            if (!failed.contains(line)) failed << line;
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

    void copyLines(const QStringList& lines) {
        if (!lines.isEmpty()) QGuiApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
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

        [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override {
            return parent.isValid() ? 0 : static_cast<int>(entries.size());
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

    // Painted entries: × removes one, Delete the selected ones, a double-click takes one back into the add field.
    class EntryView : public QListView {
    public:
        explicit EntryView(QWidget* parent) : QListView(parent), delegate(new EntryDelegate(this)) {
            setItemDelegate(delegate);
            setMouseTracking(true);
            setSelectionMode(QAbstractItemView::ExtendedSelection);
            setEditTriggers(QAbstractItemView::NoEditTriggers);
            setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        }

        std::function<void(const QList<int>&)> onRemove;
        std::function<void(int)> onEdit;
        std::function<void(const QPoint&)> onMenu;
        QString emptyText;
        int hoverRow = -1;
        bool hoverRemove = false;

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
        }

        [[nodiscard]] bool hasSelection() const {
            return selectionModel() != nullptr && selectionModel()->hasSelection();
        }

        // In view order.
        [[nodiscard]] QList<int> selectedRowList() const {
            QList<int> rows;
            if (selectionModel() != nullptr) {
                for (const QModelIndex& index : selectionModel()->selectedIndexes()) rows << index.row();
            }
            std::sort(rows.begin(), rows.end());
            rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
            return rows;
        }

        void copySelected() const {
            const auto* entries = static_cast<const EntryModel*>(model());
            if (entries == nullptr) return;
            QStringList lines;
            for (const int row : selectedRowList()) {
                if (row < entries->entries.size()) lines << lineOf(entries->entries.at(row));
            }
            copyLines(lines);
        }

        void removeSelected() {
            if (hasSelection()) deferRemove(selectionModel()->selectedIndexes());
        }

    protected:
        bool event(QEvent* event) override {
            // Delete and Copy must reach keyPressEvent even when a shortcut for them is set further up.
            if (event->type() == QEvent::ShortcutOverride && hasSelection()) {
                const auto* key = static_cast<QKeyEvent*>(event);
                if (isRemoveKey(key) || key->matches(QKeySequence::Copy)) {
                    event->accept();
                    return true;
                }
            }
            return QListView::event(event);
        }

        void keyPressEvent(QKeyEvent* event) override {
            if (event->matches(QKeySequence::Copy)) {
                copySelected();
                event->accept();
                return;
            }
            if (isRemoveKey(event) && hasSelection()) {
                removeSelected();
                event->accept();
                return;
            }
            QListView::keyPressEvent(event);
        }

        void contextMenuEvent(QContextMenuEvent* event) override {
            if (!onMenu) {
                QListView::contextMenuEvent(event);
                return;
            }
            event->accept();
            onMenu(event->globalPos());
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
                if (index.isValid() && index.row() == pressed && removeRectOf(index).contains(event->position().toPoint())) deferRemove({index});
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
        int pressedRemove = -1;

        static bool isRemoveKey(const QKeyEvent* event) {
            const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeyboardModifiers(Qt::KeypadModifier);
            return (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) && modifiers == Qt::NoModifier;
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

        void deferRemove(const QModelIndexList& indexes) {
            QList<QPersistentModelIndex> targets;
            for (const QModelIndex& index : indexes) targets << QPersistentModelIndex(index);
            QTimer::singleShot(0, this, [this, targets] {
                QList<int> rows;
                for (const auto& target : targets) {
                    if (target.isValid()) rows << target.row();
                }
                if (!rows.isEmpty() && onRemove) onRemove(rows);
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

    // Ctrl+Enter or leaving applies, Esc reverts; neither key reaches the dialog.
    class EntryText : public QPlainTextEdit {
    public:
        explicit EntryText(QWidget* parent) : QPlainTextEdit(parent) {
            setLineWrapMode(QPlainTextEdit::NoWrap);
            setTabChangesFocus(true);
        }

        std::function<void()> onApply;
        std::function<void()> onRevert;
        std::function<void()> onLeave;

    protected:
        bool event(QEvent* event) override {
            if (event->type() == QEvent::ShortcutOverride) {
                const auto* key = static_cast<QKeyEvent*>(event);
                if (isRevertKey(key) || isApplyKey(key)) {
                    event->accept();
                    return true;
                }
            }
            return QPlainTextEdit::event(event);
        }

        void keyPressEvent(QKeyEvent* event) override {
            if (isRevertKey(event) || isApplyKey(event)) {
                event->accept();
                const auto& handler = isRevertKey(event) ? onRevert : onApply;
                if (handler) handler();
                return;
            }
            QPlainTextEdit::keyPressEvent(event);
        }

        void focusOutEvent(QFocusEvent* event) override {
            QPlainTextEdit::focusOutEvent(event);
            if (event->reason() != Qt::PopupFocusReason && event->reason() != Qt::ActiveWindowFocusReason && onLeave) onLeave();
        }

    private:
        static Qt::KeyboardModifiers modifiersOf(const QKeyEvent* event) {
            return event->modifiers() & ~Qt::KeyboardModifiers(Qt::KeypadModifier);
        }

        static bool isRevertKey(const QKeyEvent* event) {
            return event->key() == Qt::Key_Escape && modifiersOf(event) == Qt::NoModifier;
        }

        static bool isApplyKey(const QKeyEvent* event) {
            return (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && modifiersOf(event) == Qt::ControlModifier;
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
    QPushButton* addButton = nullptr;
    QLabel* addError = nullptr;
    QToolButton* textButton = nullptr;
    EntryText* text = nullptr;
    bool textMode = false;
    // The first line the last apply rejected.
    Configs::RuleIssue textIssue;

    void refreshRow() {
        rows = RouteEditor::BuildRows(*profile);
        const int at = RouteEditor::RowOfRule(rows, static_cast<int>(profile->Rules.indexOf(firstRule)));
        if (at >= 0) row = rows.at(at);
    }

    [[nodiscard]] bool allOff() const {
        const int end = qMin(row.first + row.count, static_cast<int>(profile->Rules.size()));
        bool off = row.first < end;
        for (int i = qMax(0, row.first); i < end; ++i) {
            if (!profile->Rules.at(i)->disabled) off = false;
        }
        return off;
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
    d->textButton = new QToolButton(this);
    d->textButton->setText(tr("Edit as text"));
    d->textButton->setAutoRaise(true);
    d->textButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    d->textButton->setCursor(Qt::PointingHandCursor);
    header->addWidget(d->title);
    header->addWidget(d->pill);
    header->addStretch(1);
    header->addWidget(d->textButton);
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
    d->view->onRemove = [this](const QList<int>& rows) { removeEntries(rows); };
    d->view->onEdit = [this](int row) { editEntry(row); };
    d->view->onMenu = [this](const QPoint& pos) { showEntryMenu(pos); };
    layout->addWidget(d->view, 1);

    d->text = new EntryText(this);
    d->text->setPlaceholderText(tr("One entry per line"));
    d->text->onApply = [this] { applyText(true); };
    d->text->onRevert = [this] { closeText(); };
    d->text->onLeave = [this] { applyText(false); };
    d->text->hide();
    layout->addWidget(d->text, 1);
    connect(d->textButton, &QToolButton::clicked, this, [this] {
        if (d->textMode) applyText(true);
        else openText();
    });

    auto* addRow = new QHBoxLayout();
    d->input = new EntryInput(this);
    d->input->setPlaceholderText(tr("example.com, 10.0.0.0/8, app.exe or ruleset:name"));
    d->input->setClearButtonEnabled(true);
    attachCompleter(d->input, completionModel(this));
    d->input->onSubmit = [this] { submit(); };
    d->addButton = new QPushButton(tr("Add"), this);
    d->addButton->setAutoDefault(false);
    connect(d->addButton, &QPushButton::clicked, this, [this] { submit(); });
    addRow->addWidget(d->input, 1);
    addRow->addWidget(d->addButton);
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
        closeText();
        d->addError->clear();
        d->addError->hide();
    }
    reload(same ? d->view->currentIndex().row() : -1, same ? d->view->verticalScrollBar()->value() : 0);
}

QWidget* QuickListEditor::revealEntry(const QuickLists::Entry& entry) {
    if (d->textMode) {
        const QString line = lineOf(entry);
        for (QTextBlock block = d->text->document()->begin(); block.isValid(); block = block.next()) {
            if (block.text().trimmed() != line) continue;
            d->text->setTextCursor(QTextCursor(block));
            break;
        }
        return d->text;
    }
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
    // The pill already says where entries go; only Proxy without exit needs a word on when it differs from Proxy.
    d->intro->setText(list == Configs::warpBypass ? noWarpHint() : QString());
    d->intro->setVisible(list == Configs::warpBypass);

    d->offNote->setVisible(d->allOff());

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

void QuickListEditor::removeEntries(QList<int> rows) {
    const auto& entries = d->model->entries;
    std::sort(rows.begin(), rows.end());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    rows.removeIf([&entries](int row) { return row < 0 || row >= entries.size(); });
    if (rows.isEmpty()) return;
    int current = d->view->currentIndex().row();
    if (current >= 0) current -= static_cast<int>(std::count_if(rows.cbegin(), rows.cend(), [current](int row) { return row < current; }));
    for (const int row : std::as_const(rows)) QuickLists::Remove(entries.at(row));
    reload(current, d->view->verticalScrollBar()->value());
    emit changed();
}

void QuickListEditor::editEntry(int row) {
    if (row < 0 || row >= d->model->entries.size()) return;
    const auto entry = d->model->entries.at(row);
    QuickLists::Remove(entry);
    reload(-1, d->view->verticalScrollBar()->value());
    putLine(d->input, lineOf(entry));
    emit changed();
}

void QuickListEditor::showEntryMenu(const QPoint& globalPos) {
    const bool any = d->view->hasSelection();
    QMenu menu(this);
    QAction* copy = menu.addAction(tr("Copy"));
    copy->setShortcut(QKeySequence::Copy);
    copy->setEnabled(any);
    QAction* copyAll = menu.addAction(tr("Copy all"));
    copyAll->setEnabled(!d->model->entries.isEmpty());
    menu.addSeparator();
    QAction* remove = menu.addAction(tr("Remove"));
    remove->setShortcut(QKeySequence::Delete);
    remove->setEnabled(any);
    const QAction* picked = menu.exec(globalPos);
    if (picked == copy) {
        d->view->copySelected();
    } else if (picked == copyAll) {
        QStringList lines;
        for (const auto& entry : std::as_const(d->model->entries)) lines << lineOf(entry);
        copyLines(lines);
    } else if (picked == remove) {
        d->view->removeSelected();
    }
}

void QuickListEditor::openText() {
    if (d->textMode) return;
    QStringList lines;
    for (const auto& entry : QuickLists::Entries(*d->profile, d->row.first, d->row.count)) lines << lineOf(entry);
    d->text->setPlainText(lines.isEmpty() ? QString() : lines.join(QLatin1Char('\n')) + QLatin1Char('\n'));
    d->text->setExtraSelections({});
    d->textIssue = {};
    d->textMode = true;
    d->addError->clear();
    d->addError->hide();
    d->view->hide();
    d->input->hide();
    d->addButton->hide();
    d->text->show();
    d->textButton->setText(tr("Done"));
    d->text->moveCursor(QTextCursor::End);
    d->text->setFocus(Qt::OtherFocusReason);
}

void QuickListEditor::closeText() {
    if (!d->textMode) return;
    // First, so the focus leaving the box below applies nothing.
    d->textMode = false;
    d->textIssue = {};
    d->view->show();
    d->input->show();
    d->addButton->show();
    if (d->text->hasFocus()) d->view->setFocus(Qt::OtherFocusReason);
    d->text->hide();
    d->text->setExtraSelections({});
    d->text->clear();
    d->textButton->setText(tr("Edit as text"));
    d->addError->clear();
    d->addError->hide();
}

bool QuickListEditor::applyText(bool focus) {
    if (!d->textMode) return false;
    const auto entries = QuickLists::Entries(*d->profile, d->row.first, d->row.count);
    QSet<QString> existing;
    for (const auto& entry : entries) existing.insert(entryKey(entry.prefix, entry.value));
    QSet<QString> keep;
    QStringList fresh;
    QList<int> failedBlocks;
    Configs::RuleIssue issue;
    for (QTextBlock block = d->text->document()->begin(); block.isValid(); block = block.next()) {
        // An entry already in the row passes as written, even one the list flags.
        if (const QString key = lineKey(block.text().trimmed()); existing.contains(key)) {
            keep.insert(key);
            continue;
        }
        for (const QString& line : QuickLists::SplitLines(block.text())) {
            if (const QString key = lineKey(line); existing.contains(key)) {
                keep.insert(key);
                continue;
            }
            QString error;
            const QString canonical = QuickLists::Canonical(line, &error);
            if (canonical.isEmpty()) {
                if (issue.message.isEmpty()) {
                    issue.value = line;
                    issue.message = error;
                }
                if (failedBlocks.isEmpty() || failedBlocks.last() != block.blockNumber()) failedBlocks << block.blockNumber();
                continue;
            }
            const QString key = lineKey(canonical);
            if (keep.contains(key)) continue;
            keep.insert(key);
            if (!existing.contains(key)) fresh << canonical;
        }
    }

    if (!failedBlocks.isEmpty()) {
        d->textIssue = issue;
        d->addError->setText(QStringLiteral("\"%1\": %2").arg(RouteEditor::ElideValue(issue.value), issue.message));
        d->addError->show();
        QColor tint = themeManager()->tokens.danger;
        tint.setAlphaF(0.18f);
        QList<QTextEdit::ExtraSelection> marks;
        for (const int number : std::as_const(failedBlocks)) {
            QTextEdit::ExtraSelection mark;
            mark.format.setBackground(tint);
            mark.format.setProperty(QTextFormat::FullWidthSelection, true);
            mark.cursor = QTextCursor(d->text->document()->findBlockByNumber(number));
            marks << mark;
        }
        d->text->setExtraSelections(marks);
        revealPending();
        if (focus) d->text->setFocus(Qt::OtherFocusReason);
        return false;
    }

    bool edited = false;
    for (const auto& entry : entries) {
        if (keep.contains(entryKey(entry.prefix, entry.value))) continue;
        QuickLists::Remove(entry);
        edited = true;
    }
    for (const QString& line : std::as_const(fresh)) {
        // Fails only for a value another row of this list already holds.
        if (!QuickLists::Add(*d->profile, d->row, line, nullptr)) continue;
        d->refreshRow();
        edited = true;
    }
    closeText();
    if (edited) {
        reload(-1, 0);
        emit changed();
    }
    return true;
}

Configs::RuleIssue QuickListEditor::pendingIssue() const {
    if (!d->textMode || d->textIssue.message.isEmpty()) return {};
    Configs::RuleIssue issue = d->textIssue;
    // A turned-off list never blocks saving.
    issue.severity = d->allOff() ? Validation::Severity::Warning : Validation::Severity::Error;
    return issue;
}

QWidget* QuickListEditor::revealPending() {
    if (!d->textMode) return d->view;
    if (const auto marks = d->text->extraSelections(); !marks.isEmpty()) {
        QTextCursor cursor = marks.first().cursor;
        cursor.clearSelection();
        cursor.movePosition(QTextCursor::EndOfBlock);
        d->text->setTextCursor(cursor);
    }
    return d->text;
}
