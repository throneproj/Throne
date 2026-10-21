#include "include/engine/EngineEvents.h"

#include <QCoreApplication>
#include <QThread>

namespace Engine {
    EngineEvents::EngineEvents(QObject *parent) : QObject(parent) {}

    EngineEvents *EngineEvents::instance() {
        // Deliberately leaked; pinned to the main thread so AutoConnection receivers there run direct from main and queued from elsewhere.
        static EngineEvents *const self = [] {
            auto *events = new EngineEvents(nullptr);
            if (auto *app = QCoreApplication::instance(); app && events->thread() != app->thread()) events->moveToThread(app->thread());
            return events;
        }();
        return self;
    }

    void EngineEvents::SubscriptionNewGroup(int groupId) {
        emit instance()->subscriptionNewGroup(groupId);
    }

    void EngineEvents::SubscriptionFinished(int importedCount, bool quiet) {
        emit instance()->subscriptionFinished(importedCount, quiet);
    }

    void EngineEvents::SubscriptionGroupChanged(int groupId, const QList<int> &disturbedProfileIds) {
        emit instance()->subscriptionGroupChanged(groupId, disturbedProfileIds);
    }

    void EngineEvents::SubscriptionDiff(const QString &title, const QString &body) {
        emit instance()->subscriptionDiff(title, body);
    }

    void EngineEvents::GroupOrderChanged(const QList<int> &groupIds) {
        emit instance()->groupOrderChanged(groupIds);
    }

    void EngineEvents::ExtraCoreExited() {
        emit instance()->extraCoreExited();
    }

    void EngineEvents::GroupsChanged() {
        emit instance()->groupsChanged();
    }

    void EngineEvents::ProfilesRemoved(const QList<int> &profileIds) {
        emit instance()->profilesRemoved(profileIds);
    }

    void EngineEvents::SettingsChanged(const QStringList &keys) {
        emit instance()->settingsChanged(keys);
    }
} // namespace Engine
