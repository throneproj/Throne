#include "include/configs/sub/vpnFileImport.hpp"

#include <QHostAddress>
#include <QObject>
#include <QRegularExpression>
#include <QSet>
#include <QXmlStreamReader>

#include "include/configs/generate.h"
#include "include/configs/outbounds/openconnect.h"
#include "include/configs/outbounds/openvpn.h"

namespace Configs {
    // Unity build: an anonymous namespace shares one TU with its neighbours, hence the prefix.
    namespace {
        constexpr int kOvpnDefaultPort = 1194;

        struct vpnfiDirective {
            QStringList args;
            QString inlineTag;
            QStringList inlineBody;
        };

        struct vpnfiRemote {
            QString host;
            int port = 0;
            QString network;
        };

        struct vpnfiHostEntry {
            QString name;
            QString address;
            QString group;
            QString protocol;
            QStringList fingerprints;
        };

        // parse_line treats `#`/`;` as a comment only where a parameter would start.
        QStringList vpnfiTokenize(const QString& line)
        {
            QStringList tokens;
            QString token;
            QChar quote;
            bool inToken = false;
            for (int i = 0; i < line.size(); i++) {
                const auto ch = line[i];
                if (quote != QChar()) {
                    if (ch == quote) {
                        quote = QChar();
                        continue;
                    }
                    if (ch == '\\' && quote == '"' && i + 1 < line.size() && (line[i + 1] == '"' || line[i + 1] == '\\')) {
                        token += line[++i];
                        continue;
                    }
                    token += ch;
                    continue;
                }
                if (ch.isSpace()) {
                    if (inToken) {
                        tokens += token;
                        token.clear();
                        inToken = false;
                    }
                    continue;
                }
                if (!inToken && (ch == '#' || ch == ';')) break;
                inToken = true;
                if (ch == '"' || ch == '\'') {
                    quote = ch;
                    continue;
                }
                if (ch == '\\' && i + 1 < line.size()) {
                    token += line[++i];
                    continue;
                }
                token += ch;
            }
            if (inToken) tokens += token;
            return tokens;
        }

        QList<vpnfiDirective> vpnfiReadOvpnDirectives(const QString& body)
        {
            QList<vpnfiDirective> result;
            const auto lines = body.split('\n');
            QString pending;
            for (int i = 0; i < lines.size(); i++) {
                auto trimmed = lines[i];
                trimmed.remove('\r');
                trimmed = trimmed.trimmed();
                if (pending.isEmpty() && trimmed.size() > 2 && trimmed.startsWith('<') && trimmed.endsWith('>') &&
                    !trimmed.startsWith("</")) {
                    vpnfiDirective item;
                    item.inlineTag = trimmed.mid(1, trimmed.size() - 2).trimmed();
                    const auto closing = "</" + item.inlineTag + ">";
                    for (i++; i < lines.size(); i++) {
                        auto inner = lines[i];
                        inner.remove('\r');
                        inner = inner.trimmed();
                        if (inner == closing) break;
                        item.inlineBody += inner;
                    }
                    while (!item.inlineBody.isEmpty() && item.inlineBody.constLast().isEmpty()) item.inlineBody.removeLast();
                    while (!item.inlineBody.isEmpty() && item.inlineBody.constFirst().isEmpty()) item.inlineBody.removeFirst();
                    result += item;
                    continue;
                }
                if (trimmed.endsWith('\\') && !trimmed.endsWith("\\\\")) {
                    pending += trimmed.chopped(1);
                    pending += ' ';
                    continue;
                }
                const auto full = pending + trimmed;
                pending.clear();
                if (auto args = vpnfiTokenize(full); !args.isEmpty()) result += vpnfiDirective{args, {}, {}};
            }
            if (!pending.isEmpty()) {
                if (auto args = vpnfiTokenize(pending); !args.isEmpty()) result += vpnfiDirective{args, {}, {}};
            }
            return result;
        }

        // sing-box wants a duration string where `.ovpn` writes bare seconds.
        QString vpnfiDuration(const QString& value)
        {
            bool ok = false;
            const auto seconds = value.toLongLong(&ok);
            if (ok) return seconds > 0 ? QString::number(seconds) + "s" : QString();
            return IsValidDuration(value) ? value : QString();
        }

        // parseSubnet takes both the /24 and the /255.255.255.0 spelling.
        QString vpnfiPrefix(const QString& address, const QString& mask)
        {
            const auto parsed = QHostAddress::parseSubnet(address + "/" + mask);
            if (parsed.second < 0 || parsed.first.isNull()) return {};
            return parsed.first.toString() + "/" + QString::number(parsed.second);
        }

        QString vpnfiNormalizePrefix(const QString& prefix)
        {
            const auto parsed = QHostAddress::parseSubnet(prefix);
            if (parsed.second < 0 || parsed.first.isNull()) return {};
            return parsed.first.toString() + "/" + QString::number(parsed.second);
        }

        // The endpoint wants 64 bare lowercase hex characters, not colon-separated.
        QString vpnfiFingerprint(const QString& raw)
        {
            auto hex = raw;
            hex.remove(':');
            hex.remove(' ');
            hex = hex.toLower();
            static const QRegularExpression re("^[0-9a-f]{64}$");
            return re.match(hex).hasMatch() ? hex : QString();
        }

        QString vpnfiNetwork(const QString& proto)
        {
            const auto value = proto.toLower();
            if (value == "udp" || value == "udp4" || value == "udp6" || value == "tcp" || value == "tcp4" || value == "tcp6") {
                return value;
            }
            if (value == "tcp-client") return "tcp";
            if (value == "tcp4-client") return "tcp4";
            if (value == "tcp6-client") return "tcp6";
            return {};
        }

        bool vpnfiRouteKeyword(const QString& value)
        {
            return value == "vpn_gateway" || value == "net_gateway" || value == "remote_host" || value == "default" ||
                   value == "dhcp";
        }

        vpnfiRemote vpnfiParseRemote(const QStringList& args)
        {
            vpnfiRemote remote;
            if (args.size() < 2) return remote;
            remote.host = args[1];
            for (int i = 2; i < args.size() && i < 4; i++) {
                bool numeric = false;
                if (const auto port = args[i].toInt(&numeric); numeric && port > 0) remote.port = port;
                else if (auto network = vpnfiNetwork(args[i]); !network.isEmpty()) remote.network = network;
            }
            return remote;
        }

        // No endpoint equivalent, or the endpoint already behaves this way.
        const QSet<QString> vpnfiOvpnIgnored = {
            "allow-recursive-routing", "auth-nocache", "auth-token", "auth-token-user", "bind", "block-outside-dns",
            "client", "connect-retry", "connect-retry-max", "connect-timeout", "daemon", "dco", "dev-node", "dh",
            "dhcp-option", "dhcp-release", "dhcp-renew", "disable-dco", "down", "down-pre", "echo", "errors-to-stderr",
            "fast-io", "float", "group", "hash-size", "ifconfig-noexec", "ifconfig-nowarn", "ipchange", "key-method",
            "log", "log-append", "lport", "machine-readable-output", "max-routes", "mode", "mssfix-default", "mtu-disc",
            "mtu-test", "multihome", "mute", "mute-replay-warnings", "ncp-disable", "nice", "nobind", "opt-verify",
            "passtos", "persist-key", "persist-local-ip", "persist-remote-ip", "persist-tun", "ping-timer-rem", "pull",
            "push-peer-info", "rcvbuf", "register-dns", "remap-usr1", "remote-random-hostname", "resolv-retry",
            "route-delay", "route-method", "route-pre-down", "route-up", "script-security", "server-poll-timeout",
            "service", "setenv-safe", "single-session", "sndbuf", "socket-flags", "status", "suppress-timestamps",
            "syslog", "tls-client", "tls-exit", "tls-verify", "topology-subnet", "tran-window", "tun-mtu-extra",
            "txqueuelen", "up", "up-delay", "up-restart", "user", "verb", "windows-driver", "writepid",
        };

        // Recognised, but nothing in the endpoint carries them.
        const QSet<QString> vpnfiOvpnUnsupported = {
            "askpass", "capath", "cryptoapicert", "engine", "extra-certs", "http-proxy", "http-proxy-option",
            "inactive", "link-mtu", "management", "management-hold", "management-query-passwords", "no-replay",
            "ping-exit", "pkcs11-id", "pkcs11-providers", "providers", "socks-proxy", "tls-ciphersuites",
            "tls-crypt-v2-verify", "tls-export-cert", "x509-track", "x509-username-field",
        };

        const QSet<QString> vpnfiOvpnServerOnly = {
            "client-config-dir", "ifconfig-pool", "push", "server", "server-bridge", "tls-server",
        };
    }

    bool ParseOvpnConfig(const QString& body, openvpn& out, QStringList* problems)
    {
        QStringList notes;
        const auto directives = vpnfiReadOvpnDirectives(body);
        const auto flush = [&notes, problems] {
            if (problems != nullptr) *problems += notes;
        };
        if (directives.isEmpty()) {
            notes << QObject::tr("Empty OpenVPN configuration.");
            flush();
            return false;
        }

        out.servers.clear();
        out.address.clear();
        out.routes.clear();
        out.data_ciphers.clear();
        out.pull_filters.clear();
        out.redirect_gateway_flags.clear();
        out.tls->peer_fingerprint.clear();
        out.tls->remote_certificate_ku.clear();

        QList<vpnfiRemote> remotes;
        int defaultPort = 0;
        QString keyDirection;
        QString legacyCipher;
        QString ifconfigLocal;
        QString ifconfigSecond;
        QString fatal;
        QString clientCertSwitch;
        QString friendlyName;

        // OpenVPN Connect keeps its display name in a comment, which the tokenizer drops.
        for (const auto& line : body.split('\n')) {
            auto trimmed = line.trimmed();
            if (!trimmed.startsWith('#')) continue;
            trimmed = trimmed.mid(1).trimmed();
            const auto prefix = QStringLiteral("OVPN_FRIENDLY_PROFILE_NAME=");
            if (trimmed.startsWith(prefix)) friendlyName = trimmed.mid(prefix.size()).trimmed();
        }

        for (const auto& item : directives) {
            if (!fatal.isEmpty()) break;

            if (!item.inlineTag.isEmpty()) {
                const auto& tag = item.inlineTag;
                if (tag == "connection") {
                    vpnfiRemote remote;
                    QString blockNetwork;
                    int blockPort = 0;
                    for (const auto& line : item.inlineBody) {
                        const auto args = vpnfiTokenize(line);
                        if (args.isEmpty()) continue;
                        if (args[0] == "remote") remote = vpnfiParseRemote(args);
                        else if (args[0] == "proto" && args.size() > 1) blockNetwork = vpnfiNetwork(args[1]);
                        else if ((args[0] == "port" || args[0] == "rport") && args.size() > 1) blockPort = args[1].toInt();
                    }
                    if (remote.host.isEmpty()) {
                        notes << QObject::tr("<connection> block without a remote, skipped.");
                        continue;
                    }
                    if (remote.network.isEmpty()) remote.network = blockNetwork;
                    if (remote.port == 0) remote.port = blockPort;
                    remotes += remote;
                    continue;
                }
                if (tag == "ca") out.tls->certificate = item.inlineBody;
                else if (tag == "cert") out.tls->client_certificate = item.inlineBody;
                else if (tag == "key") out.tls->client_key = item.inlineBody;
                else if (tag == "tls-auth" || tag == "tls-crypt" || tag == "tls-crypt-v2") {
                    out.tls->control_wrap->type = QString(tag).replace('-', '_');
                    out.tls->control_wrap->key = item.inlineBody;
                } else if (tag == "secret") {
                    out.mode = "static_key";
                    out.static_key = item.inlineBody;
                } else if (tag == "auth-user-pass") {
                    if (!item.inlineBody.isEmpty()) out.username = item.inlineBody[0];
                    if (item.inlineBody.size() > 1) out.password = item.inlineBody[1];
                } else if (tag == "peer-fingerprint") {
                    for (const auto& line : item.inlineBody) {
                        if (auto hex = vpnfiFingerprint(line); !hex.isEmpty()) out.tls->peer_fingerprint += hex;
                        else notes << QObject::tr("Ignored an unreadable peer fingerprint: %1").arg(line);
                    }
                } else if (tag == "pkcs12") {
                    fatal = QObject::tr("PKCS#12 bundles are not supported; export the CA, certificate and key as PEM.");
                } else {
                    notes << QObject::tr("Ignored inline block: <%1>").arg(tag);
                }
                continue;
            }

            const auto& args = item.args;
            const auto key = args[0];
            const auto value = args.size() > 1 ? args[1] : QString();

            if (vpnfiOvpnServerOnly.contains(key)) {
                fatal = QObject::tr("This is an OpenVPN server configuration (%1), not a client profile.").arg(key);
                continue;
            }
            if (key == "pkcs12") {
                fatal = QObject::tr("PKCS#12 bundles are not supported; export the CA, certificate and key as PEM.");
                continue;
            }
            if (key == "dev" || key == "dev-type") {
                if (value.startsWith("tap")) {
                    fatal = QObject::tr("TAP (layer 2) tunnels are not supported; only `dev tun` profiles can be imported.");
                }
                continue;
            }
            if (key == "mode") {
                if (value == "server") {
                    fatal = QObject::tr("This is an OpenVPN server configuration (mode server), not a client profile.");
                }
                continue;
            }

            if (key == "remote") {
                if (auto remote = vpnfiParseRemote(args); !remote.host.isEmpty()) remotes += remote;
                continue;
            }
            if (key == "proto") {
                if (auto network = vpnfiNetwork(value); !network.isEmpty()) out.network = network;
                else if (value.endsWith("-server")) fatal = QObject::tr("`proto %1` is a server transport.").arg(value);
                else notes << QObject::tr("Unknown transport: proto %1").arg(value);
                continue;
            }
            if (key == "port" || key == "rport") {
                defaultPort = value.toInt();
                continue;
            }
            if (key == "remote-random") {
                out.remote_random = true;
                continue;
            }

            if (key == "ifconfig" && args.size() > 2) {
                ifconfigLocal = args[1];
                ifconfigSecond = args[2];
                continue;
            }
            if (key == "ifconfig-ipv6" && args.size() > 1) {
                if (auto prefix = vpnfiNormalizePrefix(args[1]); !prefix.isEmpty()) out.address += args[1];
                else notes << QObject::tr("Ignored an unreadable IPv6 interface address: %1").arg(args[1]);
                if (args.size() > 2) out.peer_address_ipv6 = args[2];
                continue;
            }
            if (key == "topology") {
                out.topology = value;
                continue;
            }
            if (key == "auth-retry") {
                out.auth_retry = value;
                continue;
            }
            if (key == "auth-user-pass") {
                if (!value.isEmpty() && value != "[inline]") {
                    notes << QObject::tr("Credentials live in %1; enter them in the profile editor.").arg(value);
                }
                continue;
            }
            // OpenVPN 3 reads both as a client-side "log in without a client certificate"; 2.x has no such switch.
            if (key == "client-cert-not-required") {
                clientCertSwitch = key;
                continue;
            }
            if (key == "setenv") {
                const auto setting = args.size() > 2 ? args[2] : QString();
                if (value == "CLIENT_CERT" && setting == "0") clientCertSwitch = "setenv CLIENT_CERT 0";
                else if (value == "FRIENDLY_NAME" && !setting.isEmpty()) friendlyName = setting;
                continue;
            }
            if (key == "static-challenge") {
                out.static_challenge = value;
                if (args.size() > 2) out.static_challenge_echo = args[2] == "1";
                continue;
            }
            if (key == "key-direction") {
                keyDirection = value == "1" ? "client" : value == "0" ? "server" : QString();
                continue;
            }
            if (key == "secret") {
                out.mode = "static_key";
                if (!value.isEmpty() && value != "[inline]") out.static_key_path = value;
                if (args.size() > 2) keyDirection = args[2] == "1" ? "client" : args[2] == "0" ? "server" : QString();
                continue;
            }

            if (key == "ca" || key == "cert" || key == "key" || key == "crl-verify") {
                if (value.isEmpty() || value == "[inline]") continue;
                if (key == "ca") out.tls->certificate_path = value;
                else if (key == "cert") out.tls->client_certificate_path = value;
                else if (key == "key") out.tls->client_key_path = value;
                else {
                    out.tls->crl_path = value;
                    if (args.size() > 2 && args[2] == "dir") {
                        notes << QObject::tr("A hash-directory CRL is not supported; point `crl-verify` at a PEM or DER file.");
                    }
                }
                continue;
            }
            if (key == "tls-auth" || key == "tls-crypt" || key == "tls-crypt-v2") {
                out.tls->control_wrap->type = QString(key).replace('-', '_');
                if (!value.isEmpty() && value != "[inline]") out.tls->control_wrap->key_path = value;
                if (key == "tls-auth" && args.size() > 2) {
                    keyDirection = args[2] == "1" ? "client" : args[2] == "0" ? "server" : QString();
                }
                continue;
            }
            if (key == "peer-fingerprint" || key == "verify-hash") {
                if (auto hex = vpnfiFingerprint(value); !hex.isEmpty()) out.tls->peer_fingerprint += hex;
                else notes << QObject::tr("Ignored an unreadable peer fingerprint: %1").arg(value);
                continue;
            }
            if (key == "remote-cert-tls") {
                out.tls->remote_certificate_tls = value;
                continue;
            }
            if (key == "remote-cert-ku") {
                for (int i = 1; i < args.size(); i++) out.tls->remote_certificate_ku += args[i];
                continue;
            }
            if (key == "remote-cert-eku") {
                out.tls->remote_certificate_eku = value;
                continue;
            }
            if (key == "ns-cert-type") {
                out.tls->ns_certificate_type = value;
                continue;
            }
            if (key == "verify-x509-name") {
                out.tls->server_name = value;
                out.tls->server_name_type = args.size() > 2 ? args[2] : QStringLiteral("subject");
                continue;
            }
            if (key == "tls-version-min" || key == "tls-version-max") {
                (key == "tls-version-min" ? out.tls->version_min : out.tls->version_max) = value;
                continue;
            }
            if (key == "tls-cipher") {
                out.tls->cipher = value;
                continue;
            }
            if (key == "tls-groups") {
                out.tls->groups = value;
                continue;
            }
            if (key == "tls-cert-profile") {
                out.tls->certificate_profile = value;
                continue;
            }

            if (key == "cipher") {
                legacyCipher = value;
                continue;
            }
            if (key == "data-ciphers" || key == "ncp-ciphers") {
                out.data_ciphers = value.split(':', Qt::SkipEmptyParts);
                continue;
            }
            if (key == "data-ciphers-fallback") {
                out.data_ciphers_fallback = value;
                continue;
            }
            if (key == "auth") {
                out.auth = value;
                continue;
            }
            if (key == "mssfix") {
                if (value.isEmpty()) continue;
                if (value == "0") out.mss_fix_disabled = true;
                else {
                    out.mss_fix = value.toInt();
                    if (args.size() > 2 && (args[2] == "mtu" || args[2] == "fixed")) out.mss_fix_mode = args[2];
                }
                continue;
            }
            if (key == "fragment") {
                out.fragment = value.toInt();
                continue;
            }
            if (key == "replay-window") {
                out.replay_window = value.toInt();
                if (args.size() > 2) out.replay_window_time = vpnfiDuration(args[2]);
                continue;
            }
            if (key == "compress") {
                const auto mode = value.toLower();
                // The endpoint spells `compress lzo` as the identical comp-lzo mode.
                if (mode == "lzo") out.compression_lzo = "yes";
                else if (mode.isEmpty()) out.compression = "stub";
                else if (mode == "migrate") out.compression = "none";
                else out.compression = mode;
                continue;
            }
            if (key == "comp-lzo") {
                out.compression_lzo = value.isEmpty() ? QStringLiteral("adaptive") : value.toLower();
                continue;
            }
            if (key == "allow-compression") {
                out.allow_compression = value.toLower();
                continue;
            }

            if (key == "route-nopull") {
                out.route_no_pull = true;
                continue;
            }
            if (key == "pull-filter" && args.size() > 2) {
                auto filter = std::make_shared<OpenVPNPullFilter>();
                filter->action = args[1];
                filter->text = args[2];
                out.pull_filters += filter;
                continue;
            }
            if (key == "route" && args.size() > 1) {
                if (vpnfiRouteKeyword(args[1])) {
                    notes << QObject::tr("Ignored a symbolic route target: route %1").arg(args[1]);
                    continue;
                }
                QString prefix;
                int next = 2;
                if (args[1].contains('/')) {
                    prefix = vpnfiNormalizePrefix(args[1]);
                } else if (args.size() > 2 && !vpnfiRouteKeyword(args[2]) && !vpnfiPrefix(args[1], args[2]).isEmpty()) {
                    prefix = vpnfiPrefix(args[1], args[2]);
                    next = 3;
                } else {
                    prefix = vpnfiPrefix(args[1], "255.255.255.255");
                }
                if (prefix.isEmpty()) {
                    notes << QObject::tr("Ignored an unreadable route: %1").arg(args.mid(1).join(' '));
                    continue;
                }
                if (!out.routes.contains(prefix)) out.routes += prefix;
                if (args.size() > next && !vpnfiRouteKeyword(args[next]) && out.route_gateway.isEmpty()) {
                    out.route_gateway = args[next];
                }
                if (args.size() > next + 1 && out.route_metric == 0) out.route_metric = args[next + 1].toInt();
                continue;
            }
            if (key == "route-ipv6" && args.size() > 1) {
                if (auto prefix = vpnfiNormalizePrefix(args[1]); !prefix.isEmpty()) {
                    if (!out.routes.contains(prefix)) out.routes += prefix;
                } else {
                    notes << QObject::tr("Ignored an unreadable route: %1").arg(args.mid(1).join(' '));
                }
                continue;
            }
            if (key == "route-gateway") {
                if (!vpnfiRouteKeyword(value)) out.route_gateway = value;
                continue;
            }
            if (key == "route-metric") {
                out.route_metric = value.toInt();
                continue;
            }
            if (key == "redirect-gateway" || key == "redirect-private") {
                if (key == "redirect-gateway") out.redirect_gateway = true;
                else out.redirect_private = true;
                for (int i = 1; i < args.size(); i++) {
                    const auto flag = args[i].toLower();
                    if (flag == "block-local" || flag == "bypass-dhcp" || flag == "bypass-dns") {
                        notes << QObject::tr("redirect-gateway flag has no sing-box equivalent: %1").arg(flag);
                        continue;
                    }
                    if (!out.redirect_gateway_flags.contains(flag)) out.redirect_gateway_flags += flag;
                }
                continue;
            }
            if (key == "block-ipv6") {
                out.block_ipv6 = true;
                continue;
            }

            if (key == "keepalive" && args.size() > 2) {
                out.ping_interval = vpnfiDuration(args[1]);
                out.ping_restart = vpnfiDuration(args[2]);
                continue;
            }
            if (key == "ping") {
                out.ping_interval = vpnfiDuration(value);
                continue;
            }
            if (key == "ping-restart") {
                if (value == "0") out.ping_restart_disabled = true;
                else out.ping_restart = vpnfiDuration(value);
                continue;
            }
            if (key == "reneg-sec") {
                if (value == "0") out.renegotiate_disabled = true;
                else out.renegotiate_interval = vpnfiDuration(value);
                continue;
            }
            if (key == "reneg-bytes") {
                out.renegotiate_bytes = value.toLongLong();
                continue;
            }
            if (key == "reneg-pkts") {
                out.renegotiate_packets = value.toLongLong();
                continue;
            }
            if (key == "tls-timeout") {
                out.tls_timeout = vpnfiDuration(value);
                continue;
            }
            if (key == "hand-window") {
                out.handshake_window = vpnfiDuration(value);
                continue;
            }
            if (key == "explicit-exit-notify") {
                out.explicit_exit_notify = value.isEmpty() ? 1 : value.toInt();
                continue;
            }
            if (key == "tun-mtu") {
                out.mtu = value.toInt();
                continue;
            }

            if (vpnfiOvpnIgnored.contains(key)) continue;
            if (vpnfiOvpnUnsupported.contains(key)) {
                notes << QObject::tr("Not supported by the OpenVPN endpoint, ignored: %1").arg(key);
                continue;
            }
            notes << QObject::tr("Unknown OpenVPN directive, ignored: %1").arg(key);
        }

        if (!fatal.isEmpty()) {
            notes << fatal;
            flush();
            return false;
        }

        auto& tls = *out.tls;
        if (!clientCertSwitch.isEmpty()) {
            const bool dropped = !tls.client_certificate.isEmpty() || !tls.client_certificate_path.isEmpty() ||
                                 !tls.client_key.isEmpty() || !tls.client_key_path.isEmpty();
            tls.client_certificate.clear();
            tls.client_certificate_path.clear();
            tls.client_key.clear();
            tls.client_key_path.clear();
            if (dropped) {
                notes << QObject::tr("`%1` turns the client certificate off; it was dropped and the server has to accept "
                                     "password login.").arg(clientCertSwitch);
            }
        }
        // The core, like OpenVPN 2.x, refuses a lone certificate or key; OpenVPN 3 only tolerates it behind the switch.
        const bool hasClientCert = !tls.client_certificate.isEmpty() || !tls.client_certificate_path.isEmpty();
        const bool hasClientKey = !tls.client_key.isEmpty() || !tls.client_key_path.isEmpty();
        if (hasClientCert != hasClientKey) {
            notes << (hasClientCert ? QObject::tr("`cert` without a `key`: add the private key, or "
                                                  "`client-cert-not-required` for password-only login.")
                                    : QObject::tr("`key` without a `cert`: add the client certificate."));
            flush();
            return false;
        }

        for (auto& remote : remotes) {
            if (remote.port == 0) remote.port = defaultPort > 0 ? defaultPort : kOvpnDefaultPort;
        }
        if (remotes.isEmpty()) {
            notes << QObject::tr("No `remote` server in the OpenVPN configuration.");
            flush();
            return false;
        }
        // `server`/`server_port` and `servers` conflict in the core.
        if (remotes.size() == 1) {
            out.server = remotes[0].host;
            out.server_port = remotes[0].port;
            if (!remotes[0].network.isEmpty()) out.network = remotes[0].network;
        } else {
            out.server.clear();
            out.server_port = 0;
            for (const auto& remote : remotes) {
                auto entry = std::make_shared<OpenVPNRemote>();
                entry->server = remote.host;
                entry->server_port = remote.port;
                entry->network = remote.network;
                out.servers += entry;
            }
        }
        if (!friendlyName.isEmpty()) out.name = friendlyName;
        if (out.name.isEmpty()) out.name = remotes[0].host;

        if (!ifconfigLocal.isEmpty() && !ifconfigSecond.isEmpty()) {
            // Only `--topology subnet` makes the second argument a netmask.
            const bool masked = out.topology == "subnet" || ifconfigSecond.startsWith("255.");
            const auto parsed = QHostAddress::parseSubnet(ifconfigLocal + "/" + (masked ? ifconfigSecond : QStringLiteral("32")));
            if (parsed.second >= 0) out.address += ifconfigLocal + "/" + QString::number(parsed.second);
            else notes << QObject::tr("Ignored an unreadable interface address: %1").arg(ifconfigLocal);
            if (!masked) out.peer_address = ifconfigSecond;
        }

        // `key-direction` binds to the control-channel wrap in TLS mode, the secret otherwise.
        if (!keyDirection.isEmpty()) {
            if (out.tls->control_wrap->type == "tls_auth") out.tls->control_wrap->direction = keyDirection;
            else if (out.mode == "static_key") out.key_direction = keyDirection;
        }
        // Outside static-key mode `cipher` is the pre-negotiation data cipher.
        if (!legacyCipher.isEmpty()) {
            if (out.mode == "static_key") out.cipher = legacyCipher;
            else if (out.data_ciphers_fallback.isEmpty()) out.data_ciphers_fallback = legacyCipher;
        }
        if (!out.tls->remote_certificate_eku.isEmpty() && !out.tls->remote_certificate_tls.isEmpty()) {
            notes << QObject::tr("`remote-cert-eku` replaces `remote-cert-tls`; the latter was dropped.");
            out.tls->remote_certificate_tls.clear();
        }
        // A redirect-gateway config is a full tunnel, not a management network.
        if (out.redirect_gateway && !out.route_no_pull) out.only_advertised_routes = false;

        // Every inline value conflicts with its own *_path in the core.
        if (!out.tls->certificate.isEmpty()) out.tls->certificate_path.clear();
        if (!out.tls->client_certificate.isEmpty()) out.tls->client_certificate_path.clear();
        if (!out.tls->client_key.isEmpty()) out.tls->client_key_path.clear();
        if (!out.tls->control_wrap->key.isEmpty()) out.tls->control_wrap->key_path.clear();
        if (!out.static_key.isEmpty()) out.static_key_path.clear();

        flush();
        return true;
    }

    bool ParseOvpnConfig(const QString& body, openvpn& out)
    {
        return ParseOvpnConfig(body, out, nullptr);
    }

    namespace {
        bool vpnfiOcOptionHasValue(const QString& name)
        {
            static const QSet<QString> set = {
                "authgroup", "base-mtu", "cafile", "cert-expire-warning", "certificate", "compression", "config",
                "cookie", "csd-user", "csd-wrapper", "dpd", "dtls-local-port", "force-dpd", "force-trojan",
                "form-entry", "gnutls-priority", "http-auth", "interface", "key-password", "key-type",
                "local-hostname", "localname", "mca-certificate", "mca-key", "mca-key-password", "mtu", "os",
                "pid-file", "protocol", "proxy", "proxy-auth", "queue-len", "reconnect-timeout", "resolve", "script",
                "server", "servercert", "setuid", "sni", "sslkey", "token-mode", "token-secret", "user", "useragent",
                "user-agent", "usergroup", "version-string", "xmlconfig",
            };
            return set.contains(name);
        }

        const QSet<QString> vpnfiOcIgnored = {
            "authenticate", "background", "cert-expire-warning", "config", "cookie-on-stdin", "cookieonly",
            "csd-user", "deflate", "dump-http-traffic", "gnutls-debug", "gnutls-priority", "help", "interface",
            "key-password-from-fsid", "key-type", "libproxy", "no-deflate", "no-proxy", "non-inter", "passtos",
            "passwd-on-stdin", "pid-file", "printcookie", "quiet", "resolve", "script", "script-tun", "setuid",
            "syslog", "timestamp", "verbose", "version", "xmlconfig",
        };

        // Kept apart so only an unknown option may swallow a detached argument.
        const QSet<QString> vpnfiOcFlags = {
            "allow-insecure-crypto", "disable-ipv6", "juniper", "no-compression", "no-dtls", "no-external-auth",
            "no-http-keepalive", "no-passwd", "no-system-trust", "no-xmlpost", "pfs",
        };

        QString vpnfiOcLongName(QChar shortName)
        {
            switch (shortName.toLatin1()) {
                case 'b': return "background";
                case 'C': return "cookie";
                case 'c': return "certificate";
                case 'e': return "cert-expire-warning";
                case 'F': return "form-entry";
                case 'g': return "usergroup";
                case 'h': return "help";
                case 'i': return "interface";
                case 'k': return "sslkey";
                case 'l': return "syslog";
                case 'm': return "mtu";
                case 'p': return "key-password";
                case 'P': return "proxy";
                case 'Q': return "queue-len";
                case 'q': return "quiet";
                case 'S': return "script-tun";
                case 's': return "script";
                case 't': return "token-mode";
                case 'U': return "setuid";
                case 'u': return "user";
                case 'V': return "version";
                case 'v': return "verbose";
                case 'x': return "xmlconfig";
                default: return {};
            }
        }

        void vpnfiApplyOcOption(openconnect& out, const QString& name, const QString& value, bool hasValue, QStringList& notes)
        {
            if (name == "protocol") {
                static const QSet<QString> flavors = {"anyconnect", "nc", "gp", "pulse", "f5", "fortinet"};
                if (const auto flavor = value.toLower(); flavors.contains(flavor)) out.flavor = flavor;
                else notes << QObject::tr("Unsupported OpenConnect protocol, ignored: %1").arg(value);
                return;
            }
            if (name == "juniper") {
                out.flavor = "nc";
                return;
            }
            if (name == "server") {
                out.SetServerUrl(value);
                return;
            }
            if (name == "user") {
                out.username = value;
                return;
            }
            if (name == "authgroup") {
                out.auth_group = value;
                return;
            }
            if (name == "usergroup") {
                out.server_path = value;
                return;
            }
            if (name == "cookie") {
                out.cookie = value;
                return;
            }
            if (name == "certificate") {
                out.tls->client_certificate_path = value;
                return;
            }
            if (name == "sslkey") {
                out.tls->client_key_path = value;
                return;
            }
            if (name == "key-password") {
                out.tls->client_key_password = value;
                return;
            }
            if (name == "mca-certificate") {
                out.tls->mca_certificate_path = value;
                return;
            }
            if (name == "mca-key") {
                out.tls->mca_key_path = value;
                return;
            }
            if (name == "mca-key-password") {
                out.tls->mca_key_password = value;
                return;
            }
            if (name == "cafile") {
                out.tls->certificate_authority_path = value;
                return;
            }
            if (name == "servercert") {
                if (!value.isEmpty()) out.tls->peer_fingerprint += value;
                return;
            }
            if (name == "no-system-trust") {
                out.tls->system_trust_disabled = true;
                return;
            }
            if (name == "sni") {
                out.tls->server_name = value;
                return;
            }
            if (name == "no-dtls") {
                out.no_udp = true;
                return;
            }
            if (name == "dtls-local-port") {
                out.dtls_local_port = value.toInt();
                return;
            }
            if (name == "compression") {
                if (const auto mode = value.toLower(); mode == "none") out.compression_disabled = true;
                else if (mode == "stateless" || mode == "all") out.compression_mode = mode;
                else notes << QObject::tr("Unknown compression mode, ignored: %1").arg(value);
                return;
            }
            if (name == "no-compression") {
                out.compression_disabled = true;
                return;
            }
            if (name == "disable-ipv6") {
                out.ipv6_disabled = true;
                return;
            }
            if (name == "no-http-keepalive") {
                out.http_keepalive_disabled = true;
                return;
            }
            if (name == "no-xmlpost") {
                out.xml_post_disabled = true;
                return;
            }
            if (name == "no-external-auth") {
                out.external_auth_disabled = true;
                return;
            }
            if (name == "no-passwd") {
                out.password_authentication_disabled = true;
                return;
            }
            if (name == "pfs") {
                out.pfs = true;
                return;
            }
            if (name == "allow-insecure-crypto") {
                out.allow_insecure_crypto = true;
                return;
            }
            if (name == "mtu") {
                out.mtu = value.toInt();
                return;
            }
            if (name == "base-mtu") {
                out.base_mtu = value.toInt();
                return;
            }
            if (name == "dpd" || name == "force-dpd") {
                out.dpd_interval = vpnfiDuration(value);
                return;
            }
            if (name == "reconnect-timeout") {
                out.reconnect_timeout = vpnfiDuration(value);
                return;
            }
            if (name == "force-trojan") {
                out.trojan_interval = vpnfiDuration(value);
                return;
            }
            if (name == "queue-len") {
                out.queue_length = value.toInt();
                return;
            }
            if (name == "useragent" || name == "user-agent") {
                out.user_agent = value;
                return;
            }
            if (name == "version-string") {
                out.version = value;
                return;
            }
            if (name == "local-hostname" || name == "localname") {
                out.local_hostname = value;
                return;
            }
            if (name == "os") {
                static const QSet<QString> reported = {"linux", "linux-64", "win", "mac-intel", "android", "apple-ios"};
                if (reported.contains(value.toLower())) out.reported_os = value.toLower();
                else notes << QObject::tr("Unknown reported OS, ignored: %1").arg(value);
                return;
            }
            if (name == "csd-wrapper") {
                out.csd->wrapper_path = value;
                return;
            }
            if (name == "token-mode") {
                // OpenConnect's `rsa` is the endpoint's `stoken`.
                const auto mode = value.toLower();
                if (mode == "rsa" || mode == "stoken") out.token->mode = "stoken";
                else if (mode == "totp" || mode == "hotp" || mode == "oidc") out.token->mode = mode;
                else notes << QObject::tr("Unsupported token mode, ignored: %1").arg(value);
                return;
            }
            if (name == "token-secret") {
                if (value.startsWith('@')) out.token->secret_path = value.mid(1);
                else out.token->secret = value;
                return;
            }
            if (name == "form-entry") {
                const auto colon = value.indexOf(':');
                const auto equals = value.indexOf('=', colon + 1);
                if (colon < 0 || equals < 0) {
                    notes << QObject::tr("Expected --form-entry=FORM:OPTION=VALUE, ignored: %1").arg(value);
                    return;
                }
                auto entry = std::make_shared<OpenConnectFormEntry>();
                entry->form_id = value.left(colon);
                entry->name = value.mid(colon + 1, equals - colon - 1);
                entry->value = value.mid(equals + 1);
                out.form_entries += entry;
                return;
            }
            if (name == "proxy" || name == "proxy-auth" || name == "http-auth") {
                notes << QObject::tr("Configure a proxy through Throne's chain instead, ignored: %1").arg(name);
                return;
            }
            if (vpnfiOcIgnored.contains(name)) return;
            notes << QObject::tr("Unknown OpenConnect option, ignored: %1").arg(hasValue ? name + "=" + value : name);
        }

        void vpnfiWalkOcArgs(const QStringList& argv, openconnect& out, QStringList& notes)
        {
            for (int i = 0; i < argv.size(); i++) {
                const auto token = argv[i];
                if (i == 0 && (token == "openconnect" || token.endsWith("/openconnect") || token.endsWith("openconnect.exe"))) {
                    continue;
                }
                if (token.startsWith("--")) {
                    auto name = token.mid(2);
                    QString value;
                    bool hasValue = false;
                    if (const auto equals = name.indexOf('='); equals >= 0) {
                        value = name.mid(equals + 1);
                        name = name.left(equals);
                        hasValue = true;
                    } else if (vpnfiOcOptionHasValue(name) && i + 1 < argv.size()) {
                        value = argv[++i];
                        hasValue = true;
                    } else if (!vpnfiOcFlags.contains(name) && !vpnfiOcIgnored.contains(name) && i + 2 < argv.size() &&
                               !argv[i + 1].startsWith('-')) {
                        // Swallow a detached value only while a trailing positional is left.
                        value = argv[++i];
                        hasValue = true;
                    }
                    vpnfiApplyOcOption(out, name, value, hasValue, notes);
                    continue;
                }
                if (token.size() > 1 && token.startsWith('-')) {
                    for (int c = 1; c < token.size(); c++) {
                        const auto name = vpnfiOcLongName(token[c]);
                        if (name.isEmpty()) {
                            notes << QObject::tr("Unknown OpenConnect option, ignored: -%1").arg(token[c]);
                            break;
                        }
                        if (!vpnfiOcOptionHasValue(name)) {
                            vpnfiApplyOcOption(out, name, {}, false, notes);
                            continue;
                        }
                        QString value;
                        if (c + 1 < token.size()) value = token.mid(c + 1);
                        else if (i + 1 < argv.size()) value = argv[++i];
                        vpnfiApplyOcOption(out, name, value, true, notes);
                        break;
                    }
                    continue;
                }
                // `--usergroup` outranks the URL path whichever came first.
                const auto group = out.server_path;
                out.SetServerUrl(token);
                if (!group.isEmpty()) out.server_path = group;
            }
        }

        bool vpnfiLooksLikeXml(const QString& text)
        {
            return text.startsWith("<?xml") || text.contains("<AnyConnectProfile") || text.contains("<ServerList");
        }

        bool vpnfiIsOcCommandLine(const QString& text)
        {
            for (const auto& line : text.split('\n')) {
                const auto trimmed = line.trimmed();
                if (trimmed.isEmpty() || trimmed.startsWith('#')) continue;
                return trimmed.startsWith("openconnect") || trimmed.startsWith('-');
            }
            return false;
        }

        bool vpnfiReadAnyConnectHosts(const QString& xml, QList<vpnfiHostEntry>& out, QStringList& notes)
        {
            QXmlStreamReader reader(xml);
            vpnfiHostEntry current;
            bool inEntry = false;
            while (!reader.atEnd()) {
                const auto token = reader.readNext();
                // Untrusted input: a DTD is the only place entities are declared.
                if (token == QXmlStreamReader::DTD) {
                    notes << QObject::tr("An AnyConnect profile carrying a DTD is not accepted.");
                    return false;
                }
                if (token == QXmlStreamReader::StartElement) {
                    const auto element = reader.name().toString();
                    if (element.compare("HostEntry", Qt::CaseInsensitive) == 0) {
                        current = {};
                        inEntry = true;
                        continue;
                    }
                    if (!inEntry) continue;
                    const auto text = reader.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
                    if (text.isEmpty()) continue;
                    if (element.compare("HostName", Qt::CaseInsensitive) == 0) current.name = text;
                    else if (element.compare("HostAddress", Qt::CaseInsensitive) == 0) current.address = text;
                    else if (element.compare("UserGroup", Qt::CaseInsensitive) == 0) current.group = text;
                    else if (element.compare("PrimaryProtocol", Qt::CaseInsensitive) == 0) current.protocol = text;
                    else if (element.endsWith("CertificateHash", Qt::CaseInsensitive)) current.fingerprints += text;
                    continue;
                }
                if (token == QXmlStreamReader::EndElement && inEntry &&
                    reader.name().toString().compare("HostEntry", Qt::CaseInsensitive) == 0) {
                    out += current;
                    current = {};
                    inEntry = false;
                }
            }
            if (reader.hasError()) {
                notes << QObject::tr("Malformed AnyConnect profile: %1").arg(reader.errorString());
                return false;
            }
            if (out.isEmpty()) {
                notes << QObject::tr("The AnyConnect profile lists no host entry.");
                return false;
            }
            return true;
        }

        bool vpnfiApplyHostEntry(const vpnfiHostEntry& entry, openconnect& out, QStringList& notes)
        {
            const auto address = entry.address.isEmpty() ? entry.name : entry.address;
            if (address.isEmpty()) {
                notes << QObject::tr("Skipped a host entry without an address.");
                return false;
            }
            if (entry.protocol.compare("IPsec", Qt::CaseInsensitive) == 0) {
                notes << QObject::tr("Skipped \"%1\": IKEv2/IPsec is not spoken by the OpenConnect endpoint.").arg(address);
                return false;
            }
            out.flavor = "anyconnect";
            out.SetServerUrl(address);
            if (out.server.isEmpty()) {
                notes << QObject::tr("Skipped an unreadable host address: %1").arg(address);
                return false;
            }
            if (!entry.group.isEmpty()) out.server_path = entry.group;
            out.name = entry.name.isEmpty() ? address : entry.name;
            for (const auto& fingerprint : entry.fingerprints) out.tls->peer_fingerprint += fingerprint;
            return true;
        }
    }

    bool ParseAnyConnectXml(const QString& xml, QList<std::shared_ptr<openconnect>>& out, QStringList* problems)
    {
        QStringList notes;
        QList<vpnfiHostEntry> entries;
        if (!vpnfiReadAnyConnectHosts(xml, entries, notes)) {
            if (problems != nullptr) *problems += notes;
            return false;
        }
        for (const auto& entry : entries) {
            auto host = std::make_shared<openconnect>();
            if (vpnfiApplyHostEntry(entry, *host, notes)) out += host;
        }
        if (problems != nullptr) *problems += notes;
        return !out.isEmpty();
    }

    bool ParseOpenConnectProfile(const QString& text, openconnect& out, QStringList* problems)
    {
        QStringList notes;
        const auto flush = [&notes, problems] {
            if (problems != nullptr) *problems += notes;
        };
        const auto body = text.trimmed();
        if (body.isEmpty()) {
            notes << QObject::tr("Empty OpenConnect profile.");
            flush();
            return false;
        }

        if (vpnfiLooksLikeXml(body)) {
            QList<vpnfiHostEntry> entries;
            if (!vpnfiReadAnyConnectHosts(body, entries, notes)) {
                flush();
                return false;
            }
            for (const auto& entry : entries) {
                if (!vpnfiApplyHostEntry(entry, out, notes)) continue;
                flush();
                return true;
            }
            notes << QObject::tr("The AnyConnect profile has no host entry this endpoint can use.");
            flush();
            return false;
        }

        out.tls->peer_fingerprint.clear();
        out.form_entries.clear();

        QStringList argv;
        if (vpnfiIsOcCommandLine(body)) {
            QString joined;
            for (const auto& line : body.split('\n')) {
                auto trimmed = line.trimmed();
                trimmed.remove('\r');
                if (trimmed.endsWith('\\')) trimmed.chop(1);
                joined += trimmed;
                joined += ' ';
            }
            argv = vpnfiTokenize(joined);
        } else {
            for (const auto& line : body.split('\n')) {
                const auto tokens = vpnfiTokenize(line);
                if (tokens.isEmpty()) continue;
                if (tokens[0].contains('=')) argv += "--" + tokens[0];
                else if (tokens.size() > 1) argv += "--" + tokens[0] + "=" + tokens.mid(1).join(' ');
                // A lone word that names no option is the bare server address.
                else if (vpnfiOcOptionHasValue(tokens[0]) || vpnfiOcFlags.contains(tokens[0]) ||
                         vpnfiOcIgnored.contains(tokens[0])) argv += "--" + tokens[0];
                else argv += tokens[0];
            }
        }
        if (argv.isEmpty()) {
            notes << QObject::tr("No OpenConnect options found.");
            flush();
            return false;
        }
        vpnfiWalkOcArgs(argv, out, notes);

        if (out.server.isEmpty()) {
            notes << QObject::tr("No OpenConnect server address found.");
            flush();
            return false;
        }
        if (out.name.isEmpty()) out.name = out.server;
        if (!out.tls->client_certificate.isEmpty()) out.tls->client_certificate_path.clear();
        if (!out.tls->client_key.isEmpty()) out.tls->client_key_path.clear();
        if (!out.tls->certificate_authority.isEmpty()) out.tls->certificate_authority_path.clear();
        if (!out.token->secret.isEmpty()) out.token->secret_path.clear();
        flush();
        return true;
    }

    bool ParseOpenConnectProfile(const QString& text, openconnect& out)
    {
        return ParseOpenConnectProfile(text, out, nullptr);
    }
}
