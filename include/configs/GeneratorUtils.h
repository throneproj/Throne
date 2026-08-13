#pragma once

#include <QString>
#include <QtGlobal>

namespace Configs::GeneratorUtils {

struct ParsedHostPort {
    QString host;
    quint16 port = 0;
};

// Parse host:port without treating the final hextet of a raw IPv6 literal as a
// port. IPv6 ports therefore require the standard [address]:port form.
[[nodiscard]] ParsedHostPort ParseHostPort(const QString &endpoint,
                                           quint16 defaultPort);

// Return the independent XHTTP download endpoint only when it is a hostname
// that needs bootstrap DNS. Numeric IPv4/IPv6 addresses and malformed objects
// do not need (or cannot safely receive) such a DNS rule.
[[nodiscard]] QString ExtractXrayXhttpDownloadDomain(
    const QString &downloadSettings);

} // namespace Configs::GeneratorUtils
