<div align="center">

<img src="res/public/Throne.png" width="128" alt="Throne logo">

# Throne

**Cross-platform Qt desktop proxy client, powered by [sing-box](https://github.com/SagerNet/sing-box)**

[![Release](https://img.shields.io/github/v/release/throneproj/Throne?style=flat-square&label=release&color=2F80ED)](https://github.com/throneproj/Throne/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/throneproj/Throne/total?style=flat-square&logo=github&label=downloads)](https://github.com/throneproj/Throne/releases)
[![Stars](https://img.shields.io/github/stars/throneproj/Throne?style=flat-square&logo=github&label=stars)](https://github.com/throneproj/Throne/stargazers)
[![License](https://img.shields.io/github/license/throneproj/Throne?style=flat-square&color=6E7781)](LICENSE)

</div>

Throne manages proxy profiles and subscriptions, routes traffic with rule-based routing profiles, and works either as a system proxy or as a Tun VPN. It also runs headless, as a service or in a container, controlled with [thronectl](https://github.com/throneproj/Throne-api): see [Headless Throne](https://throneproj.github.io/headless/).

<p align="center">
  <img width="820" alt="Throne main window" src="https://github.com/user-attachments/assets/af4a8e32-7e55-430c-9402-ec2d665cf71a" />
</p>

## Installation

Portable ZIPs and installers for every platform are on [GitHub Releases](https://github.com/throneproj/Throne/releases). The [installation guide](https://throneproj.github.io/get_started/installation/) covers the rest, including the Linux install script, the APT and RPM repositories, the AUR and the other package managers.

### macOS
Apple platforms have a very strict security policy and since Throne does not have a signed certificate, you will have to remove the quarantine using `xattr -d com.apple.quarantine /path/to/throne.app`. Move `Throne.app` to `/Applications` before the first launch — the built-in privilege escalation opens `Terminal` to make the core setuid-root, and that step can fail while the app is still inside `~/Downloads`.

The Homebrew cask (macOS 14.4 or newer, Apple Silicon and Intel) does both for you; update with `brew upgrade --cask throne`.
```bash
brew install --cask throneproj/tap/throne
```

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

## Supporting Throne

If Throne is useful to you, you can support its development through [GitHub Sponsors](https://github.com/sponsors/Mahdi-zarei) or with a crypto donation to one of these addresses:

| Coin | Network | Address |
|:--|:--|:--|
| **USDT** · **TRX** | TRON (TRC20) | `THR2gbWMaTZh8ACk3VYAAyNRVffvdTRikj` |
| **USDT** · **USDC** | BNB Smart Chain (BEP20) · Ethereum (ERC20) | `0x7c2dF1867f5AaEE807a835fB3c55E4de4507888A` |
| **XMR** | Monero | `4AYwwN8qMQCSUb3XfwzfrLNPMHfDme6U7CmNtQY4idCF8GtHHLG68VA3iZK867ypuKFr8J7CjXiJ96eFUA2T6aLVRUbgY3A` |
