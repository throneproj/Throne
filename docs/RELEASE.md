# How we release

**Story for 0.1:** Nekoray users can move to this Throne fork — `pcs` / `pinSHA256` cert pins work; full Throne protocol set on Windows / macOS / Linux.

## Branches and tags

| | |
|---|---|
| `upstream` → `throneproj/Throne` | Merge often |
| `dev` | Default; all AL4AL work |
| Tags `al4al-0.x.y` | Our releases (never reuse upstream tag names) |

## Local Linux check

```bash
cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -S . -B build
curl -fLso build/srslist.h https://raw.githubusercontent.com/throneproj/routeprofiles/rule-set/srslist.h
ninja -C build
# Put ThroneCore next to build/Throne (CI artifact or upstream linux-amd64 zip)

cmake -DROOT=$PWD -P cmake/EngineBoundary.cmake
bash script/ci/p0_xhttp_pin_check.sh
bash script/ci/p1_sub_nul_check.sh
```

Ubuntu 24 system-Qt `.deb`: `script/pack_ubuntu24_amd64.sh` reads `packaging/ubuntu24_version.txt`, or takes `[version]` / `UBUNTU_DEB_VERSION`. Current local package line: **`1.0.5+al4al.dev-1`** (PPTP / L2TP-IPsec egress with PSK, dependency check/fix).

## Local Windows check

See [WINDOWS.md](WINDOWS.md). Short path:

```powershell
powershell -ExecutionPolicy Bypass -File script/setup_windows_dev.ps1   # elevated for VS Build Tools
# Developer PowerShell for VS 2022:
$env:CMAKE_PREFIX_PATH = "$PWD\.tools\Qt\lib\cmake"
$env:OPENSSL_ROOT_DIR  = "$PWD\.tools\openssl"
$env:INPUT_VERSION     = "1.0.5"
cmake -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -S . -B build
curl.exe -fLso build/srslist.h https://raw.githubusercontent.com/throneproj/routeprofiles/rule-set/srslist.h
ninja -C build
```

## Ship

1. Gates above green (also run on every push via `ci-push.yml`)
2. Optional Windows Authenticode secrets for signed Setup/MSIX:
   - `WINDOWS_CERT_PFX_BASE64`, `WINDOWS_CERT_PASSWORD`
   - optional repo vars: `WINDOWS_CERT_SUBJECT`, `WINDOWS_CERT_TIMESTAMP_URL`, `WINDOWS_SIGN_STRICT=1`
3. Manual full matrix when you want binaries:
   ```bash
   gh workflow run build.yml -R AL4AL/Throne -f tag=al4al-0.1.0 -f publish=r
   ```
   (`publish`: `r` = release, `p` = prerelease, empty = build only)
4. After a Windows pack, submit `deployment/Throne-<tag>-winget/` to [microsoft/winget-pkgs](https://github.com/microsoft/winget-pkgs) (see `packaging/winget/README.md`).

Pushing or editing workflow YAML needs `gh auth refresh -h github.com -s repo,workflow`.
