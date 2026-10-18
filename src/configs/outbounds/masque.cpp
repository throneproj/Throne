#include "include/configs/outbounds/masque.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <include/global/Utils.hpp>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"

namespace Configs {
    namespace {
        const QStringList masqueHttpVersions = {"0", "2", "3"};

        // Without a diagnostics scope (DB loads, clones) a parse keeps every value as it always did.
        bool masqueKeep(const QString& field, const QString& value, const Validation::Rule& rule)
        {
            return !ParseDiag::Active() || AcceptOptional(field, rule(value), value);
        }

        void masqueReport(Validation::Issues& issues, const QString& field, const QString& message, const QString& value = {})
        {
            if (!message.isEmpty()) issues << Validation::Issue{Validation::Severity::Error, field, message, value};
        }
    }

    namespace Checks {
        QString MasqueKey(const QString& text, Validation::Empty empty)
        {
            const QString t = text.trimmed();
            if (t.isEmpty()) return empty == Validation::Empty::Required ? Validation::Required(t) : QString();
            if (t.size() % 4 == 0 && Validation::Base64(t).isEmpty()) return {};
            return QCoreApplication::translate("Validation", "Must be the base64 key from WARP registration (standard base64 with = padding).");
        }
    }

    QStringList masque::HostPrefixed(const QStringList& addresses)
    {
        QStringList result;
        for (const auto& raw : addresses) {
            const auto address = raw.trimmed();
            if (address.isEmpty()) continue;
            if (address.contains('/')) result << address;
            else result << address + (address.contains(':') ? "/128" : "/32");
        }
        return result;
    }

    bool masque::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "masque") return false;
        outbound::ParseFromJson(object);
        if (object.contains("private_key")) private_key = object["private_key"].toString();
        if (object.contains("peer_public_key")) peer_public_key = object["peer_public_key"].toString();
        // Listable: the core accepts a bare string as well as an array.
        if (object.contains("address")) address = HostPrefixed(JsonStringList(object["address"], "address"));
        if (ParseDiag::Active()) {
            QStringList kept;
            for (const auto& item : std::as_const(address)) {
                if (AcceptOptional("address", Validation::Cidr(item), item)) kept << item;
            }
            address = kept;
        }
        if (object.contains("mtu")) mtu = object["mtu"].toInt();
        if (object.contains("http_version")) {
            const int version = object["http_version"].toInt();
            if (masqueKeep("http_version", QString::number(version), Validation::Rules::OneOf(masqueHttpVersions))) http_version = version;
        }
        if (object.contains("disable_version_fallback")) disable_version_fallback = object["disable_version_fallback"].toBool();
        if (object.contains("tls")) {
            auto tlsObject = object["tls"].toObject();
            tls->ParseFromJson(tlsObject);
            // An absent SNI means the server address (as in sing-box), not the constructor's WARP default.
            if (!tlsObject.contains("server_name")) tls->server_name.clear();
        }
        tls->enabled = true;
        quic->ParseFromJson(object);
        return true;
    }

    bool masque::ParseFromClash(const clash::Proxies& object)
    {
        if (object.type != "masque") return false;
        outbound::ParseFromClash(object);
        if (server_port == 0) server_port = 443;

        private_key = QString::fromStdString(object.private_key);
        peer_public_key = QString::fromStdString(object.public_key);
        address << HostPrefixed({QString::fromStdString(object.ip), QString::fromStdString(object.ipv6)});
        if (object.mtu > 0) mtu = object.mtu;

        if (!object.sni.empty()) tls->server_name = QString::fromStdString(object.sni);
        tls->insecure = object.skip_cert_verify;
        tls->enabled = true;

        // mihomo never falls back between carriers.
        if (object.network == "h2") {
            http_version = 2;
            disable_version_fallback = false;
        } else {
            http_version = 3;
            disable_version_fallback = true;
        }
        return !private_key.isEmpty();
    }

    void masque::Validate(Validation::Issues& issues)
    {
        outbound::Validate(issues);
        masqueReport(issues, "private_key", Checks::MasqueKey(private_key, Validation::Empty::Required));
        masqueReport(issues, "peer_public_key", Checks::MasqueKey(peer_public_key, Validation::Empty::Allowed));
        if (address.isEmpty()) {
            masqueReport(issues, "address", QCoreApplication::translate("Validation", "At least one local address is required, for example 10.0.0.2/32."));
        } else {
            masqueReport(issues, "address", Validation::EachItem(address.join(','), Validation::Split::Comma, Validation::Rules::Cidr()));
        }
        const auto version = QString::number(http_version);
        masqueReport(issues, "http_version", Validation::OneOf(version, masqueHttpVersions), version);
    }

    QString masque::ExportToLink()
    {
        return {};
    }

    QJsonObject masque::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "masque";
        mergeJsonObjects(object, outbound::ExportToJson());
        if (!private_key.isEmpty()) object["private_key"] = private_key;
        if (!peer_public_key.isEmpty()) object["peer_public_key"] = peer_public_key;
        if (!address.isEmpty()) object["address"] = QListStr2QJsonArray(address);
        if (mtu > 0) object["mtu"] = mtu;
        if (http_version != 0) object["http_version"] = http_version;
        if (disable_version_fallback) object["disable_version_fallback"] = true;
        object["tls"] = tls->ExportToJson();
        mergeJsonObjects(object, quic->ExportToJson());
        return object;
    }

    QJsonObject masque::ExportIdentity()
    {
        // WARP identities share one server address; the key is what tells them apart.
        auto object = outbound::ExportIdentity();
        object["private_key"] = private_key;
        return object;
    }

    BuildResult masque::Build()
    {
        if (private_key.isEmpty()) return {{}, QObject::tr("%1: the private key is empty").arg(DisplayTypeAndName())};
        if (address.isEmpty()) return {{}, QObject::tr("%1: no address is set").arg(DisplayTypeAndName())};

        tls->enabled = true;
        QJsonObject object;
        object["type"] = "masque";
        if (!name.isEmpty()) object["tag"] = name;
        const auto base = outbound::Build();
        if (!base.error.isEmpty()) return {{}, base.error};
        mergeJsonObjects(object, base.object);
        object["private_key"] = private_key;
        if (!peer_public_key.isEmpty()) object["peer_public_key"] = peer_public_key;
        object["address"] = QListStr2QJsonArray(address);
        if (mtu > 0) object["mtu"] = mtu;
        if (http_version != 0) object["http_version"] = http_version;
        if (disable_version_fallback) object["disable_version_fallback"] = true;
        // TLS::Build fails only on REALITY, which is dropped here.
        auto tlsObject = tls->Build().object;
        tlsObject.remove("reality");
        object["tls"] = tlsObject;
        const auto quicResult = quic->Build();
        if (!quicResult.error.isEmpty()) return {{}, quicResult.error};
        mergeJsonObjects(object, quicResult.object);
        return {object, ""};
    }

    QString masque::DisplayType()
    {
        return "MASQUE";
    }

    SecurityInfo masque::GetSecurity()
    {
        // The peer key pins the server certificate, which the core enforces even when insecure is set.
        if (!peer_public_key.isEmpty()) return {QObject::tr("TLS"), {}, SecurityLevel::Secure};
        return SecurityFromTLS({});
    }

    bool masque::IsEndpoint()
    {
        return true;
    }
}
