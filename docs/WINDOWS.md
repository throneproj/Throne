# Windows 11 build, signing, and installers

Throne ships three Windows distribution channels that follow Microsoft packaging guidance:

| Channel | Format | Docs |
|---|---|---|
| Setup (default) | Inno Setup 6 universal `.exe` | [Choose a distribution path](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/choose-distribution-path) (EXE/Inno remains valid for complex Win32 apps) |
| MSIX | Full-trust packaged classic app | [Manual desktop conversion](https://learn.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-manual-conversion), [MakeAppx](https://learn.microsoft.com/en-us/windows/msix/package/create-app-package-with-makeappx-tool) |
| winget | Manifest over the Inno installer | [Create a package manifest](https://learn.microsoft.com/en-us/windows/package-manager/package/manifest) |

Portable ZIPs remain available for power users.

## Local toolchain

From an **elevated** PowerShell in the repo root:

```powershell
powershell -ExecutionPolicy Bypass -File script/setup_windows_dev.ps1
```

This installs CMake, Ninja, Go, Inno Setup, VS 2022 Build Tools (MSVC + Windows 11 SDK), and downloads the same Qt/OpenSSL 7z archives CI uses.

Then in a **Developer PowerShell for VS 2022**:

```powershell
$env:CMAKE_PREFIX_PATH = "$PWD\.tools\Qt\lib\cmake"
$env:OPENSSL_ROOT_DIR  = "$PWD\.tools\openssl"
$env:INPUT_VERSION     = "1.0.5"
cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -S . -B build
curl.exe -fLso build/srslist.h https://raw.githubusercontent.com/throneproj/routeprofiles/rule-set/srslist.h
ninja -C build
```

Put `ThroneCore` (and friends) next to `build\Throne.exe` the same way CI merges the Go artifact, or unpack an upstream `windows-amd64` zip into `deployment\windows-amd64` for packaging experiments.

## Authenticode signing

`script/windows_sign.ps1` signs PE/MSIX files with `signtool` ([SignTool](https://learn.microsoft.com/en-us/windows/win32/seccrypto/signtool)).

Local sideload testing (self-signed only):

```powershell
powershell -ExecutionPolicy Bypass -File script/create_windows_dev_cert.ps1
# then pack/sign in the same shell so WINDOWS_CERT_* is set
```

Configure one of:

| Secret / env | Purpose |
|---|---|
| `WINDOWS_CERT_PFX_BASE64` + `WINDOWS_CERT_PASSWORD` | CI: base64-encoded `.pfx` |
| `WINDOWS_CERT_PFX_PATH` + `WINDOWS_CERT_PASSWORD` | Local path to `.pfx` |
| `WINDOWS_CERT_THUMBPRINT` | Cert already in the Windows store |
| `WINDOWS_CERT_TIMESTAMP_URL` | Optional RFC3161 timestamp server |
| `WINDOWS_CERT_SUBJECT` | Publisher DN for MSIX (`CN=...` must match the cert) |

Without a certificate, signing is skipped so unsigned CI/local packs still succeed. Set `WINDOWS_SIGN_STRICT=1` in CI when a release must be signed.

## Pack release artifacts

```bash
# after deployment/windows-* dirs exist (CI does this):
bash script/pack_release.sh windows al4al-0.1.0
```

That produces:

- `Throne-<tag>-windows-universal-installer.exe` (Inno; signed when cert present)
- portable ZIPs
- `Throne-<tag>-windows-amd64.msix` / `windows-arm64.msix` when the Windows SDK is available
- signed payload EXEs when a cert is configured

Generate winget YAML from the installer:

```powershell
powershell -ExecutionPolicy Bypass -File script/generate_winget_manifest.ps1 -Tag al4al-0.1.0
```

## MSIX vs Setup

The MSIX package is a **full-trust** `packagedClassicApp` with `runFullTrust` and `allowElevation`. Prefer it for Store/Intune/App Installer flows.

Prefer the **Inno Setup** installer when you need:

- Windows service install (`dist/windows/install-service.bat`)
- Config/database living next to the EXE without package virtualization
- In-place updater behavior identical to current GitHub Releases
- Legacy Windows 7/8 (portable legacy ZIPs / Setup OS version checks)

App Installer template: `packaging/msix/Throne.appinstaller.template.xml`.
