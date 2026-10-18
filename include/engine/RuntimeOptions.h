#pragma once

namespace Engine {
    // Process-wide, fixed at EngineHost::create(); the defaults are the GUI's behaviour.
    struct RuntimeOptions {
        bool headless = false;
        // !--no-resume and THRONE_RESUME not off; the GUI never clears it.
        bool resume = true;
        // False: the system proxy is never set, restored, cleared at exit, persisted or accepted from the API.
        bool systemProxy = true;
        // True: a remembered Tun the boot cannot restore holds the boot start (degraded tun_unavailable), never proxy-only.
        bool tunRequired = false;
        // False: an unknown Tun privilege answer counts as no, instead of a blocking IsPrivileged RPC on the main thread (Unix).
        bool blockingPrivilegeCheck = true;
        // Unattended starts that hit missing geoip/geosite files download them, and a start that failed for them retries once.
        bool geoAutoDownload = false;
        // The exit IP/country lookup after each start; also needs the exit_country_lookup setting.
        bool exitCountryLookup = true;
        // --no-kill-switch: a saved kill_switch=true fails closed (code overridden) instead of arming, for this boot only.
        bool killSwitchOverride = false;

        static RuntimeOptions Headless(bool resume, bool noKillSwitch = false) {
            RuntimeOptions options;
            options.headless = true;
            options.resume = resume;
            options.killSwitchOverride = noKillSwitch;
            options.systemProxy = false;
            options.tunRequired = true;
            options.blockingPrivilegeCheck = false;
            options.geoAutoDownload = true;
            options.exitCountryLookup = false;
            return options;
        }
    };
} // namespace Engine
