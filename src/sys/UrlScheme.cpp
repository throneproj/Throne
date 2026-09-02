#include "include/sys/UrlScheme.hpp"

#include "include/global/Configs.hpp"
#include "include/global/Logger.hpp"

bool UrlScheme_IsSupported() {
    return !UrlScheme_DesiredState().isEmpty();
}

void UrlScheme_RegisterIfNeeded() {
    if (!Configs::dataManager->settingsRepo->url_scheme_auto_register) return;

    const QString desired = UrlScheme_DesiredState();
    if (desired.isEmpty()) return;

    auto settings = Configs::dataManager->settingsRepo.get();
    const bool mirrorMatches = settings->url_scheme_mirror == desired;
    if (mirrorMatches && UrlScheme_IsCurrent()) return;
    if (mirrorMatches) LOG_WARN("url scheme registration points elsewhere (another install?), reclaiming it");

    UrlScheme_Apply();
    settings->url_scheme_mirror = desired;
    settings->Save();
}

bool UrlScheme_Install() {
    const QString desired = UrlScheme_DesiredState();
    if (desired.isEmpty()) return false;

    UrlScheme_Apply();
    auto settings = Configs::dataManager->settingsRepo.get();
    settings->url_scheme_mirror = desired;
    settings->Save();
    return UrlScheme_IsCurrent();
}

void UrlScheme_Uninstall() {
    UrlScheme_Remove();

    // An empty mirror never matches a desired state, so re-enabling auto registration writes the entries again instead of trusting the removed ones.
    auto settings = Configs::dataManager->settingsRepo.get();
    settings->url_scheme_mirror.clear();
    settings->Save();
}
