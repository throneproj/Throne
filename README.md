# Nekoray

Qt based Desktop cross-platform GUI proxy client, empowered by [Sing-box](https://github.com/SagerNet/sing-box)

Supports Windows / Linux / MacOS out of the box. For Windows 7 / 8 / 8.1, use [nekoray-win7](https://github.com/parhelia512/nekoray-win7)

![image](https://github.com/user-attachments/assets/ebba3f17-01e6-4066-8eaa-b9a35dd35b08)

### Note on MacOS releases
Apple platforms have a very strict security policy and since Nekoray does not have a signed certificate, you will have to remove the quarantine using `xattr -d com.apple.quarantine /path/to/nekoray.app`. Also to get the built-in privilege escalation to work, `Terminal` should have the `Full Disk` access.

### GitHub Releases (Portable ZIP)

[![GitHub All Releases](https://img.shields.io/github/downloads/Mahdi-zarei/nekoray/total?label=downloads-total&logo=github&style=flat-square)](https://github.com/Mahdi-zarei/nekoray/releases)

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
- Wireguard
- SSH
- Custom Outbound
- Custom Config
- Chaining outbounds
- Extra Core

## Subscription Formats

Various formats are supported, including share links, JSON array of outbounds and v2rayN link format as well as limited support for Shadowsocks and Clash formats.

## Credits

- [SagerNet/sing-box](https://github.com/SagerNet/sing-box)
- [Qv2ray](https://github.com/Qv2ray/Qv2ray)
- [Qt](https://www.qt.io/)
- [protobuf](https://github.com/protocolbuffers/protobuf)
- [fkYAML](https://github.com/fktn-k/fkYAML)
- [quirc](https://github.com/dlbeer/quirc)
- [QHotkey](https://github.com/Skycoder42/QHotkey)

## FAQ
**How does this project differ from the original Nekoray?** <br/>
Nekoray's developer partially abandoned the project on Decemeber of 2023, some minor updates were done recently but the project is now officially archived. This project was meant to continue the way of the original project, with lots of improvements, tons of new features and also, removal of obsolete features and simplifications.

**Why does my Anti-Virus detect Nekoray and/or its Core as malware?** <br/>
Nekoray's built-in update functionallity downloads the new release, removes the old files and replaces them with the new ones, which is quite simliar to what malwares do, remove your files and replace them with an encrypted version of your files.
Also the `System DNS` feature will change your system's DNS settings, which is also considered a dangerous action by some Anti-Virus applications.

**Is setting the `SUID` bit really needed on Linux?** <br/>
To create and manage a system TUN interface, root access is required, without it, you will have to grant the Core some `Cap_xxx_admin` and still, need to enter your password 3 to 4 times per TUN activation. You can also opt to disable the automatic privilege escalation in `Basic Settings`->`Security`, but note that features that require root access will stop working unless you manually grant the needed permissions.

**Can the AUR, PKGCONFIG etc be published so that Linux users can install the app using their own ways?** <br/>
The work is ongoing, soon the project will be rebranded under a new name, and then such packages will be released.
