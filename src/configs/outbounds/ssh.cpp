#include "include/configs/outbounds/ssh.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QRegularExpression>
#include <QUrlQuery>
#include <QtEndian>
#include <include/global/Utils.hpp>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/utils.h"

namespace Configs {
    namespace {
        // sing-box listables also take a bare string as one item.
        QStringList sshListableItems(const QJsonValue& value)
        {
            return value.isString() ? QStringList{value.toString()} : QJsonArray2QListString(value.toArray());
        }

        // Older editor saves split on ',' untrimmed, so a trailing comma stored an empty item the core rejects.
        QStringList sshNonBlankItems(const QJsonValue& value)
        {
            QStringList items;
            for (const auto& item : sshListableItems(value)) {
                if (const auto trimmed = item.trimmed(); !trimmed.isEmpty()) items << trimmed;
            }
            return items;
        }

        // The algorithm name a wire-format public key starts with, or "" when the text is not padded base64 of one.
        QString sshKeyBlobType(const QString& text)
        {
            if (text.isEmpty() || text.size() % 4 != 0) return {};
            const auto decoded = QByteArray::fromBase64Encoding(text.toLatin1(), QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors);
            if (!decoded || decoded.decoded.size() < 4) return {};
            const auto& blob = decoded.decoded;
            const auto length = qFromBigEndian<quint32>(blob.constData());
            if (length == 0 || length > quint32(blob.size() - 4)) return {};
            return QString::fromLatin1(blob.mid(4, int(length)));
        }

        // The link carries each list item as its own base64 text, joined by '-'.
        QStringList sshLinkItems(const QString& raw, const QString& field)
        {
            QStringList items;
            for (const auto& item : raw.split("-", Qt::SkipEmptyParts)) {
                // ExportToLink() omits the padding.
                auto padded = item.toUtf8();
                while (padded.size() % 4) padded.append('=');
                const auto decoded = QByteArray::fromBase64Encoding(padded, QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors);
                if (decoded && !decoded.decoded.isEmpty()) {
                    items << QString::fromUtf8(decoded.decoded).trimmed();
                } else {
                    ParseDiag::Warn(field, QCoreApplication::translate("Validation", "An entry that is not valid base64 was ignored."), item);
                }
            }
            return items;
        }
    }

    namespace Checks {
        // x/crypto ssh.ParsePrivateKey reads PEM blocks: RSA, EC, DSA, PKCS#8 and OpenSSH private keys.
        QString SshPrivateKey(const QString& text)
        {
            const auto key = text.trimmed();
            if (key.isEmpty() || (key.contains("-----BEGIN ") && key.contains("PRIVATE KEY-----"))) return {};
            static const QRegularExpression publicKey(QStringLiteral("^(\\S+\\s+)?(ssh-|ecdsa-sha2-|sk-)\\S+\\s+AAAA"));
            if (publicKey.match(key).hasMatch()) {
                return QCoreApplication::translate("Validation", "This is a public key; paste the private key, the block that starts with -----BEGIN and ends with PRIVATE KEY-----.");
            }
            if (key.startsWith("PuTTY-User-Key-File")) {
                return QCoreApplication::translate("Validation", "PuTTY keys are not supported; export the key in OpenSSH format with PuTTYgen.");
            }
            return QCoreApplication::translate("Validation", "Not a private key: paste the whole block from -----BEGIN to the PRIVATE KEY----- line.");
        }

        // x/crypto ssh.ParseAuthorizedKey: the declared type must match the blob, after at most one options field (a known_hosts host list too).
        QString SshHostKey(const QString& item)
        {
            const auto line = item.trimmed();
            const auto typed = [](const QString& text) {
                const auto fields = text.simplified().split(' ');
                return fields.size() >= 2 && sshKeyBlobType(fields[1]) == fields[0];
            };
            if (typed(line)) return {};
            bool quoted = false;
            qsizetype end = 0;
            for (; end < line.size(); ++end) {
                const auto c = line[end];
                if (!quoted && (c == ' ' || c == '\t')) break;
                if (c == '"' && (end == 0 || line[end - 1] != '\\')) quoted = !quoted;
            }
            if (end < line.size() && typed(line.mid(end))) return {};
            return QCoreApplication::translate("Validation", "Use the server's public key line, for example ssh-ed25519 AAAAC3Nza... as printed by ssh-keyscan.");
        }

        QString SshClientVersion(const QString& text)
        {
            const auto version = text.trimmed();
            if (version.isEmpty() || version.startsWith("SSH-2.0-")) return {};
            return QCoreApplication::translate("Validation", "Servers expect a version starting with SSH-2.0-, for example SSH-2.0-OpenSSH_9.6.");
        }

        QString SshAuthentication(const QString& password, const QString& privateKey, const QString& privateKeyPath)
        {
            if (!password.isEmpty() || !privateKey.trimmed().isEmpty() || !privateKeyPath.trimmed().isEmpty()) return {};
            return QCoreApplication::translate("Validation", "Without a password or a private key, only a server that allows logging in without authentication accepts this profile.");
        }

        QString SshKeySources(const QString& privateKey, const QString& privateKeyPath)
        {
            if (privateKey.trimmed().isEmpty() || privateKeyPath.trimmed().isEmpty()) return {};
            return QCoreApplication::translate("Validation", "Both a pasted key and a key file are set; the pasted key is used.");
        }
    }

    bool ssh::ParseFromLink(const QString& link)
    {
        auto url = QUrl(link);
        if (!url.isValid()) return false;
        auto query = QUrlQuery(url.query());

        outbound::ParseFromLink(link);

        if (query.hasQueryItem("user")) user = query.queryItemValue("user", QUrl::FullyDecoded);
        if (query.hasQueryItem("password")) password = query.queryItemValue("password", QUrl::FullyDecoded);
        
        QString privateKeyB64 = query.queryItemValue("private_key");
        if (!privateKeyB64.isEmpty()) {
            private_key = QByteArray::fromBase64(privateKeyB64.toUtf8(), QByteArray::OmitTrailingEquals);
        }
        if (query.hasQueryItem("private_key_path")) private_key_path = query.queryItemValue("private_key_path", QUrl::FullyDecoded);
        if (query.hasQueryItem("private_key_passphrase")) private_key_passphrase = query.queryItemValue("private_key_passphrase", QUrl::FullyDecoded);
        
        host_key << sshLinkItems(query.queryItemValue("host_key"), "host_key");
        host_key_algorithms << sshLinkItems(query.queryItemValue("host_key_algorithms"), "host_key_algorithms");

        if (query.hasQueryItem("client_version")) client_version = query.queryItemValue("client_version", QUrl::FullyDecoded).trimmed();

        // The standard ssh://user:password@host form, when the query does not name them.
        if (!query.hasQueryItem("user") && !url.userName().isEmpty()) user = url.userName(QUrl::FullyDecoded);
        if (!query.hasQueryItem("password") && !url.password().isEmpty()) password = url.password(QUrl::FullyDecoded);

        return !server.isEmpty();
    }

    bool ssh::ParseFromJson(const QJsonObject& object)
    {
        if (object.isEmpty() || object["type"].toString() != "ssh") return false;
        outbound::ParseFromJson(object);
        if (object.contains("user")) user = object["user"].toString();
        if (object.contains("password")) password = object["password"].toString();
        // The core joins private_key lines with '\n'.
        if (object.contains("private_key")) private_key = sshListableItems(object["private_key"]).join('\n');
        if (object.contains("private_key_path")) private_key_path = object["private_key_path"].toString();
        if (object.contains("private_key_passphrase")) private_key_passphrase = object["private_key_passphrase"].toString();
        if (object.contains("host_key")) host_key = sshNonBlankItems(object["host_key"]);
        if (object.contains("host_key_algorithms")) host_key_algorithms = sshNonBlankItems(object["host_key_algorithms"]);
        if (object.contains("client_version")) client_version = object["client_version"].toString();
        return true;
    }

    bool ssh::ParseFromClash(const clash::Proxies& object)
    {
        if (object.type != "ssh") return false;
        outbound::ParseFromClash(object);
        user = QString::fromStdString(object.username);
        password = QString::fromStdString(object.password);
        if (!object.private_key.empty()) private_key = QString::fromStdString(object.private_key);
        if (!object.private_key_passphrase.empty()) private_key_passphrase = QString::fromStdString(object.private_key_passphrase);
        for (const auto& s : object.host_key) {
            if (const auto key = QString::fromStdString(s).trimmed(); !key.isEmpty()) host_key.append(key);
        }
        for (const auto& s : object.host_key_algorithms) {
            if (const auto algorithm = QString::fromStdString(s).trimmed(); !algorithm.isEmpty()) host_key_algorithms.append(algorithm);
        }

        return true;
    }

    QString ssh::ExportToLink()
    {
        QUrl url;
        QUrlQuery query;
        url.setScheme("ssh");
        url.setHost(server);
        url.setPort(server_port);
        if (!name.isEmpty()) url.setFragment(name);

        if (!user.isEmpty()) query.addQueryItem("user", user);
        if (!password.isEmpty()) query.addQueryItem("password", password);
        if (!private_key.isEmpty()) {
            query.addQueryItem("private_key", private_key.toUtf8().toBase64(QByteArray::OmitTrailingEquals));
        }
        if (!private_key_path.isEmpty()) query.addQueryItem("private_key_path", private_key_path);
        if (!private_key_passphrase.isEmpty()) query.addQueryItem("private_key_passphrase", private_key_passphrase);
        
        if (!host_key.isEmpty()) {
            QStringList b64HostKeys;
            for (const auto& item : host_key) {
                b64HostKeys.append(item.toUtf8().toBase64(QByteArray::OmitTrailingEquals));
            }
            query.addQueryItem("host_key", b64HostKeys.join("-"));
        }
        
        if (!host_key_algorithms.isEmpty()) {
            QStringList b64HostKeyAlgs;
            for (const auto& item : host_key_algorithms) {
                b64HostKeyAlgs.append(item.toUtf8().toBase64(QByteArray::OmitTrailingEquals));
            }
            query.addQueryItem("host_key_algorithms", b64HostKeyAlgs.join("-"));
        }
        
        if (!client_version.isEmpty()) query.addQueryItem("client_version", client_version);
        
        mergeUrlQuery(query, outbound::ExportToLink());
        
        if (!query.isEmpty()) url.setQuery(query);
        return url.toString(QUrl::FullyEncoded);
    }

    QJsonObject ssh::ExportToJson()
    {
        QJsonObject object;
        object["type"] = "ssh";
        mergeJsonObjects(object, outbound::ExportToJson());
        if (!user.isEmpty()) object["user"] = user;
        if (!password.isEmpty()) object["password"] = password;
        if (!private_key.isEmpty()) object["private_key"] = private_key;
        if (!private_key_path.isEmpty()) object["private_key_path"] = private_key_path;
        if (!private_key_passphrase.isEmpty()) object["private_key_passphrase"] = private_key_passphrase;
        if (!host_key.isEmpty()) object["host_key"] = QListStr2QJsonArray(host_key);
        if (!host_key_algorithms.isEmpty()) object["host_key_algorithms"] = QListStr2QJsonArray(host_key_algorithms);
        if (!client_version.isEmpty()) object["client_version"] = client_version;
        return object;
    }

    BuildResult ssh::Build()
    {
        QJsonObject object;
        object["type"] = "ssh";
        const auto base = outbound::Build();
        if (!base.error.isEmpty()) return {{}, base.error};
        mergeJsonObjects(object, base.object);
        if (!user.isEmpty()) object["user"] = user;
        if (!password.isEmpty()) object["password"] = password;
        if (!private_key.isEmpty()) object["private_key"] = private_key;
        if (!private_key_path.isEmpty()) object["private_key_path"] = private_key_path;
        if (!private_key_passphrase.isEmpty()) object["private_key_passphrase"] = private_key_passphrase;
        if (!host_key.isEmpty()) object["host_key"] = QListStr2QJsonArray(host_key);
        if (!host_key_algorithms.isEmpty()) object["host_key_algorithms"] = QListStr2QJsonArray(host_key_algorithms);
        if (!client_version.isEmpty()) object["client_version"] = client_version;
        return {object, ""};
    }

    QString ssh::DisplayType()
    {
        return "SSH";
    }

    SecurityInfo ssh::GetSecurity()
    {
        // Without a host key the core accepts whatever key the server presents.
        if (host_key.isEmpty()) return {QObject::tr("Unverified Host Key"), {}, SecurityLevel::Weak};
        return {QObject::tr("Encrypted"), {}, SecurityLevel::Secure};
    }

    void ssh::Validate(Validation::Issues& issues)
    {
        outbound::Validate(issues);
        using Validation::Severity;
        const auto report = [&issues](const QString& field, const QString& message, const QString& value = {},
                                      Severity severity = Severity::Error) {
            if (!message.isEmpty()) issues << Validation::Issue{severity, field, message, value};
        };
        report("auth", Checks::SshAuthentication(password, private_key, private_key_path), {}, Severity::Warning);
        report("private_key", Checks::SshPrivateKey(private_key));
        report("private_key", Checks::SshKeySources(private_key, private_key_path), {}, Severity::Warning);
        for (const auto& key : host_key) {
            if (const auto message = Checks::SshHostKey(key); !message.isEmpty()) {
                report("host_key", message, key);
                break;
            }
        }
        report("client_version", Checks::SshClientVersion(client_version), client_version, Severity::Warning);
    }
}
