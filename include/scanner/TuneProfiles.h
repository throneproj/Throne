#pragma once

#include <QString>
#include <QStringList>
#include <memory>

#include "include/database/entities/IpScan.h"
#include "include/database/entities/Profile.h"
#include "include/scanner/TunePlanner.h"

namespace Scanner::Tune {
    // Why the search has nothing to change in this profile; empty when it has.
    [[nodiscard]] QString Unsupported(const Configs::Profile &profile);

    [[nodiscard]] Space SpaceFor(const Configs::Profile &profile, const Configs::ScanTuneOptions &options);

    [[nodiscard]] Limits LimitsFor(const Configs::ScanTuneOptions &options);

    // Writes only the profile's own fields, never the global defaults.
    bool Apply(Configs::Profile &profile, const Assignment &assignment, QString *error = nullptr);

    // Unsaved (id -1), with the same group, name and endpoint source.
    [[nodiscard]] std::shared_ptr<Configs::Profile> Copy(const Configs::Profile &profile);

    [[nodiscard]] QString Describe(const Assignment &assignment);

    // "off" stands for Go's own TLS; *error names the first value that cannot be tried.
    [[nodiscard]] QStringList ParseFingerprints(const QString &text, QString *error = nullptr);

    [[nodiscard]] QStringList ParseDelays(const QString &text, QString *error = nullptr);

    [[nodiscard]] QString CheckOptions(const Configs::ScanTuneOptions &options);

    // The endpoint override, else the profile's own address. UI thread: it needs the live profile.
    [[nodiscard]] QString DialHost(const std::shared_ptr<Configs::Profile> &profile);

    // Covers the profile, the host it dials, the options and the global defaults a variant inherits.
    [[nodiscard]] QString SpecHash(const Configs::Profile &profile, const QString &dialHost, const Configs::ScanTuneOptions &options);

    [[nodiscard]] QString MtuTestUrl(const Configs::ScanTuneOptions &options);
} // namespace Scanner::Tune
