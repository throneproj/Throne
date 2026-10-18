#include "include/ui/mainwindow.h"

#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"
#include "include/engine/connection/ConnectionManager.h"

int MainWindow::get_profile_to_start() {
    const auto ents = get_now_selected_list();
    if (ents.size() == 1) {
        return ents.first();
    }
    if (ents.isEmpty()) {
        if (connection()->lastProfileId() >= 0 && Configs::dataManager->profilesRepo->GetProfile(connection()->lastProfileId()) != nullptr) {
            return connection()->lastProfileId();
        }
        const int rememberId = Configs::dataManager->settingsRepo->remember_id;
        if (rememberId >= 0 && Configs::dataManager->profilesRepo->GetProfile(rememberId) != nullptr) {
            return rememberId;
        }
        const auto currentGroup = Configs::dataManager->groupsRepo->CurrentGroup();
        if (currentGroup) {
            const auto profiles = currentGroup->Profiles();
            if (!profiles.isEmpty()) {
                const int firstId = profiles.first();
                if (Configs::dataManager->profilesRepo->GetProfile(firstId) != nullptr) {
                    return firstId;
                }
            }
        }
    }
    return -1;
}

void MainWindow::profile_start(int _id) {
    if (_id < 0) _id = get_profile_to_start();
    if (select_mode && !Configs::dataManager->settingsRepo->prepare_exit && _id >= 0 &&
        Configs::dataManager->profilesRepo->GetProfile(_id) != nullptr) {
        finish_select_mode(_id);
        return;
    }
    connection()->start(StartRequest{_id});
}

void MainWindow::profile_stop(bool crash, bool block, bool manual, bool interactive) {
    connection()->stop({crash, block, manual, interactive});
}

std::shared_ptr<Configs::Profile> MainWindow::runningSnapshot() const {
    return connection()->runningSnapshot();
}

void MainWindow::toggle_connection() {
    const auto state = connection()->connectionState();
    if (select_mode || state == ConnectionState::Connecting || state == ConnectionState::Stopping) return;
    if (runningSnapshot() != nullptr) {
        profile_stop(false, false, true);
        return;
    }
    profile_start(connection()->resolveLastProfile());
}
