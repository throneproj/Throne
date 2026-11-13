#include "include/configs/outbounds/juicity.h"

#include <QCoreApplication>
#include <QUrlQuery>
#include <include/global/Utils.hpp>

#include "include/configs/common/utils.h"

namespace Configs {
    namespace {
        // juicity cmd/client/run.go: the first decoding that succeeds wins, so 64 hex digits decode as 48 bytes of base64.
        QByteArray juicityPinnedHash(const QString& text)
        {
            const auto data = text.toLatin1();
            if (data.size() % 4 == 0) {
                for (const auto alphabet : {QByteArray::Base64UrlEncoding, QByteArray::Base64Encoding}) {
                    if (auto result = QByteArray::fromBase64Encoding(data, alphabet | QByteArray::AbortOnBase64DecodingErrors)) return result.decoded;
                }
            }
            if (Validation::Hex(text, 0, 0).isEmpty()) return QByteArray::fromHex(data);
            return {};
        }
    }

    namespace Checks {
        QString JuicityPinnedCertChain(const QString& text)
        {
            const auto value = text.trimmed();
            if (value.isEmpty() || juicityPinnedHash(value).size() == 32) return {};
            if (Validation::Hex(value, 64, 64).isEmpty()) {
                return QCoreApplication::translate("Validation", "This is a hexadecimal SHA-256; write it in base64 (44 characters ending in =).");
            }
            return QCoreApplication::translate("Validation", "Use the 32-byte certificate chain hash in base64 (44 characters ending in =), as juicity-server prints it.");
        }
    }

    bool juicity::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);
        uuid = url.userName();
        password = url.password();
        if (query.hasQueryItem("pinned_certchain_sha256")) pinned_certchain_sha256 = query.queryItemValue("pinned_certchain_sha256", QUrl::FullyDecoded).trimmed();
        
        tls->ParseFromLink(link);
        tls->enabled = true; // Juicity always uses tls
        
        if (server_port == 0) server_port = 443;

        return !(uuid.isEmpty() || server.isEmpty());
    }

    bool juicity::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "juicity") return false;
        outbound::ParseFromJson(object);
        if (object.contains("uuid")) uuid = object["uuid"].toString();
        if (object.contains("password")) password = object["password"].toString();
        if (object.contains("pinned_certchain_sha256")) pinned_certchain_sha256 = object["pinned_certchain_sha256"].toString();
        if (object.contains("tls")) tls->ParseFromJson(object["tls"].toObject());
        return true;
    }

    void juicity::Validate(Validation::Issues& issues)
    {
        outbound::Validate(issues);
        if (const auto message = Validation::Uuid(uuid); !message.isEmpty()) {
            issues << Validation::Issue{Validation::Severity::Error, "uuid", message, {}};
        }
        if (const auto message = Checks::JuicityPinnedCertChain(pinned_certchain_sha256); !message.isEmpty()) {
            issues << Validation::Issue{Validation::Severity::Error, "pinned_certchain_sha256", message, pinned_certchain_sha256};
        }
    }

    QString juicity::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setScheme("juicity");
        url.setUserName(uuid);
        if (!password.isEmpty()) url.setPassword(password);
        url.setHost(server);
        url.setPort(server_port);
        if (!name.isEmpty()) url.setFragment(name);

        if (!pinned_certchain_sha256.isEmpty()) query.addQueryItem("pinned_certchain_sha256", pinned_certchain_sha256);
        mergeUrlQuery(query, tls->ExportToLink());
        mergeUrlQuery(query, outbound::ExportToLink());
        
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QJsonObject juicity::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "juicity";
        mergeJsonObjects(object, outbound::ExportToJson());
        if (!uuid.isEmpty()) object["uuid"] = uuid;
        if (!password.isEmpty()) object["password"] = password;
        if (!pinned_certchain_sha256.isEmpty()) object["pinned_certchain_sha256"] = pinned_certchain_sha256;
        if (tls->enabled) object["tls"] = tls->ExportToJson();
        return object;
    }

    BuildResult juicity::Build()
    {
        QJsonObject object;
        object["type"] = "juicity";
        const auto base = outbound::Build();
        if (!base.error.isEmpty()) return {{}, base.error};
        mergeJsonObjects(object, base.object);
        if (!uuid.isEmpty()) object["uuid"] = uuid;
        if (!password.isEmpty()) object["password"] = password;
        if (!pinned_certchain_sha256.isEmpty()) object["pinned_certchain_sha256"] = pinned_certchain_sha256;
        if (tls->enabled) {
            const auto tlsResult = tls->Build();
            if (!tlsResult.error.isEmpty()) return {{}, tlsResult.error};
            object["tls"] = tlsResult.object;
        }
        return {object, ""};
    }

    QString juicity::DisplayType()
    {
        return "Juicity";
    }

    SecurityInfo juicity::GetSecurity()
    {
        auto info = SecurityFromTLS("QUIC");
        // Like a certificate pin: the core checks the chain hash in place of the CA chain, insecure or not.
        if (!pinned_certchain_sha256.isEmpty() && !tls->reality->enabled) {
            info.label = QObject::tr("TLS");
            info.level = SecurityLevel::Secure;
            info.caVerified = false;
        }
        return info;
    }
}
