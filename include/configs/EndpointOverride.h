#pragma once
#include <QString>

#include <memory>

#include "include/database/entities/EndpointSource.h"
#include "include/database/entities/Profile.h"

namespace Configs
{
    // Keeps the original domain as SNI/Host; nullptr when the type has no single dialable server.
    std::shared_ptr<Profile> CloneProfileWithServer(const std::shared_ptr<Profile> &base, const QString &address, int port);

    struct EndpointResolution {
        // Empty: the profile dials its own address.
        QString address;
        // 0: the profile keeps its own port.
        int port = 0;
        // Where the address comes from, for hints and logs, e.g. IP list “Scan CF Result”.
        QString origin;
        // Why a configured source supplied no address: a missing or empty IP list.
        QString problem;

        [[nodiscard]] QString DisplayTarget() const;
    };

    // What `source` makes a profile dial; Inherit and Own supply nothing.
    EndpointResolution ResolveEndpointSource(const EndpointSource &source);

    // The profile's own source, or its group's while it inherits.
    EndpointSource EffectiveEndpointSource(const Profile &profile);

    // Non-empty when the profile has no single server address an endpoint source could replace.
    QString EndpointOverrideBlocker(const std::shared_ptr<Profile> &profile);

    // For display: the host the profile dials instead of its own, or empty. IP lists are cached until the next invalidation.
    QString EffectiveEndpointHost(const std::shared_ptr<Profile> &profile);

    // The profile's address as its type shows it, with the effective host in place of its own.
    QString DisplayEffectiveAddress(const std::shared_ptr<Profile> &profile);

    // Called whenever IP lists change, so displays pick up a new first entry.
    void InvalidateEndpointDisplayCache();
}
