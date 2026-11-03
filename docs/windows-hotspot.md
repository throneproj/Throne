# Windows mobile hotspot

The Windows GUI includes an **Open hotspot** checkbox next to **Tun Mode** and **System Proxy**. It uses the saved Windows mobile hotspot name and password. Configure these in Windows Settings before the first use.

## Use

1. Run Throne as administrator, enable TUN, and connect a profile.
2. Wait for the availability check. The checkbox is enabled only when Windows exposes the active TUN profile, reports tethering capability, and no existing mobile hotspot/legacy ICS session would be replaced.
3. Check **Open hotspot**. It remains busy until Windows starts the hotspot and the helper confirms forwarding through TUN without enabling forwarding on the physical uplink.
4. Connect a client to the saved hotspot network. The checkbox's tooltip shows its network name.
5. Uncheck it to stop sharing. Stopping the profile, disabling TUN, or exiting Throne also requests shutdown of the owned hotspot.

The checkbox is a live session control, not a saved autostart preference. It is hidden on non-Windows platforms. After a TUN restart, start sharing again explicitly. The active routing profile continues to determine proxy/direct destinations; this option does not force every destination through a server.

If the checkbox is disabled, its tooltip contains the reason. Existing unrelated sharing is not taken over. An unavailable Windows service, permission problem, disabled/unsupported Wi-Fi, or other OS restriction can prevent startup.

## Implementation

`WindowsHotspot` asynchronously launches the embedded `script/windows_hotspot_ui.ps1` with the system Windows PowerShell 5.1 executable. Probe mode is read-only; Run mode owns one newly created WinRT hotspot session. The script is compressed and passed in memory via `-EncodedCommand`, rather than executed from a writable temporary file. Console windows are suppressed and credentials are never printed.

The helper finds the active `throne-tun` adapter's connection profile by GUID and calls `NetworkOperatorTetheringManager.CreateFromConnectionProfile(profile)` followed by `StartTetheringAsync()`. This bypasses Windows Settings' source list, where a valid TUN profile can be missing. It does not change NCSI, registry values, firewall rules, VPN configuration, or core egress binding.

Stopping uses a per-session cancellation marker; this avoids hidden PowerShell stdin behavior. The helper retains the original WinRT manager, watches its parent process and the TUN adapter, and performs shutdown in `finally`. It does not abandon a pending startup operation simply because the GUI requested stop. Probe processes can time out safely because they perform no network mutation.

## Validation

Tested with Windows 11, MSVC 2022 and Qt 6.8.3:

- Complete Throne GUI compiles and links.
- Six automated controller scenarios, plus test setup/cleanup, pass: TUN gating, unavailable Windows capability, start/stop and repeated start, startup failure, TUN stop during startup, and destruction while a helper needs cleanup.
- The real embedded helper passes the opt-in integration test: capability probe, hotspot startup through the active TUN profile, forwarding verification, and shutdown.
- The earlier reference workaround was also verified with a connected phone and a site normally requiring VPN.

Normal automated tests do not change the machine's hotspot. The live smoke executable is built only with an explicit option and does change the saved Windows hotspot:

```powershell
cmake -S tests/windows-hotspot -B build-hotspot-tests -DCMAKE_PREFIX_PATH=<Qt>
cmake --build build-hotspot-tests
ctest --test-dir build-hotspot-tests --output-on-failure

# Optional and manual: requires an active elevated TUN and no existing hotspot.
cmake -S tests/windows-hotspot -B build-hotspot-tests -DHOTSPOT_BUILD_LIVE_SMOKE=ON
cmake --build build-hotspot-tests
build-hotspot-tests/hotspot_live_smoke.exe
```

The Go core was not rebuilt or changed for this feature. Testing here covers one physical Wi-Fi adapter; Windows versions, other hardware, sleep/resume, IPv6 and kill-switch combinations still need broader coverage before an official release. Forced termination of the helper itself can bypass its cleanup; Windows mobile hotspot settings remain the manual recovery control.

## References

- [Throne issue #1916](https://github.com/throneproj/Throne/issues/1916)
- [Throne's documented workaround](https://throneproj.github.io/ru/guides/recipes/#мобильный-хот-спот-windows-в-режиме-tun)
- [Microsoft: CreateFromConnectionProfile](https://learn.microsoft.com/en-us/uwp/api/windows.networking.networkoperators.networkoperatortetheringmanager.createfromconnectionprofile)
