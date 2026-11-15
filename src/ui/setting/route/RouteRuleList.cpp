#include "include/ui/setting/route/RouteRuleList.h"

#include "include/database/ProfilesRepo.h"
#include "include/global/Configs.hpp"
#include "include/ui/setting/ThemeManager.hpp"
#include "include/ui/setting/route/RouteQuickLists.h"

#include <QAbstractListModel>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDrag>
#include <QHash>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTextDocument>

#include <utility>

namespace {
    constexpr int ruleListPad = 10;
    constexpr int ruleListGripWidth = 8;
    constexpr int ruleListGap = 8;
    constexpr int ruleListVPad = 7;
    constexpr int ruleListDotSize = 7;
    constexpr int ruleListTagPad = 5;
    constexpr int ruleListPillGap = 10;
    constexpr char ruleListMime[] = "application/x-throne-route-row";

    QFont ruleListSmallFont(const QFont& base) {
        QFont font = base;
        if (font.pointSizeF() > 0) font.setPointSizeF(font.pointSizeF() * 0.9);
        else if (font.pixelSize() > 0) font.setPixelSize(qMax(8, qRound(font.pixelSize() * 0.9)));
        return font;
    }

    QFont ruleListBoldSmallFont(const QFont& base) {
        QFont font = ruleListSmallFont(base);
        font.setWeight(QFont::DemiBold);
        return font;
    }

    int ruleListBoxWidth(const QStyle* style, const QWidget* widget) {
        return qMax(style->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, widget), 14) + 4;
    }

    // Wide enough for the highest row number, at least two digits.
    int ruleListNumberWidth(const QFontMetrics& small, int rows) {
        int digits = 2;
        for (int n = rows; n >= 100; n /= 10) ++digits;
        return small.horizontalAdvance(QString(digits, QLatin1Char('8')));
    }

    // Left to right; QStyle::visualRect mirrors them for right-to-left.
    struct RuleListColumns {
        QRect grip;
        QRect box;
        QRect number;
        QRect text;
        int right = 0;
    };

    RuleListColumns ruleListColumns(const QRect& rect, int boxWidth, int numberWidth) {
        RuleListColumns columns;
        int x = rect.left() + ruleListPad;
        columns.grip = QRect(x, rect.top(), ruleListGripWidth, rect.height());
        x += ruleListGripWidth + ruleListGap;
        columns.box = QRect(x, rect.top(), boxWidth, rect.height());
        x += boxWidth + ruleListGap - 2;
        columns.number = QRect(x, rect.top(), numberWidth, rect.height());
        x += numberWidth + ruleListGap;
        columns.right = rect.right() - ruleListPad;
        columns.text = QRect(x, rect.top(), qMax(0, columns.right - x + 1), rect.height());
        return columns;
    }

    void ruleListPaintLink(QPainter* painter, const QPointF& center, const QColor& color) {
        painter->save();
        painter->translate(center);
        painter->rotate(-45);
        painter->setPen(QPen(color, 1.4));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(QRectF(-6.5, -2.4, 7.5, 4.8), 2.4, 2.4);
        painter->drawRoundedRect(QRectF(-1.0, -2.4, 7.5, 4.8), 2.4, 2.4);
        painter->restore();
    }

    QString ruleListEndpointSummary(const Configs::RouteProfile& profile, int endpointId) {
        if (const int owner = RouteEditor::InnerHopOwner(profile, endpointId); owner >= 0)
            return RouteRuleList::tr("Inner hop of %1, from the Endpoints tab").arg(RouteEditor::ProfileName(owner));
        const auto ent = Configs::dataManager->profilesRepo->GetProfile(endpointId);
        if (ent == nullptr || ent->outbound == nullptr) return RouteRuleList::tr("From the Endpoints tab");
        return RouteRuleList::tr("%1 endpoint, from the Endpoints tab").arg(ent->outbound->DisplayType());
    }

    QString ruleListPointerKey(const void* pointer) {
        return QString::number(static_cast<qulonglong>(reinterpret_cast<quintptr>(pointer)), 16);
    }
} // namespace

struct RouteRuleListLook {
    QString title;
    QString tag;
    QString summary;
    QString pill;
    QString issue;
    // What the filter searches: title, summary and every value.
    QString haystack;
    RouteEditor::Tone tone = RouteEditor::Tone::Neutral;
    bool error = false;
    bool dimmed = false;
    bool endpoint = false;
    bool checkable = false;
    bool checked = false;
    bool everythingElse = false;
};

// One entry per row plus "Everything else"; each row's look is computed when the row changes, never while painting.
class RouteRuleListModel : public QAbstractListModel {
public:
    RouteRuleListModel(std::shared_ptr<Configs::RouteProfile> routeProfile, QObject* parent)
        : QAbstractListModel(parent), profile(std::move(routeProfile)) {
        finalLook.everythingElse = true;
        finalLook.title = RouteRuleList::tr("Everything else");
        finalLook.summary = RouteRuleList::tr("The default route. Always last, set above the list.");
        setFinalRoute(profile->defaultOutboundID);
    }

    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : static_cast<int>(rowList.size()) + 1;
    }

    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() > rowList.size()) return {};
        const RouteRuleListLook& look = lookOf(index.row());
        switch (role) {
            case Qt::DisplayRole:
            case Qt::AccessibleTextRole: return look.title;
            case Qt::AccessibleDescriptionRole: return look.summary + QStringLiteral(" · ") + look.pill;
            case Qt::ToolTipRole: {
                const QString tip = look.everythingElse ? RouteRuleList::tr("Set above the list, under Everything else goes to.") : look.issue;
                return tip.isEmpty() ? QVariant() : QVariant(Qt::convertFromPlainText(tip));
            }
            case Qt::CheckStateRole:
                return look.checkable ? QVariant(look.checked ? Qt::Checked : Qt::Unchecked) : QVariant();
            default: return {};
        }
    }

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex& index) const override {
        if (!index.isValid()) return Qt::NoItemFlags;
        if (index.row() >= rowList.size()) return Qt::ItemIsEnabled;
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }

    [[nodiscard]] const QList<RouteEditor::Row>& rows() const { return rowList; }

    [[nodiscard]] const RouteRuleListLook& lookOf(int row) const {
        return row >= 0 && row < looks.size() ? looks.at(row) : finalLook;
    }

    [[nodiscard]] bool matches(int row, const QString& text) const {
        return row < 0 || row >= looks.size() || looks.at(row).haystack.contains(text, Qt::CaseInsensitive);
    }

    void rebuild(bool recompute) {
        QHash<QString, RouteRuleListLook> previous;
        if (!recompute) {
            for (int i = 0; i < rowList.size() && i < looks.size(); ++i) previous.insert(keyOf(rowList.at(i)), looks.at(i));
        }
        beginResetModel();
        rowList = RouteEditor::BuildRows(*profile);
        looks.clear();
        looks.reserve(rowList.size());
        for (const auto& row : rowList) {
            const auto it = previous.constFind(keyOf(row));
            looks.append(it != previous.cend() ? it.value() : computeLook(row));
        }
        endResetModel();
    }

    void refreshRow(int row) {
        if (row < 0 || row >= rowList.size()) return;
        looks[row] = computeLook(rowList.at(row));
        emit dataChanged(index(row), index(row));
    }

    void setFinalRoute(int outboundID) {
        const RouteEditor::Then then = RouteEditor::ThenOfOutbound(outboundID);
        finalLook.pill = RouteEditor::ThenLabel(then);
        finalLook.tone = RouteEditor::ToneOf(then);
        const int last = static_cast<int>(rowList.size());
        emit dataChanged(index(last), index(last));
    }

private:
    [[nodiscard]] QString keyOf(const RouteEditor::Row& row) const {
        const auto& rules = profile->Rules;
        if (row.first < 0 || row.count < 1 || row.first + row.count > rules.size()) return {};
        return QStringLiteral("%1:%2:%3:%4")
            .arg(static_cast<int>(row.kind))
            .arg(ruleListPointerKey(rules.at(row.first).get()), ruleListPointerKey(rules.at(row.first + row.count - 1).get()))
            .arg(row.count);
    }

    [[nodiscard]] RouteRuleListLook computeLook(const RouteEditor::Row& row) const {
        RouteRuleListLook look;
        const auto& rules = profile->Rules;
        if (row.first < 0 || row.count < 1 || row.first + row.count > rules.size()) return look;
        look.title = RouteEditor::RowTitle(*profile, row);
        QStringList hay = {look.title};
        switch (row.kind) {
            case RouteEditor::RowKind::QuickList: {
                const auto entries = QuickLists::Entries(*profile, row.first, row.count);
                bool anyOn = false;
                for (int i = row.first; i < row.first + row.count; ++i) anyOn = anyOn || !rules.at(i)->disabled;
                const RouteEditor::Then then = RouteEditor::ThenOfList(row.list);
                look.tag = RouteRuleList::tr("Quick list");
                look.summary = QuickLists::Summary(entries);
                if (!anyOn) look.summary = RouteRuleList::tr("Off · %1").arg(look.summary);
                look.pill = RouteEditor::ThenLabel(then);
                look.tone = RouteEditor::ToneOf(then);
                look.dimmed = entries.isEmpty() || !anyOn;
                look.checkable = true;
                look.checked = anyOn;
                for (const auto& entry : entries) {
                    hay << entry.value;
                    if (!look.error && !entry.issue.message.isEmpty() && entry.issue.severity == Validation::Severity::Error) {
                        look.error = true;
                        look.issue = RouteRuleList::tr("Entry \"%1\": %2").arg(entry.value, entry.issue.message);
                    }
                }
                break;
            }
            case RouteEditor::RowKind::Endpoint: {
                const auto& rule = rules.at(row.first);
                look.tag = RouteRuleList::tr("Endpoint");
                look.endpoint = true;
                look.summary = ruleListEndpointSummary(*profile, rule->outboundID);
                look.pill = RouteEditor::ThenLabel(*rule);
                look.tone = RouteEditor::ToneOf(*rule);
                hay << look.pill;
                break;
            }
            case RouteEditor::RowKind::Rule: {
                const auto& rule = rules.at(row.first);
                look.summary = RouteEditor::RuleSummary(*rule);
                if (rule->disabled) look.summary = RouteRuleList::tr("Off · %1").arg(look.summary);
                look.pill = RouteEditor::ThenLabel(*rule);
                look.tone = RouteEditor::ToneOf(*rule);
                look.dimmed = rule->disabled;
                look.checkable = true;
                look.checked = !rule->disabled;
                for (const auto& issue : RouteEditor::RuleProblems(*rule)) {
                    if (issue.severity != Validation::Severity::Error) continue;
                    look.error = true;
                    look.issue = issue.message;
                    break;
                }
                for (const QString& attr : RouteEditor::SetConditions(*rule)) hay << rule->get_current_value_string(attr);
                hay << look.pill;
                break;
            }
        }
        hay << look.summary;
        look.haystack = hay.join(QLatin1Char('\n'));
        return look;
    }

    std::shared_ptr<Configs::RouteProfile> profile;
    QList<RouteEditor::Row> rowList;
    QList<RouteRuleListLook> looks;
    RouteRuleListLook finalLook;
};

namespace {
    class RuleListDelegate : public QStyledItemDelegate {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;

        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
            const auto* model = static_cast<const RouteRuleListModel*>(index.model());
            const RouteRuleListLook& look = model->lookOf(index.row());
            const auto& tokens = themeManager()->tokens;
            const QWidget* widget = option.widget;
            const QStyle* style = widget != nullptr ? widget->style() : QApplication::style();
            const auto visual = [&option](const QRect& rect) { return QStyle::visualRect(option.direction, option.rect, rect); };
            const Qt::Alignment leftAlign = QStyle::visualAlignment(option.direction, Qt::AlignLeft | Qt::AlignVCenter);

            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            QColor background;
            if (look.everythingElse) background = RouteEditor::Mix(tokens.onSurface, tokens.surface, 0.045);
            else if (option.state & QStyle::State_Selected)
                background = RouteEditor::Mix(tokens.accent, tokens.surface, (option.state & QStyle::State_HasFocus) ? 0.2 : 0.14);
            else if (option.state & QStyle::State_MouseOver) background = RouteEditor::Mix(tokens.onSurface, tokens.surface, 0.035);
            if (background.isValid()) painter->fillRect(option.rect, background);
            painter->setPen(QPen(RouteEditor::Mix(tokens.onSurface, tokens.surface, 0.1), 1));
            painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());

            const QFont smallFont = ruleListSmallFont(option.font);
            const QFontMetrics small(smallFont);
            const RuleListColumns columns = ruleListColumns(option.rect, ruleListBoxWidth(style, widget), ruleListNumberWidth(small, model->rowCount() - 1));
            const QColor faint = RouteEditor::Mix(tokens.muted, tokens.surface, 0.75);

            if (!look.everythingElse) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(faint);
                const QPointF grip = QRectF(visual(columns.grip)).center();
                for (const qreal dx : {-2.0, 2.0}) {
                    for (const qreal dy : {-4.0, 0.0, 4.0}) painter->drawEllipse(grip + QPointF(dx, dy), 1.2, 1.2);
                }
                if (look.checkable) {
                    QStyleOptionViewItem check(option);
                    const int width = style->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, widget);
                    const int height = style->pixelMetric(QStyle::PM_IndicatorHeight, nullptr, widget);
                    check.rect = visual(QRect(columns.box.left() + (columns.box.width() - width) / 2, columns.box.center().y() - height / 2 + 1, width, height));
                    check.state = (option.state & QStyle::State_Enabled) | (look.checked ? QStyle::State_On : QStyle::State_Off);
                    style->drawPrimitive(QStyle::PE_IndicatorItemViewItemCheck, &check, painter, widget);
                } else if (look.endpoint) {
                    ruleListPaintLink(painter, QRectF(visual(columns.box)).center(), tokens.muted);
                }
                painter->setFont(smallFont);
                painter->setPen(faint);
                painter->drawText(visual(columns.number), leftAlign, QString::number(index.row() + 1));
            }

            if (look.dimmed) painter->setOpacity(0.5);
            const QFont pillFont = ruleListBoldSmallFont(option.font);
            const QFontMetrics pillMetrics(pillFont);
            QSize pill = RouteEditor::PillSize(pillMetrics, look.pill);
            pill.setWidth(qMin(pill.width(), qMax(pillMetrics.averageCharWidth() * 8, columns.text.width() * 2 / 5)));
            const QRect pillRect(columns.right - pill.width() + 1, option.rect.center().y() - pill.height() / 2, pill.width(), pill.height());
            painter->setFont(pillFont);
            RouteEditor::PaintPill(painter, visual(pillRect), look.pill, look.tone);

            const QRect text(columns.text.left(), option.rect.top(), qMax(0, pillRect.left() - ruleListPillGap - columns.text.left()), option.rect.height());
            QFont titleFont = option.font;
            if (look.endpoint) titleFont.setItalic(true);
            if (look.everythingElse) titleFont.setWeight(QFont::DemiBold);
            const QFontMetrics titleMetrics(titleFont);
            const int top = option.rect.top() + (option.rect.height() - (titleMetrics.height() + 2 + small.height())) / 2;
            int x = text.left();
            if (look.error) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(tokens.danger);
                painter->drawEllipse(visual(QRect(x, top + (titleMetrics.height() - ruleListDotSize) / 2, ruleListDotSize, ruleListDotSize)));
                x += ruleListDotSize + 6;
            }
            const QFont tagFont = ruleListBoldSmallFont(option.font);
            const QFontMetrics tagMetrics(tagFont);
            const int tagWidth = look.tag.isEmpty() ? 0 : tagMetrics.horizontalAdvance(look.tag) + 2 * ruleListTagPad;
            const int titleSpace = qMax(0, text.right() + 1 - x - (tagWidth > 0 ? tagWidth + 7 : 0));
            const QString title = titleMetrics.elidedText(look.title, Qt::ElideRight, titleSpace);
            painter->setFont(titleFont);
            painter->setPen(tokens.onSurface);
            painter->drawText(visual(QRect(x, top, titleSpace, titleMetrics.height())), leftAlign, title);
            if (tagWidth > 0 && titleSpace > 0) {
                const int tagLeft = x + qMin(titleSpace, titleMetrics.horizontalAdvance(title)) + 7;
                const QRect tag = visual(QRect(tagLeft, top + (titleMetrics.height() - tagMetrics.height() - 2) / 2, tagWidth, tagMetrics.height() + 2));
                painter->setPen(QPen(RouteEditor::Mix(tokens.onSurface, tokens.surface, 0.25), 1));
                painter->setBrush(Qt::NoBrush);
                painter->drawRoundedRect(QRectF(tag).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
                painter->setFont(tagFont);
                painter->setPen(tokens.muted);
                painter->drawText(tag, Qt::AlignCenter, look.tag);
            }
            const QRect summary(text.left(), top + titleMetrics.height() + 2, text.width(), small.height());
            painter->setFont(smallFont);
            painter->setPen(look.error ? tokens.danger : tokens.muted);
            painter->drawText(visual(summary), leftAlign, small.elidedText(look.summary, Qt::ElideRight, summary.width()));
            painter->restore();
        }

        [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const override {
            const QFontMetrics metrics(option.font);
            const QFontMetrics small(ruleListSmallFont(option.font));
            const int content = qMax(metrics.height() + 2 + small.height(), QFontMetrics(ruleListBoldSmallFont(option.font)).height() + 4);
            // No width: the list makes every row as wide as its viewport.
            return {0, content + 2 * ruleListVPad + 1};
        }

        bool editorEvent(QEvent*, QAbstractItemModel*, const QStyleOptionViewItem&, const QModelIndex&) override {
            return false;
        }
    };

    // The "#, Rule, Then" line over the rows, aligned with the delegate's columns.
    class RuleListHeader : public QWidget {
    public:
        explicit RuleListHeader(RouteRuleList* owner) : QWidget(owner), list(owner) {}

    protected:
        void paintEvent(QPaintEvent*) override {
            const auto& tokens = themeManager()->tokens;
            QPainter painter(this);
            painter.fillRect(rect(), palette().color(QPalette::Base));
            painter.setPen(QPen(RouteEditor::Mix(tokens.onSurface, tokens.surface, 0.1), 1));
            painter.drawLine(rect().bottomLeft(), rect().bottomRight());

            QFont font = ruleListBoldSmallFont(list->font());
            font.setCapitalization(QFont::AllUppercase);
            const QFontMetrics small(ruleListSmallFont(list->font()));
            const int rows = list->model() != nullptr ? list->model()->rowCount() - 1 : 0;
            const RuleListColumns columns = ruleListColumns(rect(), ruleListBoxWidth(list->style(), list), ruleListNumberWidth(small, rows));
            const auto visual = [this](const QRect& r) { return QStyle::visualRect(layoutDirection(), rect(), r); };
            const Qt::Alignment leftAlign = QStyle::visualAlignment(layoutDirection(), Qt::AlignLeft | Qt::AlignVCenter);
            const Qt::Alignment rightAlign = QStyle::visualAlignment(layoutDirection(), Qt::AlignRight | Qt::AlignVCenter);
            painter.setFont(font);
            painter.setPen(RouteEditor::Mix(tokens.muted, tokens.surface, 0.8));
            painter.drawText(visual(QRect(columns.number.left(), 0, columns.number.width() + ruleListGap, height())), leftAlign, RouteRuleList::tr("#"));
            const QString then = RouteRuleList::tr("Then");
            const int thenWidth = QFontMetrics(font).horizontalAdvance(then);
            painter.drawText(visual(QRect(columns.right - thenWidth - 1, 0, thenWidth + 2, height())), rightAlign, then);
            const int ruleWidth = qMax(0, columns.right - thenWidth - ruleListPillGap - columns.text.left());
            painter.drawText(visual(QRect(columns.text.left(), 0, ruleWidth, height())), leftAlign,
                             QFontMetrics(font).elidedText(RouteRuleList::tr("Rule"), Qt::ElideRight, ruleWidth));
        }

    private:
        RouteRuleList* list;
    };
} // namespace

RouteRuleList::RouteRuleList(std::shared_ptr<Configs::RouteProfile> profile, QWidget* parent)
    : QListView(parent), rowModel(new RouteRuleListModel(std::move(profile), this)), headerLine(new RuleListHeader(this)) {
    setModel(rowModel);
    setItemDelegate(new RuleListDelegate(this));
    setUniformItemSizes(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
    setDropIndicatorShown(false);
    setAutoScroll(true);
    setAccessibleName(tr("Rules"));
    rowModel->rebuild(true);
    placeHeader();
    connect(themeManager(), &ThemeManager::themeChanged, this, [this] {
        viewport()->update();
        headerLine->update();
    });
}

RouteRuleList::~RouteRuleList() = default;

void RouteRuleList::rebuild(bool recompute) {
    rowModel->rebuild(recompute);
    for (int row = 0; row < rows().size(); ++row) applyFilter(row);
    headerLine->update();
}

const QList<RouteEditor::Row>& RouteRuleList::rows() const {
    return rowModel->rows();
}

void RouteRuleList::refreshRow(int row) {
    rowModel->refreshRow(row);
    applyFilter(row);
}

void RouteRuleList::setDefaultOutbound(int outboundID) {
    rowModel->setFinalRoute(outboundID);
}

void RouteRuleList::setFilter(const QString& text) {
    const QString trimmed = text.trimmed();
    if (trimmed == filter) return;
    filter = trimmed;
    for (int row = 0; row < rows().size(); ++row) applyFilter(row);
}

bool RouteRuleList::isRowShown(int row) const {
    return row >= 0 && row < rows().size() && !isRowHidden(row);
}

void RouteRuleList::applyFilter(int row) {
    if (row < 0 || row >= rows().size()) return;
    setRowHidden(row, !filter.isEmpty() && !rowModel->matches(row, filter));
}

int RouteRuleList::selectedRow() const {
    const QModelIndex index = currentIndex();
    return index.isValid() && index.row() < rows().size() ? index.row() : -1;
}

void RouteRuleList::setSelectedRow(int row) {
    if (row < 0 || row >= rows().size()) {
        if (currentIndex().isValid()) selectionModel()->clear();
        return;
    }
    const QModelIndex index = rowModel->index(row);
    selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
    scrollTo(index);
}

void RouteRuleList::currentChanged(const QModelIndex& current, const QModelIndex& previous) {
    QListView::currentChanged(current, previous);
    const int row = current.isValid() && current.row() < rows().size() ? current.row() : -1;
    // Focus can move the current row without selecting it; the page edits the current row, so show it selected.
    if (row >= 0 && !selectionModel()->isSelected(current)) selectionModel()->select(current, QItemSelectionModel::ClearAndSelect);
    emit selectedRowChanged(row);
}

QModelIndex RouteRuleList::moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers) {
    const QModelIndex next = QListView::moveCursor(cursorAction, modifiers);
    const int count = static_cast<int>(rows().size());
    if (!next.isValid() || next.row() < count) return next;
    for (int row = count - 1; row >= 0; --row) {
        if (!isRowHidden(row)) return rowModel->index(row);
    }
    return currentIndex();
}

void RouteRuleList::keyPressEvent(QKeyEvent* event) {
    const int row = selectedRow();
    const int key = event->key();
    if (key == Qt::Key_Delete && event->modifiers() == Qt::NoModifier) {
        if (row >= 0) QMetaObject::invokeMethod(this, [this] { emit deleteRequested(); }, Qt::QueuedConnection);
        event->accept();
        return;
    }
    if (key == Qt::Key_Space && event->modifiers() == Qt::NoModifier && row >= 0) {
        if (rowModel->lookOf(row).checkable) QMetaObject::invokeMethod(this, [this, row] { emit toggleRequested(row); }, Qt::QueuedConnection);
        event->accept();
        return;
    }
    if ((key == Qt::Key_Up || key == Qt::Key_Down) && event->modifiers() == Qt::AltModifier) {
        const int to = row + (key == Qt::Key_Up ? -1 : 1);
        if (row >= 0 && filter.isEmpty() && to >= 0 && to < rows().size()) requestReorder(row, to);
        event->accept();
        return;
    }
    QListView::keyPressEvent(event);
}

bool RouteRuleList::onCheckBox(int row, const QPoint& pos) const {
    if (!rowModel->lookOf(row).checkable) return false;
    const QRect rect = visualRect(rowModel->index(row));
    const QFontMetrics small(ruleListSmallFont(font()));
    const RuleListColumns columns = ruleListColumns(rect, ruleListBoxWidth(style(), this), ruleListNumberWidth(small, rowModel->rowCount() - 1));
    return QStyle::visualRect(layoutDirection(), rect, columns.box).contains(pos);
}

void RouteRuleList::mousePressEvent(QMouseEvent* event) {
    pressRow = -1;
    const QPoint pos = event->position().toPoint();
    const QModelIndex index = indexAt(pos);
    // "Everything else" and the empty space below it neither select nor clear the selection.
    if (!index.isValid() || index.row() >= rows().size()) {
        setFocus(Qt::MouseFocusReason);
        event->accept();
        return;
    }
    const int row = index.row();
    if (event->button() == Qt::LeftButton && onCheckBox(row, pos)) {
        setFocus(Qt::MouseFocusReason);
        QMetaObject::invokeMethod(this, [this, row] { emit toggleRequested(row); }, Qt::QueuedConnection);
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        pressRow = row;
        pressPos = pos;
    }
    QListView::mousePressEvent(event);
}

void RouteRuleList::mouseMoveEvent(QMouseEvent* event) {
    if (pressRow >= 0 && (event->buttons() & Qt::LeftButton)) {
        // Below the drag distance the press stays a click, so the selection does not follow the mouse.
        if (filter.isEmpty() && (event->position().toPoint() - pressPos).manhattanLength() >= QApplication::startDragDistance()) {
            const int row = pressRow;
            pressRow = -1;
            startRowDrag(row);
        }
        event->accept();
        return;
    }
    QListView::mouseMoveEvent(event);
}

void RouteRuleList::mouseReleaseEvent(QMouseEvent* event) {
    pressRow = -1;
    QListView::mouseReleaseEvent(event);
}

void RouteRuleList::contextMenuEvent(QContextMenuEvent* event) {
    int row = -1;
    QPoint global = event->globalPos();
    if (event->reason() == QContextMenuEvent::Keyboard) {
        row = selectedRow();
        if (row >= 0) global = viewport()->mapToGlobal(visualRect(rowModel->index(row)).center());
    } else {
        const QModelIndex index = indexAt(event->pos());
        if (index.isValid() && index.row() < rows().size()) row = index.row();
    }
    if (row < 0) {
        event->ignore();
        return;
    }
    if (row != selectedRow()) setSelectedRow(row);
    QMetaObject::invokeMethod(this, [this, row, global] { emit menuRequested(row, global); }, Qt::QueuedConnection);
    event->accept();
}

QPixmap RouteRuleList::rowPixmap(int row) const {
    const QModelIndex index = rowModel->index(row);
    const QSize size = visualRect(index).size();
    const qreal ratio = devicePixelRatioF();
    QPixmap pixmap(size * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.rect = QRect(QPoint(0, 0), size);
    option.state |= QStyle::State_Selected;
    option.state &= ~QStyle::State_MouseOver;
    painter.setOpacity(0.9);
    painter.fillRect(option.rect, themeManager()->tokens.surface);
    itemDelegate()->paint(&painter, option, index);
    return pixmap;
}

void RouteRuleList::startRowDrag(int row) {
    auto* mime = new QMimeData;
    mime->setData(QString::fromLatin1(ruleListMime), QByteArray::number(row));
    auto* drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->setPixmap(rowPixmap(row));
    drag->setHotSpot(pressPos - visualRect(rowModel->index(row)).topLeft());
    dragRow = row;
    droppedTo = -1;
    drag->exec(Qt::MoveAction, Qt::MoveAction);
    dragRow = -1;
    dropGap = -1;
    stopAutoScroll();
    setState(NoState);
    viewport()->update();
    if (droppedTo >= 0) requestReorder(row, droppedTo);
    droppedTo = -1;
}

// 0..rows: the gap above that row; rows is the gap above "Everything else", the lowest one.
int RouteRuleList::dropGapAt(const QPoint& pos) const {
    const int count = static_cast<int>(rows().size());
    if (count == 0) return 0;
    const QModelIndex index = indexAt(pos);
    if (!index.isValid()) return pos.y() < visualRect(rowModel->index(0)).top() ? 0 : count;
    if (index.row() >= count) return count;
    const QRect rect = visualRect(index);
    return pos.y() < rect.center().y() ? index.row() : index.row() + 1;
}

int RouteRuleList::dropIndicatorY(int gap) const {
    const int count = static_cast<int>(rows().size());
    if (count == 0) return 0;
    if (gap < count) return visualRect(rowModel->index(gap)).top();
    return visualRect(rowModel->index(count - 1)).bottom() + 1;
}

void RouteRuleList::dragEnterEvent(QDragEnterEvent* event) {
    if (event->source() != this || dragRow < 0 || !event->mimeData()->hasFormat(QString::fromLatin1(ruleListMime))) {
        event->ignore();
        return;
    }
    setState(DraggingState);
    event->setDropAction(Qt::MoveAction);
    event->accept();
}

void RouteRuleList::dragMoveEvent(QDragMoveEvent* event) {
    if (event->source() != this || dragRow < 0) {
        event->ignore();
        return;
    }
    // The base class finds no drop target in this model; it only scrolls near the edges.
    QListView::dragMoveEvent(event);
    const int gap = dropGapAt(event->position().toPoint());
    if (gap != dropGap) {
        dropGap = gap;
        viewport()->update();
    }
    event->setDropAction(Qt::MoveAction);
    event->accept();
}

void RouteRuleList::dragLeaveEvent(QDragLeaveEvent* event) {
    QListView::dragLeaveEvent(event);
    dropGap = -1;
    viewport()->update();
}

void RouteRuleList::dropEvent(QDropEvent* event) {
    stopAutoScroll();
    setState(NoState);
    const int gap = dropGapAt(event->position().toPoint());
    dropGap = -1;
    viewport()->update();
    if (event->source() != this || dragRow < 0) {
        event->ignore();
        return;
    }
    // Not the base class: it would hand the drop to the model, and a move there deletes the source row.
    const int to = gap > dragRow ? gap - 1 : gap;
    droppedTo = to != dragRow ? to : -1;
    event->setDropAction(Qt::MoveAction);
    event->accept();
}

void RouteRuleList::paintEvent(QPaintEvent* event) {
    QListView::paintEvent(event);
    if (dropGap < 0 || rows().isEmpty()) return;
    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(themeManager()->tokens.accent, 2, Qt::SolidLine, Qt::RoundCap));
    const qreal y = dropIndicatorY(dropGap);
    painter.drawLine(QPointF(ruleListPad / 2.0, y), QPointF(viewport()->width() - ruleListPad / 2.0, y));
}

void RouteRuleList::changeEvent(QEvent* event) {
    QListView::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
        scheduleDelayedItemsLayout();
        placeHeader();
        headerLine->update();
    }
}

bool RouteRuleList::viewportEvent(QEvent* event) {
    if (event->type() == QEvent::Resize) placeHeader();
    return QListView::viewportEvent(event);
}

void RouteRuleList::placeHeader() {
    const int height = QFontMetrics(ruleListBoldSmallFont(font())).height() + 10;
    if (viewportMargins().top() != height) setViewportMargins(0, height, 0, 0);
    const QRect area = viewport()->geometry();
    headerLine->setGeometry(area.left(), area.top() - height, area.width(), height);
}

void RouteRuleList::requestReorder(int from, int to) {
    QMetaObject::invokeMethod(this, [this, from, to] { emit reorderRequested(from, to); }, Qt::QueuedConnection);
}
