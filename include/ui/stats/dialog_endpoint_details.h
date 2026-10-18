#pragma once

#include <QColor>
#include <QDialog>
#include <QList>
#include <QString>

#include "include/engine/vpn/VpnEndpoints.h"
#include "ui_dialog_endpoint_details.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogEndpointDetails;
}
QT_END_NAMESPACE

namespace Stats {
    inline const QColor kStatsAccentColor(0x4F, 0x8A, 0xF7);
    inline const QColor kStatsHealthyColor(0x34, 0xC9, 0x8A);
    inline const QColor kStatsProblemColor(0xC6, 0x28, 0x28);

    QColor VpnStateColor(const QString &state);
} // namespace Stats

class DialogEndpointDetails : public QDialog {
    Q_OBJECT

public:
    explicit DialogEndpointDetails(const Stats::VpnEndpointView &view, QWidget *parent = nullptr);
    ~DialogEndpointDetails() override;

    [[nodiscard]] QString tag() const { return tag_; }

    void applyStatus(const Stats::VpnEndpointView &view);

    void markGone();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    struct DetailRow {
        enum Kind { Field, Section, Value };

        Kind kind = Field;
        QString key;
        QString label;
        QString value;
        QColor color;
    };

    void setRows(const QList<DetailRow> &rows);
    void fitRowHeights();
    void queueRowFit();
    void copySelection() const;

    Ui::DialogEndpointDetails *ui;
    QString tag_;
    QList<DetailRow> rows_;
    bool rowFitQueued_ = false;
};
