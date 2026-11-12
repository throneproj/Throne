#include "include/ui/utils/ServiceCheckHtml.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSet>

#include "include/database/entities/Profile.h"
#include "include/ui/setting/ThemeManager.hpp"

namespace {
    // Unknown and failed checks count for none.
    QColor serviceCategoryColor(int available, int partial, int refused) {
        const auto &tokens = themeManager()->tokens;
        if (available + partial + refused == 0) return tokens.muted;
        if (refused == 0 && partial == 0) return tokens.success;
        if (available + partial == 0) return tokens.danger;
        return tokens.warning;
    }

    QColor serviceStatusColor(const QString &status) {
        const auto &tokens = themeManager()->tokens;
        if (status == "available") return tokens.success;
        if (status == "partial") return tokens.warning;
        if (status == "restricted" || status == "blocked") return tokens.danger;
        return tokens.muted;
    }

    QString serviceColored(const QString &text, const QColor &color) {
        return QStringLiteral("<span style=\"color:%1; font-weight:600; white-space:nowrap\">%2</span>").arg(color.name(), text.toHtmlEscaped());
    }

    // Every provider's answer merged: the network kind, then the anonymizing or abuse flags any of them saw.
    QString serviceIpKind(const QJsonArray &entries) {
        static const QStringList riskFlags{"vpn", "proxy", "tor", "abuser", "anonymous"};
        QSet<QString> traits;
        for (const auto &value : entries) {
            const auto entry = value.toObject();
            traits.insert(entry["type"].toString().toLower());
            for (const auto &flag : entry["flags"].toArray()) traits.insert(flag.toString().toLower());
        }
        QString kind;
        if (traits.contains("hosting") || traits.contains("datacenter")) kind = QObject::tr("Datacenter IP");
        else if (traits.contains("mobile")) kind = QObject::tr("Mobile IP");
        else if (traits.contains("business")) kind = QObject::tr("Business IP");
        else if (traits.contains("isp")) kind = QObject::tr("Residential IP");
        QStringList flagged;
        for (const auto &flag : riskFlags) {
            if (traits.contains(flag)) flagged << flag;
        }
        QString html = kind.toHtmlEscaped();
        if (!flagged.isEmpty()) {
            html += (html.isEmpty() ? QString() : QStringLiteral(", "))
                    + serviceColored(QObject::tr("listed as %1").arg(flagged.join(QStringLiteral(", "))), themeManager()->tokens.danger);
        }
        return html;
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

namespace ServiceCheckHtml {
    // Names, notes and lookup answers come from downloaded definitions and third parties: every one is escaped.
    QString summary(const Configs::Profile &profile) {
        if (profile.service_checks.isEmpty()) return {};
        const auto root = QJsonDocument::fromJson(profile.service_checks.toUtf8()).object();
        if (root.isEmpty()) return {};
        if (!root["error"].toString().isEmpty()) return serviceColored(QObject::tr("Failed"), themeManager()->tokens.danger);
        const auto checks = root["checks"].toArray();
        QStringList parts;
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
            parts << serviceColored(category["name"].toString(), serviceCategoryColor(available, partial, refused));
        }
        if (const auto ipKind = serviceIpKind(root["ip"].toArray()); !ipKind.isEmpty()) {
            parts << QStringLiteral("<span style=\"white-space:nowrap\">· %1</span>").arg(ipKind);
        }
        // Breaks only between parts, never inside a name.
        return parts.join(QStringLiteral("&nbsp;&nbsp; "));
    }

    QString details(const Configs::Profile &profile) {
        if (profile.service_checks.isEmpty()) return {};
        const auto root = QJsonDocument::fromJson(profile.service_checks.toUtf8()).object();
        if (root.isEmpty()) return {};
        const auto checkedAt = QDateTime::fromSecsSinceEpoch(root["at"].toInteger());
        QString html = QStringLiteral("<qt><p>%1</p>")
                           .arg(QObject::tr("Service check, %1").arg(QLocale().toString(checkedAt, QLocale::ShortFormat)).toHtmlEscaped());
        if (const auto error = root["error"].toString(); !error.isEmpty()) {
            return html + QStringLiteral("<p style=\"color:%1\">%2</p></qt>").arg(themeManager()->tokens.danger.name(), error.toHtmlEscaped());
        }
        const auto heading = [](const QString &text) {
            return QStringLiteral("<tr><td colspan=\"3\" style=\"padding-top:8px\"><b>%1</b></td></tr>").arg(text.toHtmlEscaped());
        };
        html += QStringLiteral("<table cellspacing=\"0\" cellpadding=\"2\">");
        const auto checks = root["checks"].toArray();
        for (const auto &categoryValue : root["cats"].toArray()) {
            const auto category = categoryValue.toObject();
            const auto id = category["id"].toString();
            html += heading(category["name"].toString());
            for (const auto &checkValue : checks) {
                const auto check = checkValue.toObject();
                if (check["cat"].toString() != id) continue;
                const auto status = check["status"].toString();
                const auto color = serviceStatusColor(status);
                QStringList extra;
                if (const auto region = check["region"].toString(); !region.isEmpty()) extra << region;
                if (const auto note = check["note"].toString(); !note.isEmpty()) extra << note;
                // width="1%" + nowrap: the first two columns shrink to their text and the note gets the rest.
                html += QStringLiteral("<tr><td width=\"1%\" style=\"padding-right:12px\">%1</td>"
                                       "<td width=\"1%\" style=\"color:%2; white-space:nowrap; padding-right:12px\">%3</td><td>%4</td></tr>")
                            .arg(serviceColored(check["name"].toString(), color), color.name(),
                                 serviceStatusText(status).toHtmlEscaped(), extra.join(QStringLiteral(" — ")).toHtmlEscaped());
            }
        }
        if (const auto ip = root["ip"].toArray(); !ip.isEmpty()) {
            html += heading(QObject::tr("IP"));
            if (const auto kind = serviceIpKind(ip); !kind.isEmpty()) html += QStringLiteral("<tr><td colspan=\"3\">%1</td></tr>").arg(kind);
            for (const auto &entry : ip) {
                html += QStringLiteral("<tr><td colspan=\"3\">%1</td></tr>").arg(serviceIpLine(entry.toObject()).toHtmlEscaped());
            }
        }
        return html + QStringLiteral("</table></qt>");
    }
}
