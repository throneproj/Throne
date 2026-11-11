#include <include/database/entities/Profile.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSet>
#include <QTextDocument>

#include "include/database/GroupsRepo.h"
#include "include/global/Configs.hpp"

namespace Configs
{
    namespace {
        // A available, P partial, R restricted or blocked; unknown and failed checks count for none.
        QString serviceCategoryDot(int available, int partial, int refused) {
            if (available + partial + refused == 0) return QStringLiteral("⚪");
            if (refused == 0 && partial == 0) return QStringLiteral("🟢");
            if (available + partial == 0) return QStringLiteral("🔴");
            return QStringLiteral("🟡");
        }

        // Every provider's answer merged: the network kind, then a flag when any of them saw anonymizing or abuse.
        QString serviceIpMark(const QJsonArray &entries) {
            static const QSet<QString> riskFlags{"vpn", "proxy", "tor", "abuser", "anonymous"};
            QSet<QString> traits;
            bool risky = false;
            for (const auto &value : entries) {
                const auto entry = value.toObject();
                traits.insert(entry["type"].toString().toLower());
                for (const auto &flag : entry["flags"].toArray()) {
                    const auto name = flag.toString().toLower();
                    traits.insert(name);
                    risky = risky || riskFlags.contains(name);
                }
            }
            QString mark;
            if (traits.contains("hosting") || traits.contains("datacenter")) mark = QStringLiteral("🖥️");
            else if (traits.contains("mobile")) mark = QStringLiteral("📱");
            else if (traits.contains("business")) mark = QStringLiteral("🏢");
            else if (traits.contains("isp")) mark = QStringLiteral("🏠");
            if (risky) mark += QStringLiteral("🚩");
            return mark;
        }

        QString serviceStatusIcon(const QString &status) {
            if (status == "available") return QStringLiteral("✅");
            if (status == "partial") return QStringLiteral("🟡");
            if (status == "restricted") return QStringLiteral("⛔");
            if (status == "blocked") return QStringLiteral("🚫");
            if (status == "failed") return QStringLiteral("⚠️");
            return QStringLiteral("❔");
        }

        QString serviceStatusText(const QString &status) {
            if (status == "available") return QObject::tr("Available");
            if (status == "partial") return QObject::tr("Partial");
            if (status == "restricted") return QObject::tr("Restricted");
            if (status == "blocked") return QObject::tr("Blocked");
            if (status == "unknown") return QObject::tr("Unknown");
            if (status == "failed") return QObject::tr("Failed");
            return status;
        }

        QString serviceIpLine(const QJsonObject &entry) {
            const auto provider = entry["provider"].toString();
            if (const auto error = entry["error"].toString(); !error.isEmpty()) return provider + ": " + error;
            const auto joined = [](const QStringList &parts, const QString &separator) {
                QStringList kept;
                for (const auto &part : parts) {
                    if (!part.isEmpty()) kept << part;
                }
                return kept.join(separator);
            };
            QStringList flags;
            for (const auto &flag : entry["flags"].toArray()) flags << flag.toString();
            const int risk = entry["risk"].toInt(-1);
            const QString details = joined({
                joined({entry["ip"].toString(), entry["cc"].toString()}, " "),
                joined({entry["asn"].toString(), entry["org"].toString()}, " "),
                entry["type"].toString(),
                risk >= 0 ? QObject::tr("risk %1").arg(risk) : QString(),
                joined(flags, ", "),
            }, " · ");
            return details.isEmpty() ? provider : provider + ": " + details;
        }
    } // namespace

    Profile::Profile(Configs::outbound *outbound, const QString &type_)
    {
        if (!type_.isEmpty()) this->type = type_;


        if (outbound != nullptr) {
            this->outbound = std::shared_ptr<Configs::outbound>(outbound);
        }
    }

    void Profile::ClearTestResults() {
        test_country.clear();
        ip_out.clear();
        service_checks.clear();
        latency = 0;
        latency_at = 0;
        dl_speed.clear();
        ul_speed.clear();
    }

    void Profile::SetLatency(int ms) {
        latency = ms;
        // 0 means "never measured", so a reset must clear the stamp rather than record now.
        latency_at = ms == 0 ? 0 : QDateTime::currentSecsSinceEpoch();
    }

    QString Profile::DisplayTestResult() const {
        auto group = dataManager->groupsRepo->GetGroup(gid);
        if (group == nullptr) return "";
        QString result;
        if (!test_country.isEmpty()) result += UNICODE_LRO + CountryCodeToFlag(test_country) + " ";
        if (latency == kLatencyConnectOnly) {
            return QObject::tr("Connect OK");
        } else if (latency < 0) {
            return QObject::tr("Unavailable");
        } else if (latency > 0) {
            result += QString("%1 ms").arg(latency);
        }
        bool showSpeed = group->test_items_to_show == testShowItems::all || group->test_items_to_show == testShowItems::speedOnly;
        bool showIP = group->test_items_to_show == testShowItems::all || group->test_items_to_show == testShowItems::ipOnly;
        if (!dl_speed.isEmpty() && dl_speed != "N/A" && showSpeed) result += " ↓" + dl_speed;
        if (!ul_speed.isEmpty() && ul_speed != "N/A" && showSpeed) result += " ↑" + ul_speed;
        if (!ip_out.isEmpty() && showIP) result += " 🌐" + ip_out;
        return result;
    }

    QString Profile::DisplayServiceMarks() const {
        if (service_checks.isEmpty()) return {};
        const auto root = QJsonDocument::fromJson(service_checks.toUtf8()).object();
        if (root.isEmpty()) return {};
        if (!root["error"].toString().isEmpty()) return QStringLiteral("⚠️");
        const auto checks = root["checks"].toArray();
        QStringList marks;
        for (const auto &categoryValue : root["cats"].toArray()) {
            const auto category = categoryValue.toObject();
            const auto id = category["id"].toString();
            int available = 0, partial = 0, refused = 0;
            for (const auto &checkValue : checks) {
                const auto check = checkValue.toObject();
                if (check["cat"].toString() != id) continue;
                const auto status = check["status"].toString();
                if (status == "available") ++available;
                else if (status == "partial") ++partial;
                else if (status == "restricted" || status == "blocked") ++refused;
            }
            const auto icon = category["icon"].toString();
            marks << (icon.isEmpty() ? category["name"].toString() : icon) + serviceCategoryDot(available, partial, refused);
        }
        if (const auto ipMark = serviceIpMark(root["ip"].toArray()); !ipMark.isEmpty()) marks << ipMark;
        return marks.join(QLatin1Char(' '));
    }

    QString Profile::DisplayServiceToolTip() const {
        if (service_checks.isEmpty()) return {};
        const auto root = QJsonDocument::fromJson(service_checks.toUtf8()).object();
        if (root.isEmpty()) return {};
        const auto checkedAt = QDateTime::fromSecsSinceEpoch(root["at"].toInteger());
        QStringList lines{QObject::tr("Service check, %1").arg(QLocale().toString(checkedAt, QLocale::ShortFormat))};
        if (const auto error = root["error"].toString(); !error.isEmpty()) {
            lines << error;
        } else {
            const auto checks = root["checks"].toArray();
            for (const auto &categoryValue : root["cats"].toArray()) {
                const auto category = categoryValue.toObject();
                const auto id = category["id"].toString();
                lines << QString() << QStringList{category["icon"].toString(), category["name"].toString()}.join(QLatin1Char(' ')).trimmed();
                for (const auto &checkValue : checks) {
                    const auto check = checkValue.toObject();
                    if (check["cat"].toString() != id) continue;
                    const auto status = check["status"].toString();
                    auto line = serviceStatusIcon(status) + " " + check["name"].toString() + " — " + serviceStatusText(status);
                    if (const auto region = check["region"].toString(); !region.isEmpty()) line += " (" + region + ")";
                    if (const auto note = check["note"].toString(); !note.isEmpty()) line += ": " + note;
                    lines << line;
                }
            }
            if (const auto ip = root["ip"].toArray(); !ip.isEmpty()) {
                lines << QString() << QObject::tr("IP");
                for (const auto &entry : ip) lines << serviceIpLine(entry.toObject());
            }
        }
        // Names and notes come from downloaded definitions: never let them render as markup.
        return Qt::convertFromPlainText(lines.join(QLatin1Char('\n')));
    }

    QColor Profile::DisplayLatencyColor() const {
        if (latency == kLatencyConnectOnly) {
            return Qt::darkCyan;
        } else if (latency < 0) {
            return Qt::darkGray;
        } else if (latency > 0) {
            if (latency <= 100) {
                return Qt::darkGreen;
            } else if (latency <= 300)
            {
                return Qt::darkYellow;
            } else {
                return Qt::red;
            }
        } else {
            return {};
        }
    }

    QString Profile::DisplayTraffic() const {
        if (traffic_downlink + traffic_uplink == 0) return "";
        return UNICODE_LRO + QString("%1↑ %2↓").arg(ReadableSize(traffic_uplink), ReadableSize(traffic_downlink));
    }

    void Profile::ResetTraffic() {
        traffic_downlink = 0;
        traffic_uplink = 0;
    }

        QString ProfileFilter_ent_key(const std::shared_ptr<Configs::Profile> &ent, bool ignoreMetadata) {
        auto key = ent->outbound->ExportJsonLink(ignoreMetadata);
        return key;
    }

    QString ProfileFilter_ent_identity_key(const std::shared_ptr<Configs::Profile> &ent) {
        auto obj = ent->outbound->ExportIdentity();
        return ent->type + "|" + QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    }

    void ProfileFilter::Uniq(const QList<std::shared_ptr<Profile>> &in,
                             QList<std::shared_ptr<Profile>> &out,
                             bool keep_last, bool ignoreMetadata) {
        QMap<QString, std::shared_ptr<Profile>> hashMap;

        for (const auto &ent: in) {
            QString key = ProfileFilter_ent_key(ent, ignoreMetadata);
            if (hashMap.contains(key)) {
                if (keep_last) {
                    out.removeAll(hashMap[key]);
                    hashMap[key] = ent;
                    out += ent;
                }
            } else {
                hashMap[key] = ent;
                out += ent;
            }
        }
    }

    void ProfileFilter::Common(const QList<std::shared_ptr<Profile>> &src,
                               const QList<std::shared_ptr<Profile>> &dst,
                               QList<std::shared_ptr<Profile>> &outSrc,
                               QList<std::shared_ptr<Profile>> &outDst,
                               bool ignoreMetadata) {
        QMap<QString, QList<std::shared_ptr<Profile>>> srcByKey;

        for (const auto &ent: src) {
            srcByKey[ProfileFilter_ent_key(ent, ignoreMetadata)].append(ent);
        }
        // A src entry can be claimed once: handing the same one to every dst duplicate collapses N identical servers into one profile (#1775).
        for (const auto &ent: dst) {
            auto it = srcByKey.find(ProfileFilter_ent_key(ent, ignoreMetadata));
            if (it == srcByKey.end() || it->isEmpty()) continue;
            outDst += ent;
            outSrc += it->takeFirst();
        }
    }

    void ProfileFilter::OnlyInSrc(const QList<std::shared_ptr<Profile>> &src,
                                  const QList<std::shared_ptr<Profile>> &dst,
                                  QList<std::shared_ptr<Profile>> &out,
                                  bool ignoreMetadata) {
        QMap<QString, bool> hashMap;

        for (const auto &ent: dst) {
            QString key = ProfileFilter_ent_key(ent, ignoreMetadata);
            hashMap[key] = true;
        }
        for (const auto &ent: src) {
            QString key = ProfileFilter_ent_key(ent, ignoreMetadata);
            if (!hashMap.contains(key)) out += ent;
        }
    }

    void ProfileFilter::OnlyInSrc_ByPointer(const QList<std::shared_ptr<Profile>> &src,
                                            const QList<std::shared_ptr<Profile>> &dst,
                                            QList<std::shared_ptr<Profile>> &out) {
        for (const auto &ent: src) {
            if (!dst.contains(ent)) out += ent;
        }
    }

    void ProfileFilter::ChangedByIdentity(QList<std::shared_ptr<Profile>> &src,
                                          QList<std::shared_ptr<Profile>> &dst,
                                          QList<std::shared_ptr<Profile>> &changedSrc,
                                          QList<std::shared_ptr<Profile>> &changedDst) {
        QMap<QString, QList<std::shared_ptr<Profile>>> srcByKey;
        for (const auto &ent: src) {
            srcByKey[ProfileFilter_ent_identity_key(ent)].append(ent);
        }

        QSet<Profile *> matchedSrc;
        QList<std::shared_ptr<Profile>> remainingDst;
        for (const auto &ent: dst) {
            auto &bucket = srcByKey[ProfileFilter_ent_identity_key(ent)];
            if (bucket.isEmpty()) {
                remainingDst += ent;
                continue;
            }
            auto srcEnt = bucket.takeFirst();
            changedSrc += srcEnt;
            changedDst += ent;
            matchedSrc.insert(srcEnt.get());
        }

        QList<std::shared_ptr<Profile>> remainingSrc;
        for (const auto &ent: src) {
            if (!matchedSrc.contains(ent.get())) remainingSrc += ent;
        }
        src = remainingSrc;
        dst = remainingDst;
    }
}
