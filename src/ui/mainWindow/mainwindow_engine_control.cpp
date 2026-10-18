#include "include/ui/mainwindow.h"

#include "include/engine/EngineHost.h"
#include "include/engine/connection/ConnectionManager.h"
#include "include/engine/services/StatsService.h"
#include "include/engine/tests/TestRunner.h"

Engine::ConnectionManager *MainWindow::connection() const {
    return Engine::host()->connection();
}

Engine::CoreSupervisor *MainWindow::supervisor() const {
    return Engine::host()->supervisor();
}

Engine::TestRunner *MainWindow::tests() const {
    return Engine::host()->tests();
}

Engine::VpnAuthController *MainWindow::vpnAuth() const {
    return Engine::host()->vpnAuth();
}

Engine::ExitController *MainWindow::exitController() const {
    return Engine::host()->exitController();
}
