#pragma once
#include <QString>

namespace Configs
{
    class outbound;

    // Never null; an unknown type yields a base outbound flagged invalid. Caller takes ownership.
    outbound* NewOutboundByType(const QString& type);

    // Non-empty (the reason) for types that must never be created from foreign input: links, subscriptions, shared route profiles.
    QString RefusedForeignType(const QString& type);
}
