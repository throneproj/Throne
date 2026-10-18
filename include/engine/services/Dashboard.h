#pragma once

namespace Engine::Dashboard {
    // The Clash API's external_ui folder, which only explains where the dashboard went. Main thread; relative to the config dir.
    void SeedClashUi();

    // The bundled sing-box dashboard when none is installed, and the page that hands it the API address and secret.
    void SeedApi();
} // namespace Engine::Dashboard
