#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

namespace Engine {
    // Engine-originated half of the old MwMessage bus; the instance lives on the main thread, emitting is safe from any thread.
    class EngineEvents : public QObject {
        Q_OBJECT

    public:
        static EngineEvents *instance();

        static void SubscriptionNewGroup(int groupId);
        static void SubscriptionFinished(int importedCount, bool quiet);
        static void SubscriptionGroupChanged(int groupId, const QList<int> &disturbedProfileIds);
        static void SubscriptionDiff(const QString &title, const QString &body);
        static void SubscriptionReplacedRunning(int profileId);
        static void RoutingRestartNeeded();
        static void GroupOrderChanged(const QList<int> &groupIds);
        static void ExtraCoreExited();
        static void GroupsChanged();
        static void ProfilesRemoved(const QList<int> &profileIds);
        static void ProfilesChanged(const QList<int> &profileIds);
        static void RoutesChanged();
        static void SettingsChanged(const QStringList &keys);

    signals:
        void subscriptionNewGroup(int groupId);
        // importedCount < 0 when the job did not count.
        void subscriptionFinished(int importedCount, bool quiet);
        void subscriptionGroupChanged(int groupId, const QList<int> &disturbedProfileIds);
        void subscriptionDiff(const QString &title, const QString &body);
        // A refresh stopped the running profile: profileId is its only same-name successor after a delete,
        // or the profile itself after a name match updated it in place.
        void subscriptionReplacedRunning(int profileId);
        // The routing profile the running config uses changed underneath it.
        void routingRestartNeeded();
        void groupOrderChanged(const QList<int> &groupIds);
        void extraCoreExited();
        // Groups were added, edited, deleted or repaired: committed by a service, from the window or the API.
        void groupsChanged();
        void profilesRemoved(const QList<int> &profileIds);
        // Profiles were added or edited: committed by a service, from the window or the API.
        void profilesChanged(const QList<int> &profileIds);
        // Routing profiles were added, edited or deleted: committed by a service, from the window or the API.
        void routesChanged();
        // Database keys a settings commit (Engine::SettingsService) changed, from a window, a menu or the API alike.
        void settingsChanged(const QStringList &keys);

    private:
        explicit EngineEvents(QObject *parent = nullptr);
    };
} // namespace Engine
