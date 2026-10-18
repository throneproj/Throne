#include "include/engine/vpn/VpnEndpoints.h"

#include "include/api/RPC.h"
#include "include/database/DatabaseManager.h"
#include "include/database/ProfilesRepo.h"
#include "include/database/SettingsRepo.h"
#include "include/engine/EngineTr.h"

#include <QMutex>
#include <QObject>

namespace {
    QStringList endpointDetailsToStringList(const std::vector<std::string> &values) {
        QStringList out;
        out.reserve(static_cast<qsizetype>(values.size()));
        for (const auto &value : values) {
            const auto text = QString::fromStdString(value).trimmed();
            if (!text.isEmpty()) out << text;
        }
        return out;
    }

    // Pushed suffixes arrive raw: any case, maybe a trailing dot, and "." alone means every domain.
    QStringList endpointDetailsNormalizeDomains(const std::vector<std::string> &values) {
        QStringList out;
        for (const auto &value : values) {
            auto text = QString::fromStdString(value).trimmed();
            if (text.isEmpty()) continue;
            if (text != QStringLiteral(".")) {
                while (text.endsWith(QLatin1Char('.'))) text.chop(1);
                text = text.toLower();
            }
            if (text.isEmpty() || out.contains(text)) continue;
            out << text;
        }
        return out;
    }
    QMutex endpointDetailsProfilesMu;
    QMap<QString, int> endpointDetailsProfiles;
}

namespace Stats {
    void SetVpnEndpointProfiles(const QMap<QString, int> &tagToProfileID) {
        QMutexLocker lk(&endpointDetailsProfilesMu);
        endpointDetailsProfiles = tagToProfileID;
    }

    int VpnEndpointProfileID(const QString &tag) {
        QMutexLocker lk(&endpointDetailsProfilesMu);
        return endpointDetailsProfiles.value(tag, -1);
    }

    QString VpnEndpointDisplayName(const QString &tag) {
        const int id = VpnEndpointProfileID(tag);
        if (id < 0 || Configs::dataManager == nullptr) return tag;
        const auto ent = Configs::dataManager->profilesRepo->GetProfile(id);
        if (ent == nullptr || ent->outbound == nullptr) return tag;
        return ent->outbound->DisplayName();
    }

    VpnEndpointView MakeVpnEndpointView(const libcore::VPNEndpointStatus &status) {
        VpnEndpointView view;
        view.tag = QString::fromStdString(status.tag.value());
        view.displayName = VpnEndpointDisplayName(view.tag);
        view.state = QString::fromStdString(status.state.value());
        view.error = QString::fromStdString(status.error.value());
        view.server = QString::fromStdString(status.server.value());
        view.network = QString::fromStdString(status.network.value());
        view.cipher = QString::fromStdString(status.cipher.value());
        view.ipv4 = endpointDetailsToStringList(status.ipv4);
        view.ipv6 = endpointDetailsToStringList(status.ipv6);
        view.dns = endpointDetailsToStringList(status.dns);
        view.routes = endpointDetailsToStringList(status.routes);
        view.excludedRoutes = endpointDetailsToStringList(status.excluded_routes);
        view.domains = endpointDetailsNormalizeDomains(status.search_domains);
        view.mtu = status.mtu.value();
        view.connectedSince = status.connected_since.value();
        view.connected = status.connected.value();
        return view;
    }

    QString VpnStateText(const QString &state) {
        if (state == QStringLiteral("connected")) return QObject::tr("Connected");
        if (state == QStringLiteral("connecting")) return QObject::tr("Connecting…");
        if (state == QStringLiteral("auth-pending")) return QObject::tr("Waiting for sign-in");
        if (state == QStringLiteral("error")) return QObject::tr("Error");
        if (state.isEmpty()) return QObject::tr("Unknown");
        return state;
    }

    QString HumanizeDuration(qint64 seconds) {
        if (seconds < 0) seconds = 0;
        const qint64 d = seconds / 86400; seconds %= 86400;
        const qint64 h = seconds / 3600;  seconds %= 3600;
        const qint64 m = seconds / 60;    seconds %= 60;
        QStringList parts;
        if (d > 0) parts << QStringLiteral("%1d").arg(d);
        if (h > 0) parts << QStringLiteral("%1h").arg(h);
        if (m > 0) parts << QStringLiteral("%1m").arg(m);
        if (d == 0 && h == 0) parts << QStringLiteral("%1s").arg(seconds);
        return parts.join(QLatin1Char(' '));
    }

    std::shared_ptr<Configs::Profile> VpnExitEndpoint(const std::shared_ptr<Configs::Profile> &ent) {
        auto hop = ent;
        // The "proxy" tag lands on the exit hop, and the stored list runs in to out.
        if (hop != nullptr && hop->type == "chain") {
            const auto *chain = hop->Chain();
            if (chain == nullptr || chain->list.isEmpty()) return nullptr;
            hop = Configs::dataManager->profilesRepo->GetProfile(chain->list.back());
        }
        if (hop == nullptr) return nullptr;
        if (hop->type != "openvpn" && hop->type != "openconnect") return nullptr;
        return hop;
    }

    QString VpnExitStateText(const QString &state, const QString &error) {
        if (state == "connected") return MainWindowTr::tr("Connect OK");
        if (state == "connecting") return MainWindowTr::tr("Connecting");
        if (state == "auth-pending") return MainWindowTr::tr("Waiting for authentication");
        if (state == "error") {
            return error.isEmpty() ? MainWindowTr::tr("Tunnel error")
                                   : MainWindowTr::tr("Tunnel error") + ": " + error;
        }
        return state;
    }

    QString LiveVpnStateText(bool *connected) {
        if (connected != nullptr) *connected = false;
        const int startedID = Configs::dataManager->settingsRepo->started_id;
        if (startedID < 0) return {};
        if (VpnExitEndpoint(Configs::dataManager->profilesRepo->GetProfile(startedID)) == nullptr) return {};

        bool ok = false;
        const auto status = API::defaultClient->QueryVPNStatus(&ok, {"proxy"});
        if (!ok || status.results.empty()) return {};
        const auto &res = status.results.front();
        if (connected != nullptr) *connected = res.connected.value();
        return VpnExitStateText(QString::fromStdString(res.state.value()), QString::fromStdString(res.error.value()));
    }

    QString LiveVpnConnectOkText() {
        bool connected = false;
        const auto text = LiveVpnStateText(&connected);
        return connected ? text : QString();
    }
} // namespace Stats
