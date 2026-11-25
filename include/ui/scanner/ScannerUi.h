#pragma once

#include <QString>

#include "include/database/entities/IpScan.h"
#include "include/scanner/ScanManager.h"

class QWidget;

namespace ScannerUi {
    // Ref-counted per scan: every Watch(id, true) needs a matching Watch(id, false).
    void Watch(int scanId, bool watched);

    [[nodiscard]] bool IsResumable(const Configs::IpScan &scan);

    [[nodiscard]] int ResultCount(const Configs::IpScan &scan);

    // Asks only when starting over discards an unfinished pass or will replace results.
    bool ConfirmStartOver(QWidget *parent, const Configs::IpScan &scan);

    bool StartScan(QWidget *parent, int scanId, Scanner::StartMode mode);

    [[nodiscard]] Scanner::ScanLiveState RowState(const Configs::IpScan &scan);

    [[nodiscard]] QString StatusLine(const Configs::IpScan &scan, const Scanner::ScanLiveState &live, bool withStage,
                                     bool *isError);

    // Opens the newest DNS resolver scan of the profile, creating one when it has none.
    void FindResolvers(int profileId);
} // namespace ScannerUi
