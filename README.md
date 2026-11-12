# Throne (AL4AL fork)

Qt desktop proxy manager (sing-box / Xray). Fork of [throneproj/Throne](https://github.com/throneproj/Throne); successor to archived [AL4AL/nekoray](https://github.com/AL4AL/nekoray).

**What we ship and how we release:** [docs/GOLDEN.md](docs/GOLDEN.md) · [docs/RELEASE.md](docs/RELEASE.md) · [docs/UPSTREAM.md](docs/UPSTREAM.md)

Throne manages proxy profiles and subscriptions, routes traffic with rule-based routing profiles, and works either as a system proxy or as a Tun VPN. It also runs headless, as a service or in a container, controlled with [thronectl](https://github.com/throneproj/Throne-api): see [Headless Throne](https://throneproj.github.io/headless/).

<p align="center">
  <img width="820" alt="Throne main window" src="https://github.com/user-attachments/assets/af4a8e32-7e55-430c-9402-ec2d665cf71a" />
</p>

## Installation

[![GitHub All Releases](https://img.shields.io/github/downloads/AL4AL/Throne/total?label=downloads-total&logo=github&style=flat-square)](https://github.com/AL4AL/Throne/releases)

Upstream portable builds: [throneproj/Throne releases](https://github.com/throneproj/Throne/releases). The [installation guide](https://throneproj.github.io/get_started/installation/) covers APT and RPM repositories, the AUR, and other package managers.

### Windows 11

Preferred: **Setup (Universal)** Inno installer from GitHub Releases (selects x64 / ARM64 / legacy automatically). Also published: **MSIX** (x64 / ARM64) and **winget** manifests (`packaging/winget`). Build, signing, and packaging notes: [docs/WINDOWS.md](docs/WINDOWS.md).

```powershell
# after a release is published to winget-pkgs:
winget install AL4AL.Throne
```

Local Windows toolchain:

```powershell
powershell -ExecutionPolicy Bypass -File script/setup_windows_dev.ps1
```

### macOS
Apple platforms have a very strict security policy and since Throne does not have a signed certificate, you will have to remove the quarantine using `xattr -d com.apple.quarantine /path/to/throne.app`. Move `Throne.app` to `/Applications` before the first launch — the built-in privilege escalation opens `Terminal` to make the core setuid-root, and that step can fail while the app is still inside `~/Downloads`.

The Homebrew cask (macOS 14.4 or newer, Apple Silicon and Intel) does both for you; update with `brew upgrade --cask throne`.
```bash
brew install --cask throneproj/tap/throne
```

### Linux package repositories
Every stable **upstream** release is published to signed APT and RPM repositories (the system-Qt packages, x64 and ARM64) and to the AUR. See the [installation guide](https://throneproj.github.io/get_started/installation/#apt-repository).

For a local Ubuntu 24.04 system-Qt `.deb` from this fork: `script/pack_ubuntu24_amd64.sh`.

Debian / Ubuntu (upstream):
```bash
sudo install -d -m 0755 /etc/apt/keyrings
sudo curl -fsSLo /etc/apt/keyrings/throne.asc https://throneproj.github.io/Throne/throne.asc
sudo curl -fsSLo /etc/apt/sources.list.d/throne.list https://throneproj.github.io/Throne/apt/throne.list
sudo apt update && sudo apt install throne
```

Fedora / RHEL 9+:
```bash
sudo curl -fsSLo /etc/yum.repos.d/throne.repo https://throneproj.github.io/Throne/rpm/throne.repo
sudo dnf install throne --refresh
```

openSUSE / SLES:
```bash
sudo zypper addrepo -fc https://throneproj.github.io/Throne/rpm/throne.repo
sudo zypper install throne
```

Arch Linux (AUR): `yay -S throne` or `paru -S throne`

## Supported protocols

- SOCKS
- HTTP(S)
- Shadowsocks
- Trojan
- VMess
- VLESS
- TUIC
- Hysteria
- Hysteria2
- AnyTLS
- Mieru
- Snell
- NaïveProxy
- Juicity
- TrustTunnel
- ShadowTLS
- Wireguard
- AmneziaWG
- MASQUE
- SSH
- DNSTT / VayDNS
- MasterDNS
- Xray VLESS (XHTTP/XDrive)
- OpenVPN/OpenConnect
- Custom Outbound (Both Sing-box and Xray)
- Custom Config (Both Sing-box and Xray)
- Chaining outbounds
- Extra Core

## Subscription Formats

Various formats are supported, including share links, various JSON representation of Sing-box configs, and v2rayN link format as well as limited support for Shadowsocks and Clash formats.

Deeplinks are also supported, read the [documentation](https://throneproj.github.io/advanced/deeplinks/) for more information.

## Credits

- [SagerNet/sing-box](https://github.com/SagerNet/sing-box)
- [XTLS/Xray-core](https://github.com/xtls/xray-core)
- [Qv2ray](https://github.com/Qv2ray/Qv2ray)
- [Qt](https://www.qt.io/)
- [simple-protobuf](https://github.com/tonda-kriz/simple-protobuf)
- [fkYAML](https://github.com/fktn-k/fkYAML)
- [quirc](https://github.com/dlbeer/quirc)
- [srombauts/sqlitecpp](https://github.com/srombauts/sqlitecpp)

## FAQ
**How does this fork relate to Nekoray and upstream Throne?**  
[AL4AL/nekoray](https://github.com/AL4AL/nekoray) is archived. Ongoing desktop work is this Throne fork. Upstream Throne continues independently; we merge from `throneproj/Throne` and keep a few fork-only features (see [docs/GOLDEN.md](docs/GOLDEN.md)).

**Why does my Anti-Virus detect Throne and/or its Core as malware?**  
Throne's built-in update functionallity downloads the new release, removes the old files and replaces them with the new ones, which is quite simliar to what malwares do, remove your files and replace them with an encrypted version of your files.

**Is setting the `SUID` bit really needed on Linux?**  
To create and manage a system TUN interface, root access is required, without it, you will have to grant the Core some `Cap_xxx_admin` and still, need to enter your password 3 to 4 times per TUN activation. You can also opt to disable the automatic privilege escalation in `Basic Settings`->`Security`, but note that features that require root access will stop working unless you manually grant the needed permissions.

**Why does my internet stop working after I force quit Throne?**  
If Throne is force-quit while `System proxy` is enabled, the process ends immediately and Throne cannot reset the proxy.  
Solution:
- Always close Throne normally.
- If you force quit by accident, open Throne again, enable `System proxy`, then disable it- this will reset the settings.

**Where are the downloadable route profiles/rulesets coming from?**  
They are located at the [routeprofiles](https://github.com/throneproj/routeprofiles) repository.

**How does "Throne-\<version\>-debian-system-qt-x64.deb" differ from "Throne-\<version\>-debian-x64.deb" and why is the latter 3 times heavier then the former?**  
The first one does not pack the Qt libraries and relies on those installed on the host. The second one packs everything needed with itself, thus being heavier. The reason the first one exists is that on legacy systems provided Qt libraries use unsupported system features. If a graphical interface fails to load for your system, you may try to download the system-qt version and install fitting Qt libraries from your package manager or compile them from source.

## Supporting upstream Throne

If upstream Throne is useful to you, you can support its development through [GitHub Sponsors](https://github.com/sponsors/Mahdi-zarei) or with a crypto donation to one of these addresses:

| Coin | Network | Address |
|:--|:--|:--|
| **USDT** · **TRX** | TRON (TRC20) | `THR2gbWMaTZh8ACk3VYAAyNRVffvdTRikj` |
| **USDT** · **USDC** | BNB Smart Chain (BEP20) · Ethereum (ERC20) | `0x7c2dF1867f5AaEE807a835fB3c55E4de4507888A` |
| **XMR** | Monero | `4AYwwN8qMQCSUb3XfwzfrLNPMHfDme6U7CmNtQY4idCF8GtHHLG68VA3iZK867ypuKFr8J7CjXiJ96eFUA2T6aLVRUbgY3A` |
