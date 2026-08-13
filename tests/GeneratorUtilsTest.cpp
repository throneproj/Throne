#include "include/configs/GeneratorUtils.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using Configs::GeneratorUtils::ExtractXrayXhttpDownloadDomain;
using Configs::GeneratorUtils::ParseHostPort;

namespace {

class TestFailure final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

void require(const bool condition, const char *expression, const int line) {
    if (!condition)
        throw TestFailure("line " + std::to_string(line) + ": " + expression);
}

#define REQUIRE(expression) require(static_cast<bool>(expression), #expression, __LINE__)

void parsesWarpHostAndPort() {
    const auto endpoint = ParseHostPort(QStringLiteral("engage.cloudflareclient.com:2408"),
                                        1234);
    REQUIRE(endpoint.host == QStringLiteral("engage.cloudflareclient.com"));
    REQUIRE(endpoint.port == 2408);
}

void parsesBracketedWarpIpv6() {
    const auto endpoint = ParseHostPort(QStringLiteral("[2606:4700:d0::a29f:c001]:2408"),
                                        1234);
    REQUIRE(endpoint.host == QStringLiteral("2606:4700:d0::a29f:c001"));
    REQUIRE(endpoint.port == 2408);
}

void preservesRawWarpIpv6WithoutGuessingPort() {
    const auto endpoint = ParseHostPort(QStringLiteral("2001:db8::1:2408"),
                                        1234);
    REQUIRE(endpoint.host == QStringLiteral("2001:db8::1:2408"));
    REQUIRE(endpoint.port == 1234);
}

void usesDefaultPortForBareHost() {
    const auto endpoint = ParseHostPort(QStringLiteral("engage.cloudflareclient.com"),
                                        2408);
    REQUIRE(endpoint.host == QStringLiteral("engage.cloudflareclient.com"));
    REQUIRE(endpoint.port == 2408);
}

void extractsXhttpDownloadHostname() {
    const auto domain = ExtractXrayXhttpDownloadDomain(QStringLiteral(
        R"({"address":"download.example.com","port":443,"network":"xhttp"})"));
    REQUIRE(domain == QStringLiteral("download.example.com"));
}

void normalizesXhttpDownloadHostAndPort() {
    const auto domain = ExtractXrayXhttpDownloadDomain(QStringLiteral(
        R"({"address":"download.example.com:8443","port":443,"network":"xhttp"})"));
    REQUIRE(domain == QStringLiteral("download.example.com"));
}

void excludesNumericXhttpDownloadAddresses() {
    REQUIRE(ExtractXrayXhttpDownloadDomain(
                QStringLiteral(R"({"address":"203.0.113.7"})"))
                .isEmpty());
    REQUIRE(ExtractXrayXhttpDownloadDomain(
                QStringLiteral(R"({"address":"2001:db8::7"})"))
                .isEmpty());
    REQUIRE(ExtractXrayXhttpDownloadDomain(
                QStringLiteral(R"({"address":"[2001:db8::7]"})"))
                .isEmpty());
}

void rejectsMalformedXhttpDownloadSettings() {
    REQUIRE(ExtractXrayXhttpDownloadDomain(QStringLiteral("not json")).isEmpty());
    REQUIRE(ExtractXrayXhttpDownloadDomain(QStringLiteral(R"({"port":443})")).isEmpty());
}

} // namespace

int main() {
    const std::vector<std::pair<const char *, void (*)()>> tests = {
        {"WARP host and port", parsesWarpHostAndPort},
        {"bracketed WARP IPv6", parsesBracketedWarpIpv6},
        {"raw WARP IPv6", preservesRawWarpIpv6WithoutGuessingPort},
        {"WARP default port", usesDefaultPortForBareHost},
        {"XHTTP download hostname", extractsXhttpDownloadHostname},
        {"XHTTP download host and port", normalizesXhttpDownloadHostAndPort},
        {"numeric XHTTP download addresses", excludesNumericXhttpDownloadAddresses},
        {"malformed XHTTP download settings", rejectsMalformedXhttpDownloadSettings},
    };

    int failures = 0;
    for (const auto &[name, test] : tests) {
        try {
            test();
            std::cout << "PASS: " << name << '\n';
        } catch (const std::exception &error) {
            ++failures;
            std::cerr << "FAIL: " << name << ": " << error.what() << '\n';
        }
    }
    std::cout << tests.size() - static_cast<std::size_t>(failures) << '/'
              << tests.size() << " tests passed\n";
    return failures == 0 ? 0 : 1;
}
