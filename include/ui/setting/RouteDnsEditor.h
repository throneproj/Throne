#pragma once

#include <QWidget>

#include "include/database/entities/RouteProfile.h"

class DnsServerListEdit;
class QCheckBox;
class QComboBox;
class QLabel;
class QRadioButton;

namespace Validation {
    class Form;
}

namespace DnsServerHistory {
    const QStringList& RemotePresets();

    const QStringList& DirectPresets();

    // Moves a typed server to the front, keeping at most 8; false when the history did not change.
    bool Remember(QStringList& history, const QStringList& presets, const QString& server);
}

// A routing profile's DNS tab: follow Routing Settings > DNS, or use its own servers.
class RouteDnsEditor : public QWidget {
    Q_OBJECT

public:
    explicit RouteDnsEditor(const Configs::RouteProfile& profile, QWidget* parent = nullptr);

    // The server fields are disabled while the global settings apply, so the form skips them then.
    void bindTo(Validation::Form* form);

    // Also remembers the servers typed into the fields.
    void applyTo(Configs::RouteProfile& profile) const;

    // applyTo without touching the server history.
    void writeTo(Configs::RouteProfile& profile) const;

    [[nodiscard]] bool isCustom() const;

signals:
    void customChanged(bool custom);

private:
    void showDns(const Configs::RouteDns& dns) const;

    [[nodiscard]] Configs::RouteDns shownDns() const;

    void syncState() const;

    QRadioButton* globalRadio = nullptr;
    QRadioButton* customRadio = nullptr;
    QLabel* dnsObjectNote = nullptr;
    QWidget* fields = nullptr;
    DnsServerListEdit* remoteDns = nullptr;
    QCheckBox* remoteDisableIPv6 = nullptr;
    DnsServerListEdit* directDns = nullptr;
    QCheckBox* directDisableIPv6 = nullptr;
    QComboBox* finalOut = nullptr;
    QCheckBox* dnsRouting = nullptr;
    QCheckBox* fakeIP = nullptr;
    QCheckBox* fakeIPDisableIPv6 = nullptr;

    // The custom values, kept while the global ones are shown.
    Configs::RouteDns customDraft;
    // As opened, so only servers typed here are remembered.
    Configs::RouteDns opened;
};
