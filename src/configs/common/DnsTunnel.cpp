#include "include/configs/common/DnsTunnel.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QUrl>

#include <algorithm>
#include <cmath>

#include "include/configs/ParseDiagnostics.h"
#include "include/configs/common/ResolverSource.h"
#include "include/configs/common/utils.h"
#include "include/database/DatabaseManager.h"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/global/Validation.hpp"

namespace Configs {
    namespace {
        constexpr int kDnsTunnelStartupMs = 30000;
        constexpr int kDnsTunnelMasterDnsStartupMs = 90000;
        constexpr int kDnsTunnelMaxDepth = 8;
        constexpr qsizetype kDnstMaxLinkSize = 64 * 1024;

        const QStringList kDnstParams = {"pubkey", "cert", "record-type", "dnstt-compat", "clientid-size", "idle-timeout", "keepalive",
                                         "mkey", "encryption", "user", "password", "pk", "method"};
        const QStringList kDnstTransportParams = {"pubkey", "cert", "record-type", "dnstt-compat", "clientid-size", "idle-timeout", "keepalive",
                                                  "mkey", "encryption"};

        bool dnsTunnelIsXdns(const Profile &profile) {
            const auto *vless = profile.XrayVLESS();
            return vless != nullptr && vless->streamSetting != nullptr && vless->streamSetting->IsXdns();
        }

        int dnsTunnelOwnAllowanceMs(const Profile &profile) {
            if (profile.type == QLatin1String("masterdns")) return kDnsTunnelMasterDnsStartupMs;
            if (profile.type == QLatin1String("dnstt") || dnsTunnelIsXdns(profile)) return kDnsTunnelStartupMs;
            return 0;
        }

        // Stacked tunnels start one after another, so their allowances add up.
        int dnsTunnelHopsAllowanceMs(const std::shared_ptr<Profile> &profile, QSet<int> &visited, int depth) {
            if (profile == nullptr || profile->outbound == nullptr || depth > kDnsTunnelMaxDepth) return 0;
            if (profile->id >= 0) {
                if (visited.contains(profile->id)) return 0;
                visited.insert(profile->id);
            }
            int allowance = dnsTunnelOwnAllowanceMs(*profile);
            if (const auto *chain = profile->Chain(); chain != nullptr) {
                for (const int id : chain->list) allowance += dnsTunnelHopsAllowanceMs(dataManager->profilesRepo->GetProfile(id), visited, depth + 1);
            }
            return allowance;
        }

        bool dnstDerElement(const QByteArray &der, qsizetype pos, quint8 &tag, qsizetype &start, qsizetype &end) {
            if (pos < 0 || pos + 2 > der.size()) return false;
            tag = quint8(der[pos]);
            quint64 length = quint8(der[pos + 1]);
            start = pos + 2;
            if (length & 0x80) {
                const int bytes = int(length & 0x7f);
                if (bytes == 0 || bytes > 4 || start + bytes > der.size()) return false;
                length = 0;
                for (int i = 0; i < bytes; ++i) length = (length << 8) | quint8(der[start + i]);
                start += bytes;
            }
            if (length > quint64(der.size() - start)) return false;
            end = start + qsizetype(length);
            return true;
        }

        // The PEM type of a private key: OpenSSH's own format, or PKCS#8 (plain or encrypted), SEC1, DSA or PKCS#1 told apart by their ASN.1 shape.
        QString dnstPemType(const QByteArray &der) {
            if (der.startsWith(QByteArray("openssh-key-v1\0", 15))) return QStringLiteral("OPENSSH PRIVATE KEY");
            quint8 tag = 0;
            qsizetype start = 0, end = 0;
            if (!dnstDerElement(der, 0, tag, start, end) || tag != 0x30 || end != der.size()) return {};
            QList<quint8> tags;
            QByteArray firstInteger;
            for (qsizetype pos = start; pos < end && tags.size() <= 16;) {
                quint8 childTag = 0;
                qsizetype childStart = 0, childEnd = 0;
                if (!dnstDerElement(der, pos, childTag, childStart, childEnd) || childEnd > end) return {};
                if (tags.isEmpty() && childTag == 0x02) firstInteger = der.mid(childStart, childEnd - childStart);
                tags << childTag;
                pos = childEnd;
            }
            const auto leadingIntegers = std::find_if(tags.cbegin(), tags.cend(), [](quint8 t) { return t != 0x02; }) - tags.cbegin();
            if (tags.size() >= 3 && tags[0] == 0x02 && tags[1] == 0x30 && tags[2] == 0x04) return QStringLiteral("PRIVATE KEY");
            if (tags.size() == 2 && tags[0] == 0x30 && tags[1] == 0x04) return QStringLiteral("ENCRYPTED PRIVATE KEY");
            if (tags.size() >= 2 && tags[0] == 0x02 && tags[1] == 0x04 && firstInteger == QByteArray(1, '\x01')) return QStringLiteral("EC PRIVATE KEY");
            if (tags.size() == 6 && leadingIntegers == 6) return QStringLiteral("DSA PRIVATE KEY");
            if (leadingIntegers >= 9) return QStringLiteral("RSA PRIVATE KEY");
            return {};
        }

        QString dnstJsonText(const QJsonValue &value) {
            if (value.isString()) return value.toString();
            if (value.isBool()) return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
            if (value.isDouble()) {
                const double number = value.toDouble();
                if (std::isfinite(number) && number == std::trunc(number) && std::abs(number) < 1e15) return QString::number(qint64(number));
                return QString::number(number);
            }
            return {};
        }

        QString dnstFormDecode(QString text) {
            text.replace(u'+', u' ');
            return QUrl::fromPercentEncoding(text.toUtf8());
        }

        bool dnstDecode(const QString &link, DnstLink &out, QString *error, bool full) {
            const auto fail = [error](const QString &message) {
                if (error != nullptr) *error = message;
                return false;
            };
            out = DnstLink{};
            if (link.size() > kDnstMaxLinkSize) return fail(QCoreApplication::translate("Validation", "The link is too long."));
            if (!link.startsWith(QLatin1String("dnst://"), Qt::CaseInsensitive)) return fail(QCoreApplication::translate("Validation", "Not a dnst:// link."));
            auto rest = link.mid(7).trimmed();
            QString fragment;
            if (const auto hash = rest.indexOf(u'#'); hash >= 0) {
                fragment = QUrl::fromPercentEncoding(rest.mid(hash + 1).toUtf8()).trimmed();
                rest = rest.left(hash);
            }
            QString query;
            if (const auto mark = rest.indexOf(u'?'); mark >= 0) {
                query = rest.mid(mark + 1);
                rest = rest.left(mark);
            }

            if (rest.contains(u'/')) {
                const auto parts = rest.split(u'/');
                out.tag = fragment;
                out.domain = QUrl::fromPercentEncoding(parts.value(0).toUtf8()).trimmed();
                out.transport = QUrl::fromPercentEncoding(parts.value(1).toUtf8()).trimmed().toLower();
                out.backend = QUrl::fromPercentEncoding(parts.value(2).toUtf8()).trimmed().toLower();
                for (const auto &pair : query.split(u'&', Qt::SkipEmptyParts)) {
                    const auto eq = pair.indexOf(u'=');
                    const auto key = dnstFormDecode(eq < 0 ? pair : pair.left(eq)).trimmed().toLower();
                    if (kDnstParams.contains(key)) out.params.insert(key, eq < 0 ? QString() : dnstFormDecode(pair.mid(eq + 1)));
                }
            } else {
                const auto bytes = DnsTunnelBase64(rest);
                if (bytes.isEmpty()) return fail(QCoreApplication::translate("Validation", "The link is damaged: its payload is not valid base64."));
                const auto document = QJsonDocument::fromJson(bytes);
                if (!document.isObject()) return fail(QCoreApplication::translate("Validation", "The link is damaged: its payload is not a JSON object."));
                const auto root = document.object();
                const auto transport = root.value("transport").toObject();
                const auto backend = root.value("backend").toObject();
                out.tag = root.value("tag").toString().trimmed();
                if (out.tag.isEmpty()) out.tag = fragment;
                out.domain = transport.value("domain").toString().trimmed();
                out.transport = transport.value("type").toString().trimmed().toLower();
                out.backend = backend.value("type").toString().trimmed().toLower();
                for (const auto &name : kDnstParams) {
                    const auto key = QString(name).replace(u'-', u'_');
                    const auto &layer = kDnstTransportParams.contains(name) ? transport : backend;
                    if (layer.contains(key)) out.params.insert(name, dnstJsonText(layer.value(key)));
                }
                if (full && root.contains("resolvers")) {
                    out.resolvers = JsonStringList(root.value("resolvers"), QStringLiteral("resolvers"));
                    out.hasResolvers = true;
                }
            }
            if (out.transport.isEmpty()) return fail(QCoreApplication::translate("Validation", "The link does not name the tunnel type."));

            if (full) {
                if (const auto key = out.params.value("pk").trimmed(); !key.isEmpty()) {
                    const auto pem = key.contains(QLatin1String("-----BEGIN ")) ? key : DnstPrivateKeyPem(key);
                    if (pem.isEmpty()) {
                        ParseDiag::Warn("ssh_private_key", QCoreApplication::translate("Validation", "The SSH private key in the link could not be read; it was ignored."));
                        out.params.remove("pk");
                    } else {
                        out.params.insert("pk", pem);
                    }
                }
            }
            return true;
        }
    }

    bool IsDnsTunnelProfile(const Profile &profile) {
        return profile.type == QLatin1String("dnstt") || profile.type == QLatin1String("masterdns") || dnsTunnelIsXdns(profile);
    }

    int DnsTunnelStartupAllowanceMs(const std::shared_ptr<Profile> &profile) {
        if (profile == nullptr || dataManager == nullptr) return 0;
        QSet<int> visited;
        int allowance = dnsTunnelHopsAllowanceMs(profile, visited, 0);
        if (const auto group = dataManager->groupsRepo->GetGroup(profile->gid); group != nullptr) {
            for (const int id : {group->front_proxy_id, group->landing_proxy_id}) {
                if (id >= 0) allowance += dnsTunnelHopsAllowanceMs(dataManager->profilesRepo->GetProfile(id), visited, 1);
            }
        }
        return allowance;
    }

    ResolverSource *ProfileResolverSource(const Profile &profile) {
        if (auto *bean = profile.Dnstt(); bean != nullptr) return &bean->resolvers;
        if (auto *bean = profile.MasterDns(); bean != nullptr) return &bean->resolvers;
        if (dnsTunnelIsXdns(profile)) return &profile.XrayVLESS()->streamSetting->xdnsResolvers;
        return nullptr;
    }

    bool DecodeDnstLink(const QString &link, DnstLink &out, QString *error) {
        return dnstDecode(link, out, error, true);
    }

    QString DnstLinkTransport(const QString &link) {
        DnstLink parsed;
        return dnstDecode(link, parsed, nullptr, false) ? parsed.transport : QString();
    }

    QString EncodeDnstLink(const DnstLink &link, bool asJson) {
        if (asJson) {
            QJsonObject transport{{"type", link.transport}, {"domain", link.domain}};
            QJsonObject backend{{"type", link.backend}};
            for (const auto &name : kDnstParams) {
                const auto value = link.params.value(name);
                if (value.isEmpty()) continue;
                QJsonValue json = value;
                if (name == QLatin1String("dnstt-compat")) json = value == QLatin1String("true");
                else if (name == QLatin1String("clientid-size") || name == QLatin1String("encryption")) json = value.toInt();
                (kDnstTransportParams.contains(name) ? transport : backend).insert(QString(name).replace(u'-', u'_'), json);
            }
            QJsonObject root;
            if (!link.tag.isEmpty()) root["tag"] = link.tag;
            root["transport"] = transport;
            root["backend"] = backend;
            const auto payload = QJsonDocument(root).toJson(QJsonDocument::Compact).toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
            return QStringLiteral("dnst://") + QString::fromLatin1(payload);
        }
        auto text = QStringLiteral("dnst://%1/%2/%3").arg(link.domain, link.transport, link.backend);
        QStringList query;
        for (const auto &name : kDnstParams) {
            auto value = link.params.value(name);
            if (name == QLatin1String("pk") && !value.isEmpty()) value = DnstPrivateKeyParam(value);
            if (value.isEmpty()) continue;
            query << name + QLatin1Char('=') + QString::fromLatin1(QUrl::toPercentEncoding(value));
        }
        if (!query.isEmpty()) text += QLatin1Char('?') + query.join(u'&');
        if (!link.tag.isEmpty()) text += QLatin1Char('#') + QString::fromLatin1(QUrl::toPercentEncoding(link.tag));
        return text;
    }

    QByteArray DnsTunnelBase64(const QString &text) {
        QByteArray data;
        data.reserve(text.size() + 3);
        for (const QChar c : text) {
            const char16_t u = c.unicode();
            if (u == u'\r' || u == u'\n' || u == u'\t') continue;
            if (u >= 128) return {};
            // A '+' that form decoding turned into a space.
            if (u == u' ' || u == u'+') data.append('-');
            else if (u == u'/') data.append('_');
            else data.append(char(u));
        }
        while (data.endsWith('=')) data.chop(1);
        if (data.isEmpty()) return {};
        while (data.size() % 4 != 0) data.append('=');
        const auto decoded = QByteArray::fromBase64Encoding(data, QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
        return decoded ? decoded.decoded : QByteArray();
    }

    QString DnstPrivateKeyPem(const QString &param) {
        const auto der = DnsTunnelBase64(param.trimmed());
        const auto type = der.isEmpty() ? QString() : dnstPemType(der);
        if (type.isEmpty()) return {};
        QStringList lines{QStringLiteral("-----BEGIN %1-----").arg(type)};
        const auto encoded = der.toBase64();
        for (qsizetype i = 0; i < encoded.size(); i += 64) lines << QString::fromLatin1(encoded.mid(i, 64));
        lines << QStringLiteral("-----END %1-----").arg(type);
        return lines.join(u'\n');
    }

    QString DnstPrivateKeyParam(const QString &pem) {
        const auto text = pem.trimmed();
        const auto begin = text.indexOf(QLatin1String("-----BEGIN "));
        const auto labelEnd = begin < 0 ? -1 : text.indexOf(QLatin1String("-----"), begin + 11);
        const auto end = labelEnd < 0 ? -1 : text.indexOf(QLatin1String("-----END "), labelEnd + 5);
        if (end < 0) return {};
        const auto body = text.mid(labelEnd + 5, end - labelEnd - 5);
        // Proc-Type and DEK-Info lines of a legacy encrypted key.
        if (body.contains(u':')) return {};
        QByteArray compact;
        for (const QChar c : body) {
            if (!c.isSpace()) compact.append(char(c.unicode() < 128 ? c.unicode() : '!'));
        }
        const auto decoded = QByteArray::fromBase64Encoding(compact, QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors);
        if (!decoded || decoded.decoded.isEmpty()) return {};
        return QString::fromLatin1(decoded.decoded.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
    }

    namespace Checks {
        QString DnsTunnelDomain(const QString &text) {
            const auto domain = text.trimmed();
            if (domain.isEmpty()) return QCoreApplication::translate("Validation", "Enter the tunnel domain, for example t.example.com.");
            if (const auto message = Validation::Host(domain); !message.isEmpty()) return message;
            auto bare = domain;
            if (bare.endsWith(u'.')) bare.chop(1);
            if (domain.startsWith(u'[') || Validation::Ip(bare).isEmpty() || !bare.contains(u'.'))
                return QCoreApplication::translate("Validation", "Use the domain name delegated to the tunnel server, for example t.example.com.");
            return {};
        }
    }
}
