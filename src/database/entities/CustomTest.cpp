#include "include/database/entities/CustomTest.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>

#include <algorithm>

namespace Configs {
    namespace {
        constexpr int kCustomTestMinTimeoutMs = 500;
        constexpr int kCustomTestMaxTimeoutMs = 60000;
    } // namespace

    QJsonObject CustomTest::ToJson() const {
        QJsonObject obj;
        obj[QStringLiteral("id")] = id;
        obj[QStringLiteral("name")] = name;
        obj[QStringLiteral("script")] = script;
        obj[QStringLiteral("timeout_ms")] = timeout_ms;
        return obj;
    }

    CustomTest CustomTest::FromJson(const QJsonObject &obj) {
        CustomTest test;
        test.id = obj[QStringLiteral("id")].toString().trimmed();
        test.name = obj[QStringLiteral("name")].toString();
        test.script = obj[QStringLiteral("script")].toString();
        const int timeout = obj[QStringLiteral("timeout_ms")].toInt(5000);
        test.timeout_ms = std::clamp(timeout, kCustomTestMinTimeoutMs, kCustomTestMaxTimeoutMs);
        return test;
    }

    QString CustomTest::NewId() {
        return QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    QString CustomTest::DefaultScript() {
        return QStringLiteral(
            "// Implements test(ctx) -> string | number | { text, color?, sortKey?, tooltip? }\n"
            "// Helpers (all routed through the proxy): fetch(url, opts), http.get/head/post/request,\n"
            "// tcpPing(host, port, timeoutMs), sleep(ms), atob, btoa\n"
            "async function test(ctx) {\n"
            "  const res = await fetch(\"https://cp.cloudflare.com/\");\n"
            "  if (!res.ok) {\n"
            "    return { text: \"HTTP \" + res.status, color: \"red\", sortKey: 999999 };\n"
            "  }\n"
            "  return {\n"
            "    text: res.elapsedMs + \" ms\",\n"
            "    color: res.elapsedMs < 300 ? \"green\" : \"yellow\",\n"
            "    sortKey: res.elapsedMs\n"
            "  };\n"
            "}\n");
    }

    QByteArray CustomTest::SerializeList(const QList<CustomTest> &tests) {
        QJsonArray arr;
        for (const auto &test : tests) {
            if (test.id.isEmpty()) continue;
            arr.append(test.ToJson());
        }
        return QJsonDocument(arr).toJson(QJsonDocument::Compact);
    }

    QList<CustomTest> CustomTest::ParseList(const QString &raw) {
        QList<CustomTest> list;
        if (raw.trimmed().isEmpty()) return list;
        const auto doc = QJsonDocument::fromJson(raw.toUtf8());
        if (!doc.isArray()) return list;
        const auto arr = doc.array();
        list.reserve(arr.size());
        for (const auto &item : arr) {
            if (!item.isObject()) continue;
            auto test = FromJson(item.toObject());
            if (test.id.isEmpty()) test.id = NewId();
            list.append(std::move(test));
        }
        return list;
    }

    QJsonObject CustomTestResult::ToJson() const {
        QJsonObject obj;
        if (!text.isEmpty()) obj[QStringLiteral("text")] = text;
        if (!color.isEmpty()) obj[QStringLiteral("color")] = color;
        if (!tooltip.isEmpty()) obj[QStringLiteral("tooltip")] = tooltip;
        if (!error.isEmpty()) obj[QStringLiteral("error")] = error;
        if (sort_kind != SortKind::None) {
            obj[QStringLiteral("sort_kind")] = static_cast<int>(sort_kind);
            if (sort_kind == SortKind::Number) obj[QStringLiteral("sort_number")] = sort_number;
            else if (sort_kind == SortKind::String) obj[QStringLiteral("sort_string")] = sort_string;
        }
        return obj;
    }

    CustomTestResult CustomTestResult::FromJson(const QJsonObject &obj) {
        CustomTestResult res;
        res.text = obj[QStringLiteral("text")].toString();
        res.color = obj[QStringLiteral("color")].toString();
        res.tooltip = obj[QStringLiteral("tooltip")].toString();
        res.error = obj[QStringLiteral("error")].toString();
        const int kind = obj[QStringLiteral("sort_kind")].toInt(0);
        if (kind >= static_cast<int>(SortKind::None) && kind <= static_cast<int>(SortKind::String)) {
            res.sort_kind = static_cast<SortKind>(kind);
        }
        res.sort_number = obj[QStringLiteral("sort_number")].toDouble(0.0);
        res.sort_string = obj[QStringLiteral("sort_string")].toString();
        return res;
    }

    QByteArray CustomTestResult::SerializeMap(const QMap<QString, CustomTestResult> &map) {
        QJsonObject obj;
        for (auto it = map.cbegin(); it != map.cend(); ++it) {
            if (it.key().isEmpty() || it.value().isEmpty()) continue;
            obj.insert(it.key(), it.value().ToJson());
        }
        return QJsonDocument(obj).toJson(QJsonDocument::Compact);
    }

    QMap<QString, CustomTestResult> CustomTestResult::ParseMap(const QString &raw) {
        QMap<QString, CustomTestResult> map;
        if (raw.trimmed().isEmpty()) return map;
        const auto doc = QJsonDocument::fromJson(raw.toUtf8());
        if (!doc.isObject()) return map;
        const auto obj = doc.object();
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
            if (!it.value().isObject() || it.key().isEmpty()) continue;
            auto res = FromJson(it.value().toObject());
            if (!res.isEmpty()) map.insert(it.key(), std::move(res));
        }
        return map;
    }
} // namespace Configs
