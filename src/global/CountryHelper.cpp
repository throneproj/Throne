#include "include/global/CountryHelper.hpp"

#include <QHash>

QString CountryNameToCode(const QString& countryName) {
    return CountryMap.value(countryName, "");
}

QString CountryCodeToName(const QString& countryCode) {
    static const QHash<QString, QString> names = [] {
        QHash<QString, QString> reverse;
        for (auto it = CountryMap.cbegin(); it != CountryMap.cend(); ++it) reverse.insert(it.value(), it.key());
        return reverse;
    }();
    return names.value(countryCode.trimmed().toUpper());
}

QString CountryCodeToFlag(const QString& countryCode) {
    QVector<uint> ucs4 = countryCode.toUcs4();
    for (uint& code : ucs4) {
        code += 0x1F1A5;
    }
    return QString::fromUcs4(ucs4.data(), countryCode.length());
}