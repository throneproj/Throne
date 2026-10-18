#include "include/configs/outbounds/shadowsocks.h"

#include <QCoreApplication>
#include <QHash>
#include <QUrlQuery>
#include <algorithm>
#include <include/global/Utils.hpp>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"

namespace Configs {
    namespace {
        // Share links use either base64 alphabet.
        QString shadowsocksDecodeBase64(const QString &text) {
            auto decoded = DecodeB64IfValid(text, QByteArray::Base64UrlEncoding);
            if (decoded.isEmpty()) decoded = DecodeB64IfValid(text, QByteArray::Base64Encoding);
            return QString::fromUtf8(decoded);
        }

        // The core matches method names exactly; links and subscriptions often upper-case them.
        QString shadowsocksMethodName(const QString &method) {
            return method.trimmed().toLower();
        }

        // simple-obfs and Clash's obfs are the obfs-local plugin the core registers.
        QString shadowsocksPluginName(const QString &plugin) {
            const QString name = plugin.trimmed();
            if (name == "simple-obfs" || name == "obfs") return QStringLiteral("obfs-local");
            return name;
        }

        // sing-box transport/sip003/args.go ParsePluginOptions: ';'-separated key[=value], backslash escapes, first value wins.
        QString shadowsocksPluginArgs(const QString &opts, QHash<QString, QString> &args) {
            const qsizetype n = opts.size();
            qsizetype i = 0;
            const auto read = [&](const QString &terms, QString &out) {
                for (; i < n && !terms.contains(opts[i]); ++i) {
                    if (opts[i] == u'\\' && ++i == n) return false;
                    out += opts[i];
                }
                return true;
            };
            const auto dangling = [] { return QCoreApplication::translate("Validation", "A backslash at the end has nothing to escape."); };
            while (i < n) {
                QString key, value = QStringLiteral("1");
                if (!read(QStringLiteral("=;"), key)) return dangling();
                if (key.isEmpty()) return QCoreApplication::translate("Validation", "Every option needs a name, as in obfs=http;obfs-host=example.com.");
                if (i < n && opts[i] == u'=') {
                    ++i;
                    value.clear();
                    if (!read(QStringLiteral(";"), value)) return dangling();
                }
                ++i;
                if (!args.contains(key)) args.insert(key, value);
            }
            return {};
        }

        // strconv.Atoi: one optional + or - sign, then decimal digits, no spaces.
        bool shadowsocksAtoi(const QString &text) {
            const bool plus = text.startsWith(u'+');
            const QString rest = plus ? text.mid(1) : text;
            return rest == rest.trimmed() && !(plus && rest.startsWith(u'-')) && Validation::ToInt(rest).has_value();
        }

        QString shadowsocksKeyMessage(const QString &key, int bytes) {
            // The core decodes the key as written, so a space anywhere breaks it.
            if (std::any_of(key.begin(), key.end(), [](QChar c) { return c.isSpace(); }))
                return QCoreApplication::translate("Validation", "Must not contain spaces.");
            return Validation::Base64Key(key, bytes);
        }
    }

    namespace Checks {
        QString ShadowsocksPassword(const QString &method, const QString &password) {
            if (method == "none") return {};
            const bool aes128 = method == "2022-blake3-aes-128-gcm";
            const bool chacha = method == "2022-blake3-chacha20-poly1305";
            if (!aes128 && !chacha && method != "2022-blake3-aes-256-gcm") return password.isEmpty() ? Validation::Required(password) : QString();
            const QStringList keys = password.split(u':');
            if (chacha && keys.size() > 1)
                return QCoreApplication::translate("Validation", "2022-blake3-chacha20-poly1305 takes a single key; several keys joined with : need an AES 2022 method.");
            for (qsizetype i = 0; i < keys.size(); ++i) {
                const QString message = shadowsocksKeyMessage(keys[i], aes128 ? 16 : 32);
                if (message.isEmpty()) continue;
                if (keys.size() == 1) return message;
                return QCoreApplication::translate("Validation", "Item %1: %2").arg(QString::number(i + 1), message);
            }
            return {};
        }

        QString ShadowsocksPluginOpts(const QString &plugin, const QString &opts) {
            if (plugin.isEmpty()) return {};
            QHash<QString, QString> args;
            if (auto message = shadowsocksPluginArgs(opts, args); !message.isEmpty()) return message;
            if (plugin == "obfs-local") {
                const QString mode = args.value(QStringLiteral("obfs"), QStringLiteral("http"));
                if (mode != "http" && mode != "tls")
                    return QCoreApplication::translate("Validation", "Unknown obfs mode \"%1\". Use obfs=http or obfs=tls.").arg(mode);
            } else if (plugin == "v2ray-plugin") {
                const QString mode = args.value(QStringLiteral("mode"), QStringLiteral("websocket"));
                if (mode != "websocket" && mode != "quic")
                    return QCoreApplication::translate("Validation", "Unknown mode \"%1\". Use mode=websocket or mode=quic.").arg(mode);
                if (mode == "websocket" && args.contains(QStringLiteral("mux")) && !shadowsocksAtoi(args.value(QStringLiteral("mux"))))
                    return QCoreApplication::translate("Validation", "mux must be a whole number, for example mux=1.");
            }
            return {};
        }
    }

    bool shadowsocks::ParseFromLink(const QString& link)
    {
        QUrl url;
        if (SubStrBefore(link, "#").contains("@")) {
            url = QUrl(link);
        } else {
            // v2rayN format: base64 encoded full URL
            QString linkN = shadowsocksDecodeBase64(SubStrBefore(SubStrAfter(link, "://"), "#"));
            if (linkN.isEmpty()) {
                ParseDiag::Error("userinfo", QCoreApplication::translate("Validation", "The link is neither ss://method:password@host:port nor valid base64."));
                return false;
            }
            if (link.contains("#")) linkN += "#" + SubStrAfter(link, "#");
            url = QUrl("https://" + linkN);
        }
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());
        outbound::ParseFromLink(url.toString());

        if (url.password().isEmpty()) {
            // Traditional format: method:password base64 encoded in username
            auto method_password = shadowsocksDecodeBase64(url.userName());
            if (method_password.isEmpty()) {
                ParseDiag::Error("userinfo", QCoreApplication::translate("Validation", "The method and password are not valid base64."));
                return false;
            }
            // Without the colon the decoded text may be the bare password, which Validate would echo as the method.
            if (!method_password.contains(':')) {
                ParseDiag::Error("userinfo", QCoreApplication::translate("Validation", "The decoded userinfo must be method:password, for example aes-256-gcm:secret."));
                return false;
            }
            method = SubStrBefore(method_password, ":");
            password = SubStrAfter(method_password, ":");
        } else {
            // 2022 format: method in username, password in password
            method = url.userName();
            password = url.password();
        }
        method = shadowsocksMethodName(method);

        const auto pluginSpec = query.queryItemValue("plugin", QUrl::FullyDecoded);
        plugin = shadowsocksPluginName(SubStrBefore(pluginSpec, ";"));
        plugin_opts = pluginSpec.contains(';') ? SubStrAfter(pluginSpec, ";").trimmed() : QString();
        if (query.hasQueryItem("plugin-opts")) plugin_opts = query.queryItemValue("plugin-opts", QUrl::FullyDecoded).trimmed();
        if (query.hasQueryItem("uot")) uot = query.queryItemValue("uot") == "true" || query.queryItemValue("uot").toInt() > 0;
        multiplex->ParseFromLink(link);

        return !server.isEmpty();
    }

    bool shadowsocks::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "shadowsocks") return false;
        outbound::ParseFromJson(object);
        if (object.contains("method")) method = object["method"].toString();
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("plugin")) plugin = object["plugin"].toString();
        if (object.contains("plugin_opts")) plugin_opts = object["plugin_opts"].toString();
        if (ParseDiag::Active()) {
            method = shadowsocksMethodName(method);
            plugin = shadowsocksPluginName(plugin);
        }
        if (object.contains("udp_over_tcp"))
        {
            if (object["udp_over_tcp"].isBool()) uot = object["udp_over_tcp"].toBool();
            if (object["udp_over_tcp"].isObject()) uot = object["udp_over_tcp"].toObject()["enabled"].toBool();
        }
        if (object.contains("multiplex")) multiplex->ParseFromJson(object["multiplex"].toObject());
        return true;
    }

    bool shadowsocks::ParseFromClash(const clash::Proxies& object)
    {
        if (object.type != "ss") return false;
        outbound::ParseFromClash(object);
        method = shadowsocksMethodName(QString::fromStdString(object.cipher));
        password = QString::fromStdString(object.password);
        uot = object.udp_over_tcp;
        if (!object.plugin.empty()) {
            if (object.plugin != "v2ray-plugin" && object.plugin != "obfs") {
                ParseDiag::Error("plugin", QCoreApplication::translate("Validation", "Unsupported plugin; only obfs and v2ray-plugin work."), QString::fromStdString(object.plugin));
            } else if (object.plugin == "v2ray-plugin") {
                plugin = "v2ray-plugin";
                QStringList ssPlugin;
                clash::v2rayPlugin plugin_config = object.plugin_opts.get_value<clash::v2rayPlugin>();
                if (plugin_config.tls) ssPlugin << "tls";
                if (!plugin_config.host.empty()) ssPlugin << "host=" + QString::fromStdString(plugin_config.host);
                if (!plugin_config.path.empty()) ssPlugin << "path=" + QString::fromStdString(plugin_config.path);
                if (!plugin_config.mode.empty()) ssPlugin << "mode=" + QString::fromStdString(plugin_config.mode);
                if (plugin_config.mux) ssPlugin << "mux";
                plugin_opts = ssPlugin.join(";");
            } else if (object.plugin == "obfs") {
                plugin = "obfs-local";
                QStringList ssPlugin;
                clash::obfs plugin_config = object.plugin_opts.get_value<clash::obfs>();
                if (!plugin_config.mode.empty()) ssPlugin << "obfs=" + QString::fromStdString(plugin_config.mode);
                if (!plugin_config.host.empty()) ssPlugin << "obfs-host=" + QString::fromStdString(plugin_config.host);
                plugin_opts = ssPlugin.join(";");
            }
        }

        multiplex->ParseFromClash(object);
        return true;
    }

    bool shadowsocks::ParseFromSIP008(const QJsonObject& object)
    {
        if (object.isEmpty()) return false;
        outbound::ParseFromJson(object);
        if (object.contains("remarks")) name = object["remarks"].toString();
        if (object.contains("method")) method = shadowsocksMethodName(object["method"].toString());
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("plugin")) plugin = shadowsocksPluginName(object["plugin"].toString());
        if (object.contains("plugin_opts")) plugin_opts = object["plugin_opts"].toString().trimmed();
        if (object.contains("uot"))
        {
            if (object["uot"].isBool()) uot = object["uot"].toBool();
            if (object["uot"].isObject()) uot = object["uot"].toObject()["enabled"].toBool();
        }
        if (object.contains("multiplex")) multiplex->ParseFromJson(object["multiplex"].toObject());
        return !server.isEmpty();
    }

    QString shadowsocks::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setScheme("ss");
        
        if (method.startsWith("2022-")) {
            url.setUserName(method);
            url.setPassword(password);
        } else {
            auto method_password = method + ":" + password;
            url.setUserName(method_password.toUtf8().toBase64(QByteArray::Base64Option::Base64UrlEncoding));
        }
        
        url.setHost(server);
        url.setPort(server_port);
        if (!name.isEmpty()) url.setFragment(name);
        
        if (!plugin.isEmpty()) {
            QString pluginString = plugin;
            if (!plugin_opts.isEmpty()) pluginString.append(";" + plugin_opts);
            query.addQueryItem("plugin", QUrl::toPercentEncoding(pluginString));
        }
        if (uot) query.addQueryItem("uot", "1");
        
        mergeUrlQuery(query, multiplex->ExportToLink());
        mergeUrlQuery(query, outbound::ExportToLink());
        
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QJsonObject shadowsocks::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "shadowsocks";
        mergeJsonObjects(object, outbound::ExportToJson());
        if (!method.isEmpty()) object["method"] = method;
        if (!password.isEmpty()) object["password"] = password;
        if (!plugin.isEmpty()) object["plugin"] = plugin;
        if (!plugin_opts.isEmpty()) object["plugin_opts"] = plugin_opts;
        if (uot) object["udp_over_tcp"] = uot;
        if (auto muxObj = multiplex->ExportToJson(); !muxObj.isEmpty()) object["multiplex"] = muxObj;
        return object;
    }

    BuildResult shadowsocks::Build()
    {
        if (plugin.contains(";")) {
            plugin_opts = SubStrAfter(plugin, ";");
            plugin = SubStrBefore(plugin, ";");
        }
        QJsonObject object;
        object["type"] = "shadowsocks";
        const auto base = outbound::Build();
        if (!base.error.isEmpty()) return {{}, base.error};
        mergeJsonObjects(object, base.object);
        if (!method.isEmpty()) object["method"] = method;
        if (!password.isEmpty()) object["password"] = password;
        if (!plugin.isEmpty()) object["plugin"] = plugin;
        if (!plugin_opts.isEmpty()) object["plugin_opts"] = plugin_opts;
        if (uot) object["udp_over_tcp"] = uot;
        const auto muxResult = multiplex->Build();
        if (!muxResult.error.isEmpty()) return {{}, muxResult.error};
        if (!muxResult.object.isEmpty()) object["multiplex"] = muxResult.object;
        return {object, ""};
    }

    QString shadowsocks::DisplayType()
    {
        return "Shadowsocks";
    }

    void shadowsocks::Validate(Validation::Issues &issues)
    {
        outbound::Validate(issues);
        using Validation::Severity;
        if (auto message = Validation::OneOf(method, shadowsocksMethods, Validation::Empty::Required); !message.isEmpty()) {
            issues.append({Severity::Error, "method", message, method});
        } else if (auto message = Checks::ShadowsocksPassword(method, password); !message.isEmpty()) {
            issues.append({Severity::Error, "password", message});
        }
        // Build() still splits a legacy "name;opts" plugin value.
        const QString pluginName = SubStrBefore(plugin, ";");
        const QString opts = plugin.contains(';') ? SubStrAfter(plugin, ";") : plugin_opts;
        if (auto message = Validation::OneOf(pluginName, shadowsocksPlugins); !message.isEmpty()) {
            issues.append({Severity::Error, "plugin", message, pluginName});
        } else if (auto message = Checks::ShadowsocksPluginOpts(pluginName, opts); !message.isEmpty()) {
            issues.append({Severity::Error, "plugin_opts", message, opts});
        }
    }

    SecurityInfo shadowsocks::GetSecurity()
    {
        SecurityInfo info;
        if (method.isEmpty() || method == "none") {
            info.label = QObject::tr("Raw");
            info.level = SecurityLevel::None;
            return info;
        }
        // Pre-AEAD stream ciphers: no integrity, trivially detectable.
        static const QStringList streamCiphers = {
            "aes-128-ctr", "aes-192-ctr", "aes-256-ctr",
            "aes-128-cfb", "aes-192-cfb", "aes-256-cfb",
            "rc4-md5", "chacha20-ietf", "xchacha20",
        };
        if (streamCiphers.contains(method)) {
            info.label = QObject::tr("Weak Cipher");
            info.level = SecurityLevel::Weak;
            return info;
        }
        info.label = QObject::tr("Encrypted");
        info.level = SecurityLevel::Secure;
        return info;
    }
}
