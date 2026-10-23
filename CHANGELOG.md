#### 1.4.0-beta.4

* Add DNS settings to route profiles
* Add enable/disable toggle for route rules and a shortcut to the route profile editor
* Add Ctrl+K command palette for commands and settings
* Show the matched route rule of each connection
* Allow routing a connection through a specific server from the connections tab
* Extend Mux preset and allow custom per-profile Mux settings
* Improve resolve domains
* Improve macOS Tun DNS handling
* Use native file pickers
* Fix UI freezes when editing route rules in large profiles
* Update Qt to 6.12 and Go to 1.27.2 **1**
* Add Homebrew cask **2**
* Add WebDAV backup **3**
* Update Xray and add XDRIVE transport **4**
* Fixes and Improvements

**1**:
The main macOS builds now require macOS 14.4 or newer; older macOS versions should use the `macoslegacy` build.

**2**:
Stable releases are now published to Throne's own Homebrew tap and can be installed via `brew install --cask throneproj/tap/throne`, which also handles moving the app to `/Applications` and removing the quarantine flag. Refer to the [documentation](https://throneproj.github.io/get_started/installation/#homebrew) for more details.

**3**:
Backups can now be uploaded to and restored from any WebDAV server (Nextcloud, a NAS, Yandex Disk, ...) through the Backup section of Basic Settings. The WebDAV credentials stay on the device and are never included in a backup.

**4**:
Due to security reasons, XDRIVE profiles have no `vless://` link and are shared as Throne JSON links only. Xray can now also optionally trust the system certificates (Basic Settings > Security), and Xray profiles without their own uTLS fingerprint now use the global one.

#### 1.4.0-beta.3

* Add Amnezia auto l-values generator
* Improve group sort
* Show profile names in connection tab
* Remove Xray forced deprecation
* Refactor traffic graph
* Add input validation
* Add multi-group autoselector support
* Introducing Headless mode **1**
* Fixes and Improvements

**1**:
Throne can now be run as a process without GUI through docker or any daemon, controlled via the [API and our command line tool](https://github.com/throneproj/Throne-api), useful for automations and environments like raspberry-pi, routers or VMs without a Desktop. Refer to the [documentation](https://throneproj.github.io/headless/thronectl/) for more details.

#### 1.4.0-beta.2

* Fix TLS/XHTTP import issues
* Improve ECH Support
* Use monospace fonts for logs
* Add real-time filter support for logs
* Improve logs scroll behavior
* Add Throne API **1**
* Fixes and Improvements

**1**:
Throne now supports remote control via API calls, which enables integrating Throne in automation scripts/Remote machines as gateway. The API uses custom encryption and wire format, so the [Throne-api](https://github.com/throneproj/Throne-api) repository allows for communication with the API either as a helper library or the `thronectl` binary.

#### 1.4.0-beta.1

* Refactor OS Proxy setup
* Refactor Global Hotkey and add KDE Wayland support
* Allow routing any level of the current domain to route rules in connection table
* Add automatic apt/aur/rpm repo releases
* Add IP Scanner **1**
* Remove hijack and redirect features
* Update sing-box and enforce go Tun stack **2**
* Add Kill switch **3**
* Fixes and Improvements

**1**:
The new IP Scanner feature allows for scanning a list of IPs for specific traits (like TLS connection establishments, or URLTest of a config), which is useful to find accessible ranges of CDNs/Warp

**2**:
The new go Tun stack of sing-box offers a significant performance boost while solving compatibility issues of other stacks, thus deprecating system/gvisor stacks

**3**:
When enabled, no traffic will be able to leave the device (except some LAN/discovery ranges) unless tunneled through throne, effectively blocking leaks. The kill switch is bound to Throne, and will be removed if you exit throne. Its primary range of effect is the gap between profile switches, core crash or accidental profile stops

#### 1.3.2

* Fix auto start on windows
* Fix DNS issue with chain through endpoints
* Fix Xray Random DNS Select
* Fix Xray Interface Bind
* Allow customizing subscription connection parameters
* Add Subscription info UI (Thanks @VindEi)
* Fixes and Improvements

#### 1.3.1

**Throne's Android client has had major rewrites to make it more compatible with the Desktop app in both logic and feature, please try the [v2.0.0 release](https://github.com/throneproj/ThroneForAndroid/releases/tag/v2.0.0) and file an issue for the problems you encounter**

* Fix Testing on Linux failing when Tun is on
* Fix DNS not being hijacked in Tun mode
* Fix Xray not inheriting interface bind
* Fix Warp UDP socket binding
* Fix Amnezia on MacOS
* Fix legacy MacOS build
* Add IPv6 defaults to private range bypass
* Deprecate Hijack feature **1**
* Add customization features to Subscription Groups **2**
* Fixes and Improvements

**1**:
Hijack and Redirect features, alongside windows System DNS feature are deprecated and will be removed in the next release.

**2**:
Groups can now have custom UA/HWID, and perform testing/filtering automatically after update.

#### 1.3.0

**Introducing our [Android client](https://github.com/throneproj/ThroneForAndroid?utm_source=gemini)! Work is ongoing to bring the Android experience up to par with the desktop Throne app. Please submit any issues to the Android repo so we can continue improving the application.**

* Fix Tun/DNS issues
* Add OTP management
* Add OpenVPN/OpenConnect support
* Add endpoints to route profiles
* Add sing-box dashboard support
* Add Hysteria Gecko obfs
* Add Snell support
* Add L3 bridge support
* Allow altering private ranges in Tun settings
* Migrate to custom JSON editor
* Add TLS Spoof support
* Update AmneziaWG
* Fix windows Extra process privilege drop
* Fix MacOS Tun DNS address
* Add RPM Packaging (Thanks @bardiakz)
* Fix URL scheme registration and allow users to opt out from it
* Add Xray finalMask support (Thanks @VindEi)
* Fix Auto selector not recovering from network outage
* Fix Global insecure not applying to configs
* Significantly optimize window process searcher
* Migrate runtime stats to the main window
* Allow directly adding domains/addresses from connections tab to routing profiles (Thanks @renkagod)
* Migrate windows installers to Inno
* Add Masque protocol support
* Improve warp generator
* Improve trusttunnel support (Thanks @chasylexus)
* Replace connections table with per-process tree table (Thanks @renkagod)
* Add diagnostics collector
* Fix MacOS Tray crash (Thanks @chasylexus)
* Auto detect local DNS on MacOS in Tun mode (Thanks @chasylexus)
* Lots of Fixes and Improvements

#### 1.3.0-beta.4

* Improve warp generator
* Improve trusttunnel support (Thanks @chasylexus)
* Replace connections table with per-process tree table (Thanks @renkagod)
* Add diagnostics collector **1**
* Fixes and Improvements

**1**:
The Golang core can now be profiled through the GUI for diagnostics purposes.

#### 1.3.0-beta.3

* Fix Auto selector not recovering from network outage
* Fix Global insecure not applying to configs
* Update sing-box
* Update Xray-core to `v26.9.9`
* Significantly optimize window process searcher
* Migrate runtime stats to the main window
* Allow directly adding domains/addresses from connections tab to routing profiles (Thanks @renkagod)
* Migrate windows installers to Inno
* Add Masque protocol support
* Fixes and Improvements

#### 1.3.0-beta.2

* Add RPM Packaging (Thanks @bardiakz)
* Update AmneziaWG
* Fix URL scheme registration and allow users to opt out from it
* Add Xray finalMask support (Thanks @VindEi)
* Rewrite subscription import logic **1**
* Lots of Fixes and Improvements

**1**:
The behavior should remain the same, with the resource usage being significantly improved

#### 1.3.0-beta.1

* Fix Tun/DNS issues **1**
* Add OTP management
* Fix Windows Installer Location
* Add OpenVPN/OpenConnect support
* Add endpoints to route profiles
* Add sing-box dashboard support
* Add Hysteria Gecko obfs
* Add snell support
* Add L3 bridge support in Linux
* Allow altering private ranges in Tun settings
* Migrate to custom JSON editor
* Add TLS Spoof support
* Update amnesia wireguard
* Add close connection support to connections table
* Fix theme/color issues across certain setups
* Fix windows Extra process privilege drop
* Fix MacOS Tun DNS address
* Fix Xray dns/loopback issues
* Lots of fixes and improvements

**1**:
Many issues regarding looped connections in Tun mode have been fixed, it is suggested to upgrade Throne before submitting any more issues regarding such defects.

#### 1.2.4

* Fix Xray profile's DNS issues
* Fix some auto selector bugs
* Fix MacOS DNS issues in Tun mode
* Fix Xray config issues on Linux in Tun mode
* Fixed and Improvements

#### 1.2.3

**This version has been re-released to fix a startup crash on Unix systems**

* Add Auto-selector **1**
* Fix windows installer behavior
* Fix Xray profiles being broken on certain windows systems
* Add multiple file selection for importing from file
* Fix Exclude private range in Tun mode
* Lots of Fixes and Improvements

**1**:
You can now create profiles that try to find a suitable set of profiles from a group and automatically choose the best one and run it, optionally you can load-balance your traffic amongst these profiles too. It can automatically detect failure and switch to other available profiles.

#### 1.2.2

* Update sing-box **1**
* Add Amnezia v3 support
* Update Xray
* Fixes and Improvements

**1**:
This should fix the tun issues on Linux.

#### 1.2.1

* Improve certain UI sections
* Re-Enable strict route on windows again, with warnings on faulty systems (Thanks @0x3mp7y)
* Fix Xray downloadSetting interface binding (Thanks @0x3mp7y)
* Re-add `auto_redirect` on Linux
* Improve subscription update
* Fix Xray core DNS issues (Thanks @x15rte)
* Fixed and Improvements

#### 1.2.0

**Throne is free and open-source, and will always remain so. If you'd like to support development, you can now do so via [GitHub Sponsors](https://github.com/sponsors/Mahdi-zarei).**

* Add remote route profile support **1**
* Add `block` and `warp-bypass` default route support
* Add Raw routing profile support
* Improve Xray support
* Add Hiddify's TLS fragment and TLS Mixed-case support
* Add traffic statistics module **2**
* Improve menus and Tool Buttons
* Show connection speed in connections tab and allow sorting by it
* Improve system tray menu
* Add Mieru protocol
* Add runtime stats widget
* Disable strict route by default **3**
* Use Google DoH as default remote DNS
* Add security view to proxy Table's Type column **4**
* Add Remove insecure configs action
* Add Linux universal installer script (Thanks @thearialume)
* Lots of Fixes and Improvements

**1**:
You can now add remote routing profiles which can be updated remotely as well, refer to [Deeplink docs](https://throneproj.github.io/advanced/deeplinks/) for more information on how to add them via Deeplinks

**2**:
This module keeps an aggregated statistics of the traffic usage grouped by config/application, and can be disabled in settings

**3**:
Strict route causes issues on certain windows environments, so it is disabled by default

**4**:
It should be enabled in the settings and is not enabled by default

#### 1.2.0-beta.7

* Disable strict route by default
* Fix Xray full config issues
* Add softPink theme
* Use Google DoH as default remote DNS
* Improve context menus
* Update deep-link schemas
* Add security view to Proxy Table
* Add `Remove Insecure configs` menu action
* Fixes and Improvements

#### 1.2.0-beta.6

* Make Full Xray configs testable
* Migrate interface binding and initial DNS resolution of Xray configs to our fork
* Fixes and Improvements

#### 1.2.0-beta.5

* Add runtime stats Widget
* Add full Xray JSON array subscription support
* Use widgets for Tray server/routing profile selection
* Fixes and Improvements

#### 1.2.0-beta.4

* Refactor context menus
* Improve Start/Stop button (Thanks @renkagod)
* Allow disabling the traffic stats feature
* Add remote route profile support **1**
* Fix sub update logic **2**
* Fix mieru Link format
* Upgrade Xray core to `v26.6.27`
* Fixes and Improvements

**1**:
Route profiles can now be remotely subscribed to, allowing a route profile that gets updated from a remote source. Adding it can be done via [Deeplinks](https://throneproj.github.io/advanced/deeplinks/) as well.

**2**:
Previously, periodic jobs would run on an interval starting from the launch time, now they behave properly and trigger once the specified interval value has passed since the last update time.

#### 1.2.0-beta.3

* Fix testing current with default routing outbound direct using the wrong outbound
* Make sniff and hijack-dns always on
* Show current speed in connections Tab
* Use Xray for reality by default and allow importing all VLESS in Xray
* Update Sing-box to `v1.13.14`
* System Tray menu improvements (Thanks @renkagod)
* Add Mieru protocol support
* Fixes and Improvements

#### 1.2.0-beta.2

* Update Xray core And Improve its Tun Handling
* Add Hiddify's TLS fragment and TLS Mixed-case
* Add traffic statistics module **1**
* Add Profile start/stop button **2**
* Add Xray config validation
* Fixes and Improvements

**1**:
Aggregates and summarizes the traffic used by configs/apps over the last 90 days for better insight into one's usage

**2**:
The top toolbar has been changed to simplify operations and add room for future features

#### 1.2.0-beta.1

* Allow generating warp config as Wiregaurd profile
* Improve route profile import/export And add deeplink support for it
* Add block and warp-bypass routing profile default outbounds **1**
* Disable `auto_redirect` and fix Tun Exclude subnets
* Add raw routing profile support **2**
* Fixes and Improvements

**1**:
warp-bypass will allow to customize what goes through warp and what uses the proxy directly. Works the same as proxy if warp is not enabled

**2**:
You can write your own JSON routing section which will be used as the entire `route` section of sing-box, allowing for custom rule-sets and more control over the routing logic for advanced users

#### 1.1.6

* Fix system proxy
* Use polling mode for windows process searcher
* Improve Loopback detection rules
* Fix some Xray fields being registered with wrong type
* Minor fixes and improvements

#### 1.1.5

* Improve font handling
* Fix symlink issues on windows
* Improve proxy list refresh
* Fix MacOS database storage location
* Extend XHTTP settings support (Thanks @Katze-942)
* Improve Xray traffic handling (Thanks @0x3mp7y)
* Add deeplink support **1**
* Added Amenzia Wireguard support again **2**
* Fix wireguard start issues
* Add Old Nekoray Themes **3**
* Update Xray allow insecure handling
* Update Sing-box to v1.13.13
* Fix Extra core traffic loop
* Fixes and Improvements

**1**:
Currently only adding subscriptions is supported, but we plan to extend its capabilities

**2**:
Amenzia wireguard has now been readded, and it supports the version 2 schematics

**3**:
FlatGray, LightBlue and BlackSoft themes have been readded

#### 1.1.4

* Fix slow startup on Linux
* Fix core parent check not following symlinks
* Fix ram disk installation and core parent check incompatibility
* Fix Chain config build
* Fix traffic looper and chain compatibility issues
* Other Fixes and Improvements

#### 1.1.3

* Add ICMP network to routing profile
* Allow disabling Mixed inbound
* Fix windows installer permission handling
* Make windows installer universal
* Add chain support for Xray
* Disable system proxy on profile stop
* Add ECH server name
* Allow chaining Extra core profiles (only as the last hop)
* Route extra/xray core dns to direct dns to avoid loopback
* Add Xray custom outbound/custom full config
* Allow chaining custom outbound/custom Xray outbound/custom Xray configs
* Add support for Xray json importing
* Add `bind_interface`, `inet_bind_ipv4/6` options
* Add direct outound **1**
* Add option to restart profile on system proxy disable
* Add backup and restore capability
* Security enhancements **2**
* Add log auto scroll support (Thanks @alpatovdanila)
* Force clear system proxy on exit (Thanks @4RH1T3CT0R7)
* Add XHTTP Xpadding options (Thanks @Yulinanami)
* Update Qt (6.11.1), Golang (1.25.9), Sing-box (1.13.12) And Xray (26.3.27)
* Lots of fixes and Improvements

**1**:
Its main use is to bind to non-default interfaces (Corporate OpenVPN etc) when using Tun mode

**2**:
The core is now strictly required to be in the same directory as the GUI executable, and the name of the GUI executable should also not be changed. Also the core may only be started by the GUI executable, communicates with the GUI via unix socket/named pipes and runs Extra cores as unprivileged processes.

#### 1.1.2

* Improve column resizing
* Added new Protocols: Naive, Juicity, ShadowTLS  and TrustTunnel
* Use Task scheduler for windows auto start on startup **1**
* Show current profile's Country
* Allow disabling DNS routing
* Fix MacOS Tray and Tray's context menu bugs (thanks @AliKarami)
* Add bypass action and ICMP reply reject in routing
* Redesign routing profile advanced UI
* Add wifi ssid/bssid options to routing rules
* Add test progress indicator
* Add support for filtering profiles by port
* Add inbound authorization support
* Fix active profile being stopped by default on sub update
* Lots of fixes and improvements

**1**:
Instead of registry, now we use task scheduler to make throne start on startup. Creating a task requires UAC approval, and will start Throne as admin if it was running as admin before without a need for UAC approval on every start (unless you configure Throne to start as normal user in the security settings). Note that if you had auto startup enabled, you will be prompted to approve the task creation on starting this version of Throne, this is to migrate the old startup method to the new one.

#### 1.1.1

* Add HTTP(S) Proxy import support
* Fix subscription update ignoring profile names
* Fix MacOS tun error
* Fix Linux systemd-dns deadlock
* Fixes and Improvements

#### 1.1.0

**IMPORTANT: This version's database is not compatible with any previously released version and you MUST start from a clean installation and migrate your data by hand (import/export and manual setting configuration). By upgrading in-place, you will LOSE ALL YOUR CONFIGURATION or, depending on whether you are on a beta version, crash on startup**

* Added Xray Vless **1**
* Add custom hwid parameters (Thanks @hgx100)
* Change app icon in taskbar
* Migrate to SQLite **2**
* Add auto clear unavailable profiles to group setting
* Make simple rules index adjustable
* Add Outgoing IP Test **3**
* Improve Profile Table Columns customization options
* Add log filtering **4**
* Removed Amenzia wireguard support **5**
* Add warp gen/chain support **6**
* Upgrade sing-box to `v1.13.2` **7**
* Lots of Fixes and Improvements

**1**:
Xray core is now supported for VLESS configs. Note that not all Transports are supported and that you cannot switch between the Cores. You can however, customize when Xray core is used when importing VLESS profiles from the settings.

**2**:
This improves Throne's performance and resource usage when having lots of profiles. The current database system is not compatible with the previous one, and you have to migrate your data by hand (via import/export).

**3**:
Determines the profile's outbound IP, country and availability.

**4**:
Logs can now be filtered with granular control.

**5**:
Due to poor Amenzia wireguard documentation, we can no longer support it.

**6**:
Warp configs can now be generated, and set to be automatically chained as the egress route for the connections.

**7**:
Not all features added in sing-box are supported yet, but they are all planned for future releases, so please do not open feature requests regarding that.

#### 1.1.0-beta.3

**Important: We have migrated to SQLite for managing Throne's database. The versions below 1.1.0-beta.2 are not compatible with this version onwards and you have to migrate your data manually.**

* Fix multiple bugs related to operating massive profile counts
* Fix core-gui connection multiplexing
* Add log filtering **1**
* Improve systemd-resolved detection
* Lots of fixes and Improvements

**1**:
The logs can now be filtered with granular control

#### 1.1.0-beta.2

**Important: We have migrated to SQLite for managing Throne's database. The versions below 1.1.0-beta.2 are not compatible with this version onwards and you have to migrate your data manually.**

* Fix parsing Xray links
* Improve Throne's crash dumps **1**
* Force reset the Core on Close hang-ups
* Fix country test
* Fix Xray access logs
* Change app icon in taskbar
* Add default shortcuts
* Migrate to SQLite **2**
* Add auto clear unavailable profiles to group setting **3**
* Make simple rules index adjustable
* Add an option to skip confirmation on removing profiles
* Add Outgoing IP Test **4**
* Make Test column configurable **5**
* Allow for more granular column width control **6**
* Add an option to decide when to use Xray core when importing
* Update Qt and Golang
* Fixes and Improvements

**1**:
The crash dumps are now more readable, and can be more efficiently used for finding root causes of the crashes.

**2**:
This change makes Throne's memory usage independent of the count of profiles and resolves UI Lags when working with too many profiles.

**3**:
Enabling it will remove the unavailable profiles after a URL test has finished.

**4**:
This test can determine the outgoing IP of your configuration, along its country of origin.

**5**:
You can now choose which test data to show on the column, and how to sort according to this column.

**6**:
Column widths of the profile table are now always adjustable, and you can use the Group's context menu to refresh their widths to the default ones. Given that the content of the columns may change, you may have to refresh the widths after testing/using profiles to adjust the Test/Traffic column's widths.

#### 1.0.13

* Fix Hysteria/TUIC link import
* Hide dock icon on MacOS
* Update sing-box to `v1.12.14`
* Fix some transport config generation edge cases
* Add custom hwid parameters (Thanks @hgx100)
* Fix front proxy
* Fix Linux systemd-resolved DNS
* Fix proxy usage on http requests
* Fix orphan profiles not being deleted
* Fix crash
* Fixes and Improvements

#### 1.1.0-beta.1

* Fix Hysteria/TUIC link import
* Added Xray Vless **1**
* Hide dock icon on MacOS
* Update sing-box to dev-next branch **2**
* Fix some transport config generation edge cases
* Add custom hwid parameters (Thanks @hgx100)
* Fix front proxy
* Fix Linux systemd-resolved DNS
* Fix proxy usage on http requests
* Fix orphan profiles not being deleted
* Fixes and Improvements

**1**:
Xray core is now embedded in the core, and is used for the newly added `Vless (Xray)` config. Note that the xhttp configs from previous versions are going to break on updating to this version and you have to re-import them to be able to use them

**2**:
Due to some dependency issues, we have upgraded sing-box to dev-next (alpha version)

#### 1.0.12

* Fix XHTTP extra parameters parsing
* Improve Clash(Meta/mihomo) subscription support
* Support SIP008 format subscription
* Minor other fixes

#### 1.0.11

* Fix process names not being captured
* Fix chain using reverse order
* Fix VLESS profile edit breaking the config
* Fix reset core hanging the app

#### 1.0.10

**This is a follow-up release to fix the issues in the 1.0.9 version**

**This version has been re-released to fix a bug with shadowsocks plugins**

* Fixed various bugs related to config parsing **1**
* Fix brutal config gen
* Minor other fixes

**1**:
The fixes will make migrating from 1.0.8 to work properly, but they are not going to fix configs broken in the 1.0.9 release. It is suggested to migrate your data from the 1.0.8 version (copy the config folder to 1.0.10 version folder).

#### 1.0.9

**This update should be considered experimental, and it is highly suggested to keep the older version as a backup**

**It is kindly requested from users that are willing to test and report issues to use this version so that we can refine it for the later versions**

* Refactor and Migrate the internal structures **1**
* Save window size/position
* Add advanced connection options **2**
* Add experimental xhttp support **3**
* Return custom icon for Tray **4**
* Fixes and Improvements

**1**:
We have refactored the internal structure we use for saving profiles. Some bugs are to be expected, so please submit issues with complete information regarding the bugs you see.

**2**:
You can now configure ECH, Dial fields and other TLS security options from the UI. Note that some features are not yet supported in the sing-box version we use and are disabled for now.

**3**:
We are planning to add a more comprehensive Xray outbound support to Throne in the upcoming releases, and the format and UI for such outbounds may change, so such options are not guaranteed to remain functional on the upcoming releases and may need to manual re-importing.

**4**:
You can now choose a square-dimensioned PNG file as Icon for each state of the app in the Tray, you can also refer to the [Iconsets](https://github.com/throneproj/Iconsets) repo to download or submit Iconsets for Throne.

#### 1.0.8

* Fix Darwin Tun mode DNS Leak
* Add country test
* Fix windows size not being remembered
* Fix adblocker not working
* Fix and Improve system proxy in various Linux desktop platforms
* Add debian-system-qt build **1**
* Update sing-box to `v1.12.12`
* Add local DNS override **2**
* Fixes and Improvements

**1**:
This release does not pack its own libraries, and uses the system provided ones instead. This is particularly useful for users who have issues with the packed libraries.

**2**:
The setting has been moved to DNS settings, and will have wider effects when local DNS cannot work.

#### 1.0.7

* Readd `ctrl+f` search functionality with improvements
* Improve emoji support
* Fix hotkey capture
* Add custom shortcut settings **1**
* Fix MacOS admin check
* Make timeout configurable for url/speed test
* Add profile's country emoji to test results after speedtest
* Add Wireguard config file format support
* Support dropping text/image/files to the app for importing
* Add import from file option
* Update sing-box to `v1.12.10`
* Update Qt to `6.10.0`
* Build windows x64 with OpenSSL instead of Schannel
* Lots of fixes and improvements

**1**:
Most of menu options now do not have a default shortcut and instead, can have their shortcut configured through the hotkey settings.

#### 1.0.7-beta.1

* Readd `ctrl+f` search functionality
* Improve emoji support
* Fix hotkey capture
* Add custom shortcut settings **1**
* Fix MacOS admin check
* Make timeout configurable for url/speed test
* Add profile's country emoji to test results after speedtest
* Add Wireguard config file format support
* Support dropping text/image/files to the app for importing
* Add import from file option
* Update sing-box to `v1.12.9`
* Lots of fixes and improvements

**1**:
Most of menu options now do not have a default shortcut and instead, can have their shortcut configured through the hotkey settings.

#### 1.0.6

* Update Sing-box to `v1.12.8`
* Add Hysteria1 port hopping
* Add `windows-arm64` build for Windows on Arm
* Add `macoslegacy-amd64` build for Catalina(10.15) or Big Sur(11) on Intel-based Mac
* Add HWID to HTTP headers when requesting subscription (Thanks @0-Kutya-0)
* Lots of Fixes and Improvements

#### 1.0.5

**This is a hotfix to version `1.0.4`**

* Fix thread deadlocks
* Fix config generation with `Enable Tun Routing`

#### 1.0.4

* Remember installation path in installer
* Fix MacOS event handling
* Update Sing-box to `v1.12.4`
* Fix loopback addresses not being accessible in Tun mode
* Persist Tun status across runs
* Lots of Fixes and Improvements

#### 1.0.2

* Migrate to split rulesets
* Add an option to enable beta updates
* Fix Linux Tun conflict with systemd-resolved
* Add AdBlock routing profiles
* Lots of Fixes and Improvements

#### 1.0.2-beta.6

* Fixes and Improvements

#### 1.0.2-beta.5

* Auto-mapping old rule-set name
* Add more mirrors
* Fixes and Improvements

#### 1.0.2-beta.4

* Add local rule-set cache **1**

**1**:
Local rule-set cache stores in `config/cache.db`

#### 1.0.2-beta.3

**~Important: Due to changes in rule sets, the rule-set name was changed. Route profiles are not compatible with old versions.~**

**~Please delete the old build-in route profiles. Download new route profiles. If you are using custom route profile, you have to edit your profile. Change *_IP to geoip-* and *_SITE to geosite-*.~**
* Fix bugs in rulesets configurations

#### 1.0.2-beta.2

* Fix crash

#### 1.0.2-beta.1

* Migrate to remote ruleset repo **1**

**1**:
Geo assets are no longer generated and kept locally, and are now listed and downloaded remotely, report any problems you may encounter

#### 1.0.1

* Fix updater again **1**
* Fix core start signal
* Improve auto complete dropdown
* Update sing-box to `v1.12.1`
* Add `TLS Fragment` and `TLS Record Fragment` support
* Add Throne to RPM package
* Add Mozilla cert store support
* Add `AnyTLS` support
* Add `Tailscale` support
* Update go to `1.25.0`
* Fixes and Improvements

**1**:
You will not be able to use the updater to update to this version, but it will work for the next releases

#### 1.0.0

**The project has been renamed to `Throne`, You cannot use the in-app updater to update to this one. To update, extract the contents of the zip files, and copy the config folder from `nekoray` and paste it besides the `Throne` binary**

* Improve windows installer
* Add Linux arm build
* Merge legacy windows builds
* Fix default outbound not being saved
* Implement disable Tray option (Thanks @IAGrig)
* Update sing-box to v1.11.15
* Add support for remote rulesets
* Make the built-in profiles downloadable **1**
* Improve operations on big groups
* Hide the Update button when updater is not present
* Fix Linux Tun mode incompatibility with `systemd-resolved`
* Fix MacOS Tun name conflict
* Fix windows set proxy not working
* Add Amenzia Wireguard support **2**
* Add an option to Reset assets
* Add automatic Reset Assets scheduler
* Add an option to Randomize the listen port
* Migrate to `protorpc`
* Fixes and Improvements

**1**:
 The built-in profiles are no longer included by default, and can now be downloaded as needed.

**2**:
The amenzia protocol is a temporary and weak obfuscation method for wireguard and lacks the technical unrecognizability of methods like `swgp` and has been solely added due to user requests. It has been cherry-picked from Lantern's Implementation and any bugs related to it will not be addressed on our end.

#### 1.0.0-beta.7

* Add an option to Reset assets
* Add automatic Reset Assets scheduler
* Add an option to Randomize the listen port
* Fixes and Improvements

#### 1.0.0-beta.6

* Fix MacOS Tun name conflict
* Redesign Icons (Thanks @xmha97)
* Fix windows set proxy not working
* Add Amenzia Wireguard support **1**
* Fixes and Improvements

**1**:
The amenzia protocol is a temporary and weak obfuscation method for wireguard and lacks the technical unrecognizability of methods like `swgp` and has been solely added due to user requests. It has been cherry-picked from Lantern's Implementation and any bugs related to it will not be addressed on our end.

#### 1.0.0-beta.5

* Improve operations on big groups
* Use `dhcp://auto` for default direct dns on Linux/MacOS
* Hide the Update button when updater is not present
* Fixes and Improvements

#### 1.0.0-beta.4

* Fix Tun mode being broken after shutdown
* Fix route rules being generated incorrectly
* Fixes and improvements

#### 1.0.0-beta.3

* Fix updater again
* Improve windows installer
* Enhance app icon
* Add support for remote rulesets
* Make the built-in profiles downloadable **1**
* Fixes and Improvements

**1**:
The built-in profiles are no longer included by default, and can now be downloaded as needed.

#### 1.0.0-beta.2

* Fix profile order not being saved
* Change app icon
* Fix deb package's icon
* Fixes and Improvements

#### 1.0.0-beta.1

**The project has been renamed to `Throne`, You cannot use the in-app updater to update to this one. To update, extract the contents of the zip files, and copy the config folder from nekoray and paste it besides the `Throne` binary**

* Improve windows installer
* Fix traffic not being saved on exit
* Fix drag and drop in profiles
* Fix updater
* Add Linux arm build
* Merge legacy windows builds
* Fix default outbound not being saved
* Implement disable Tray option (Thanks @IAGrig)
* Migrate to simple protobuf
* Revert Qt back to 6.9.0
* Update sing-box to v1.11.15
* Fixes and improvements
