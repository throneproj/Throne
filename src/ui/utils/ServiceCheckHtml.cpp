#include "include/ui/utils/ServiceCheckHtml.h"

#include <QCache>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSet>

#include "include/database/entities/Profile.h"
#include "include/global/Configs.hpp"
#include "include/ui/setting/ThemeManager.hpp"

namespace {
    constexpr int kServiceHtmlCachedResults = 200;

    QString serviceColored(const QString &text, const QColor &color) {
        return QStringLiteral("<span style=\"color:%1; font-weight:600; white-space:nowrap\">%2</span>").arg(color.name(), text.toHtmlEscaped());
    }

    // Every provider's answer merged: the network kind, then the anonymizing or abuse flags any of them saw.
    QString serviceIpKind(const QJsonArray &entries, bool rich) {
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
        QString text = rich ? kind.toHtmlEscaped() : kind;
        if (!flagged.isEmpty()) {
            const auto listed = QObject::tr("listed as %1").arg(flagged.join(QStringLiteral(", ")));
            text += (text.isEmpty() ? QString() : QStringLiteral(", ")) + (rich ? serviceColored(listed, themeManager()->tokens.danger) : listed);
        }
        return text;
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

    QJsonObject serviceHtmlRoot(const Configs::Profile &profile) {
        if (profile.service_checks.isEmpty()) return {};
        return QJsonDocument::fromJson(profile.service_checks.toUtf8()).object();
    }

    QList<ServiceIcons::Item> serviceHtmlParseItems(const QString &serviceChecks) {
        const auto root = QJsonDocument::fromJson(serviceChecks.toUtf8()).object();
        if (root.isEmpty() || !root["error"].toString().isEmpty()) return {};
        QList<ServiceIcons::Item> items;
        for (const auto &value : root["checks"].toArray()) {
            const auto check = value.toObject();
            const auto status = check["status"].toString();
            ServiceIcons::Item item;
            item.id = check["id"].toString();
            item.name = check["name"].toString();
            item.state = ServiceIcons::StateOf(status);
            item.toolTip = QObject::tr("%1: %2").arg(item.name, serviceStatusText(status));
            QStringList extra;
            if (const auto region = check["region"].toString(); !region.isEmpty()) extra << region;
            if (const auto note = check["note"].toString(); !note.isEmpty()) extra << note;
            if (!extra.isEmpty()) item.toolTip += QStringLiteral(" — ") + extra.join(QStringLiteral(" — "));
            items << item;
        }
        return items;
    }
} // namespace

namespace ServiceCheckHtml {
    QList<ServiceIcons::Item> items(const Configs::Profile &profile) {
        if (profile.service_checks.isEmpty()) return {};
        static QCache<QString, QList<ServiceIcons::Item>> parsed(kServiceHtmlCachedResults);
        QList<ServiceIcons::Item> shown;
        if (const auto *cached = parsed.object(profile.service_checks)) {
            shown = *cached;
        } else {
            shown = serviceHtmlParseItems(profile.service_checks);
            parsed.insert(profile.service_checks, new QList<ServiceIcons::Item>(shown));
        }
        const auto &disabled = Configs::dataManager->settingsRepo->service_check_disabled;
        if (!disabled.isEmpty()) {
            shown.removeIf([&disabled](const ServiceIcons::Item &item) { return disabled.contains(item.id); });
        }
        return shown;
    }

    QString ipKind(const Configs::Profile &profile, bool rich) {
        return serviceIpKind(serviceHtmlRoot(profile).value(QStringLiteral("ip")).toArray(), rich);
    }

    QString failure(const Configs::Profile &profile) {
        return serviceHtmlRoot(profile).value(QStringLiteral("error")).toString();
    }

    QString summaryLine(const Configs::Profile &profile) {
        const auto root = serviceHtmlRoot(profile);
        if (root.isEmpty()) return {};
        const auto checkedAt = QDateTime::fromSecsSinceEpoch(root["at"].toInteger());
        QString line = QObject::tr("Service check, %1").arg(QLocale().toString(checkedAt, QLocale::ShortFormat));
        if (const auto error = root["error"].toString(); !error.isEmpty()) {
            line += QStringLiteral(" · ") + QObject::tr("%1: %2").arg(QObject::tr("Failed"), error);
        } else if (const auto kind = serviceIpKind(root["ip"].toArray(), false); !kind.isEmpty()) {
            line += QStringLiteral(" · ") + kind;
        }
        return line;
    }
}
