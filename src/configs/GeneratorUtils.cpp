#include "include/configs/GeneratorUtils.h"

#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>

namespace Configs::GeneratorUtils {
namespace {

bool ParsePort(const QString &text, quint16 *port) {
    bool ok = false;
    const auto parsed = text.toUInt(&ok);
    if (!ok || parsed == 0 || parsed > 65535) return false;
    *port = static_cast<quint16>(parsed);
    return true;
}

} // namespace

ParsedHostPort ParseHostPort(const QString &endpoint, const quint16 defaultPort) {
    const auto trimmed = endpoint.trimmed();
    ParsedHostPort result{trimmed, defaultPort};

    if (trimmed.startsWith('[')) {
        const auto closingBracket = trimmed.indexOf(']');
        if (closingBracket <= 1) return result;

        const auto host = trimmed.mid(1, closingBracket - 1);
        const auto suffix = trimmed.mid(closingBracket + 1);
        if (suffix.isEmpty()) return {host, defaultPort};
        if (!suffix.startsWith(':')) return result;

        quint16 port = 0;
        if (!ParsePort(suffix.mid(1), &port)) return result;
        return {host, port};
    }

    // A valid unbracketed IPv6 literal has no unambiguous port component.
    if (QHostAddress(trimmed).protocol() == QAbstractSocket::IPv6Protocol)
        return result;

    if (trimmed.count(':') != 1) return result;
    const auto separator = trimmed.indexOf(':');
    const auto host = trimmed.left(separator);
    quint16 port = 0;
    if (host.isEmpty() || !ParsePort(trimmed.mid(separator + 1), &port))
        return result;
    return {host, port};
}

QString ExtractXrayXhttpDownloadDomain(const QString &downloadSettings) {
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(downloadSettings.toUtf8(),
                                                   &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return {};

    const auto addressValue = document.object().value(QStringLiteral("address"));
    if (!addressValue.isString()) return {};
    auto address = addressValue.toString().trimmed();
    if (address.isEmpty()) return {};

    // Xray's address normally contains a bare host, but accept the standard
    // bracketed-IPv6 and host:port spellings defensively. Returning a string
    // with a port would never match a DNS domain selector.
    if (address.startsWith('[')) {
        const auto closingBracket = address.indexOf(']');
        if (closingBracket <= 1) return {};
        address = address.mid(1, closingBracket - 1);
    } else if (QHostAddress(address).protocol() ==
                   QAbstractSocket::UnknownNetworkLayerProtocol &&
               address.count(':') == 1) {
        const auto separator = address.indexOf(':');
        quint16 ignoredPort = 0;
        if (ParsePort(address.mid(separator + 1), &ignoredPort))
            address = address.left(separator);
    }

    if (QHostAddress(address).protocol() !=
        QAbstractSocket::UnknownNetworkLayerProtocol) {
        return {};
    }
    return address;
}

} // namespace Configs::GeneratorUtils
