#include "include/ui/stats/dialog_endpoint_details.h"

#include <QBrush>
#include <QClipboard>
#include <QDateTime>
#include <QGuiApplication>
#include <QHeaderView>
#include <QScreen>
#include <QShortcut>
#include <QTableWidget>
#include <QTimer>

#include <algorithm>

namespace Stats {
    QColor VpnStateColor(const QString &state) {
        if (state == QStringLiteral("connected")) return kStatsHealthyColor;
        if (state == QStringLiteral("error")) return kStatsProblemColor;
        return kStatsAccentColor;
    }
} // namespace Stats

DialogEndpointDetails::DialogEndpointDetails(const Stats::VpnEndpointView &view, QWidget *parent)
    : QDialog(parent), ui(new Ui::DialogEndpointDetails) {
    ui->setupUi(this);

    auto *table = ui->table;
    table->setColumnCount(2);
    table->horizontalHeader()->setVisible(false);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    // Wrapped rows differ wildly in height, so item-granular scrolling would jump.
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    auto *copy = new QShortcut(QKeySequence::Copy, table);
    copy->setContext(Qt::WidgetWithChildrenShortcut);
    connect(copy, &QShortcut::activated, this, [this]() { copySelection(); });

    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    applyStatus(view);

    const auto *scr = screen() != nullptr ? screen() : QGuiApplication::primaryScreen();
    if (scr != nullptr) resize(size().expandedTo(sizeHint()).boundedTo(scr->availableGeometry().size()));
}

DialogEndpointDetails::~DialogEndpointDetails() {
    delete ui;
}

void DialogEndpointDetails::setRows(const QList<DetailRow> &rows) {
    auto *table = ui->table;

    bool sameShape = rows.size() == rows_.size();
    for (qsizetype row = 0; sameShape && row < rows.size(); row++) {
        sameShape = rows[row].kind == rows_[row].kind && rows[row].key == rows_[row].key &&
                    rows[row].label == rows_[row].label;
    }

    // Only the shape change rebuilds; a value-only refill would drop the user's scroll position.
    if (!sameShape) {
        table->clearSpans();
        table->setRowCount(static_cast<int>(rows.size()));
        for (int row = 0; row < static_cast<int>(rows.size()); row++) {
            for (int column = 0; column < 2; column++) {
                if (table->item(row, column) == nullptr) table->setItem(row, column, new QTableWidgetItem());
                table->item(row, column)->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
            }
            auto *label = table->item(row, 0);
            const bool isSection = rows[row].kind == DetailRow::Section;
            label->setText(rows[row].kind == DetailRow::Value ? QString() : rows[row].label);
            auto font = table->font();
            font.setBold(isSection);
            label->setFont(font);
            if (isSection) table->setSpan(row, 0, 1, 2);
        }
    }

    bool valuesChanged = false;
    for (int row = 0; row < static_cast<int>(rows.size()); row++) {
        auto *value = table->item(row, 1);
        if (value->text() != rows[row].value) valuesChanged = true;
        value->setText(rows[row].value);
        value->setForeground(rows[row].color.isValid() ? QBrush(rows[row].color) : QBrush());
    }

    rows_ = rows;
    if (!sameShape) fitRowHeights();
    if (!sameShape || valuesChanged) queueRowFit();
}

void DialogEndpointDetails::fitRowHeights() {
    auto *table = ui->table;

    // Measured off the field labels alone: ResizeToContents would also measure the spanned headings.
    int width = 0;
    for (const auto &row : rows_) {
        if (row.kind != DetailRow::Field) continue;
        width = std::max(width, table->fontMetrics().horizontalAdvance(row.label));
    }
    table->horizontalHeader()->resizeSection(
        0, std::max(80, std::min(width + 24, table->viewport()->width() / 2)));
    table->resizeRowsToContents();
}

// Wrapping depends on the stretched value column, which only settles after the pending layout pass.
void DialogEndpointDetails::queueRowFit() {
    if (rowFitQueued_) return;
    rowFitQueued_ = true;
    QTimer::singleShot(0, this, [this]() {
        rowFitQueued_ = false;
        fitRowHeights();
    });
}

void DialogEndpointDetails::resizeEvent(QResizeEvent *event) {
    QDialog::resizeEvent(event);
    queueRowFit();
}

void DialogEndpointDetails::copySelection() const {
    QStringList lines;
    const auto selected = ui->table->selectedItems();
    for (const auto *item : selected) {
        if (!item->text().isEmpty()) lines << item->text();
    }
    if (!lines.isEmpty()) QGuiApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
}

void DialogEndpointDetails::applyStatus(const Stats::VpnEndpointView &view) {
    tag_ = view.tag;
    setWindowTitle(view.displayName.isEmpty() ? tr("Endpoint Details")
                                              : tr("Endpoint — %1").arg(view.displayName));

    QList<DetailRow> rows;
    const auto field = [&rows](const QString &key, const QString &label, const QString &value,
                               const QColor &color = QColor()) {
        if (!value.isEmpty()) rows << DetailRow{DetailRow::Field, key, label, value, color};
    };
    const auto section = [&rows](const QString &title, const QStringList &values) {
        if (values.isEmpty()) return;
        rows << DetailRow{DetailRow::Section, {},
                          QStringLiteral("%1 (%2)").arg(title).arg(values.size()), {}, {}};
        for (const auto &value : values) rows << DetailRow{DetailRow::Value, {}, {}, value, {}};
    };

    field(QStringLiteral("state"), tr("State"), Stats::VpnStateText(view.state),
          Stats::VpnStateColor(view.state));
    field({}, tr("Error"), view.error, Stats::kStatsProblemColor);
    field({}, tr("Server"), view.server);
    field({}, tr("Transport"), view.network);
    field({}, tr("Cipher"), view.cipher);
    field({}, tr("MTU"), view.mtu > 0 ? QString::number(view.mtu) : QString());
    const bool hasUptime = view.connected && view.connectedSince > 0;
    field(QStringLiteral("uptime"), tr("Connected for"),
          hasUptime ? Stats::HumanizeDuration(QDateTime::currentSecsSinceEpoch() - view.connectedSince)
                    : QString());
    field({}, tr("IPv4 address"), view.ipv4.join(QStringLiteral(", ")));
    field({}, tr("IPv6 address"), view.ipv6.join(QStringLiteral(", ")));

    QStringList domainItems;
    domainItems.reserve(view.domains.size());
    for (const auto &domain : view.domains) {
        domainItems << (domain == QStringLiteral(".")
                            ? tr("All domains — every DNS query goes through this tunnel")
                            : domain);
    }

    section(tr("Routes through this tunnel"), view.routes);
    section(tr("Kept outside this tunnel"), view.excludedRoutes);
    section(tr("DNS servers pushed by the server"), view.dns);
    section(tr("Domains routed to this tunnel"), domainItems);

    setRows(rows);
}

void DialogEndpointDetails::markGone() {
    for (int row = 0; row < static_cast<int>(rows_.size()); row++) {
        auto *value = ui->table->item(row, 1);
        if (value == nullptr) continue;
        if (rows_[row].key == QStringLiteral("state")) {
            value->setText(tr("No longer running"));
            value->setForeground(QBrush(Stats::kStatsProblemColor));
        } else if (rows_[row].key == QStringLiteral("uptime")) {
            value->setText(QStringLiteral("—"));
        }
    }
}
