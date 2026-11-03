#pragma once

#include <QWidget>

#include "include/configs/common/ResolverSource.h"

class QComboBox;
class QHBoxLayout;
class QLabel;
class QPlainTextEdit;
class QPushButton;

namespace Validation {
    class Form;
}

// The resolver lines and IP list of a dnstt, MasterDNS or XDNS profile.
class ResolverSourceWidget : public QWidget {
    Q_OBJECT

public:
    explicit ResolverSourceWidget(QWidget *parent = nullptr);

    void setFlavor(Configs::ResolverFlavor flavor);
    [[nodiscard]] Configs::ResolverFlavor flavor() const { return m_flavor; }

    void load(const Configs::ResolverSource &source);
    [[nodiscard]] Configs::ResolverSource save() const;

    // Re-reads the IP lists, so one made after load() is offered, and selects id (-1 for none).
    void setIpList(int id);

    // The saved profile "Find resolvers…" scans for; -1, an unsaved profile, disables it.
    void setProfileId(int id);
    [[nodiscard]] int profileId() const { return m_profileId; }

    void bindValidation(Validation::Form &form, QObject *owner = nullptr);

    [[nodiscard]] QPlainTextEdit *linesEdit() const { return m_lines; }
    [[nodiscard]] QComboBox *ipListCombo() const { return m_ipList; }
    // The IP list row; widgets added here follow the combo.
    [[nodiscard]] QHBoxLayout *actionRow() const { return m_row; }

signals:
    void changed();

private:
    QPlainTextEdit *m_lines = nullptr;
    QLabel *m_ipListLabel = nullptr;
    QComboBox *m_ipList = nullptr;
    QHBoxLayout *m_row = nullptr;
    QPushButton *m_find = nullptr;
    int m_profileId = -1;
    Configs::ResolverFlavor m_flavor = Configs::ResolverFlavor::Dnstt;
};
