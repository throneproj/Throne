#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>

namespace Configs {
    struct CustomTest {
        QString id;
        QString name;
        QString script;
        int timeout_ms = 5000;

        bool operator==(const CustomTest &) const = default;

        [[nodiscard]] QJsonObject ToJson() const;
        static CustomTest FromJson(const QJsonObject &obj);

        static QString NewId();
        static QString DefaultScript();

        static QByteArray SerializeList(const QList<CustomTest> &tests);
        static QList<CustomTest> ParseList(const QString &raw);
    };

    struct CustomTestResult {
        enum class SortKind : int {
            None = 0,
            Number = 1,
            String = 2,
        };

        QString text;
        QString color;
        QString tooltip;
        QString error;
        SortKind sort_kind = SortKind::None;
        double sort_number = 0.0;
        QString sort_string;

        [[nodiscard]] bool isEmpty() const {
            return text.isEmpty() && error.isEmpty();
        }

        [[nodiscard]] QString displayText() const {
            return !text.isEmpty() ? text : error;
        }

        bool operator==(const CustomTestResult &) const = default;

        [[nodiscard]] QJsonObject ToJson() const;
        static CustomTestResult FromJson(const QJsonObject &obj);

        static QByteArray SerializeMap(const QMap<QString, CustomTestResult> &map);
        static QMap<QString, CustomTestResult> ParseMap(const QString &raw);
    };
} // namespace Configs
