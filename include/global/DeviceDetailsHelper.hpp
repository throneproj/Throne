#pragma once

#include <QString>

struct DeviceDetails {
    QString hwid;
    QString os;
    QString osVersion;
    QString model;
};

DeviceDetails GetDeviceDetails();

// Once, before the first GetDeviceDetails() (Bootstrap::OpenDataDir; EngineHost warms it on a worker): where a Unix machine
// without a usable /etc/machine-id keeps the generated device ID (<configDir>/hwid, owner-only).
void SetDeviceIdStore(const QString &configDir);
