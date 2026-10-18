#pragma once

#include <QList>
#include <QString>

#include "include/engine/EngineTypes.h"

namespace libcore { struct SpeedTestResult; }   // struct in the generated libcore.pb.h

namespace Engine {
    // View side of a test session. Synchronous, in session order, on the thread noted per method. Never queue the panel
    // calls: a seed could then land after the clear of the previous session.
    class TestObserver {
    public:
        virtual ~TestObserver() = default;

        // Test threads; must be thread-safe.
        virtual void testPanelSeeded(TestKind kind, int totalProfiles) = 0;
        virtual void testProgressed() = 0;
        virtual void testPanelsCleared() = 0;
        // force false may be throttled away; off the main thread it may block until the main thread has rendered.
        virtual void testViewChanged(bool force) = 0;

        // Main thread.
        virtual void speedTestProgress(const QString &profileName, const libcore::SpeedTestResult &result) = 0;
        virtual void testedProfilesChanged(const QList<int> &profileIds) = 0;
    };
} // namespace Engine
