# Nekoray-new

基于 Qt 的跨平台的桌面 GUI 代理客户端，授权自 [Sing-box](https://github.com/SagerNet/sing-box)

开箱支持 Windows / Linux / MacOS 。对于 Windows 7 / 8 / 8.1，使用 [nekoray-win7](https://github.com/parhelia512/nekoray-win7)

![image](https://github.com/user-attachments/assets/ebba3f17-01e6-4066-8eaa-b9a35dd35b08)

### MacOS 发行版说明
苹果的平台具有非常严格的安全策略，由于 Nekoray 没有签名证书，所以必须使用 `xattr -d com.apple.quarantine /path/to/nekoray.app` 去掉隔离。 此外，为了使内置的提权起效，“终端”应该具有 “Full Disk” 访问权限。 

### GitHub 发行版 (Portable ZIP)

[![GitHub All Releases](https://img.shields.io/github/downloads/Mahdi-zarei/nekoray/total?label=downloads-total&logo=github&style=flat-square)](https://github.com/Mahdi-zarei/nekoray/releases)

## 支持的协议

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

## 订阅格式

支持各种格式，包括分享链接，出站的 JSON 数组和 v2rayN 链接格式，以及对 Shadowsocks 和 Clash 格式的有限支持。

## 致谢

- [SagerNet/sing-box](https://github.com/SagerNet/sing-box)
- [Qv2ray](https://github.com/Qv2ray/Qv2ray)
- [Qt](https://www.qt.io/)
- [protobuf](https://github.com/protocolbuffers/protobuf)
- [fkYAML](https://github.com/fktn-k/fkYAML)
- [quirc](https://github.com/dlbeer/quirc)
- [QHotkey](https://github.com/Skycoder42/QHotkey)

## FAQ
**这个项目与原始的 Nekoray 有什么不同？** <br/>
[Nekoray](https://github.com/MatsuriDayo/nekoray/releases) 的原始开发者 MatsuriDayo 从 2023 年 12 月开始开始部分放弃项目，只进行一些小更新，而现在该项目以正式归档。
[Mahdi-zarei 的 nekoray](https://github.com/Mahdi-zarei/nekoray) 旨在继承原始计划，带有大量的改进，增加大量新特性，还去掉了过时的功能并进行了简化。

**为何某些防毒软件检测到 Nekoray 或其 核心 为恶意软件?** <br/>
Nekoray 的内置更新功能下载新发行版，移除旧文件并使用新的文件进行替代，这些相当类似于某些恶意软件所作所为（删除你的文件，使用加密文件替换你的文件）。
还有 `System DNS` 功能将更改你的系统的 DNS 设置，也会被某些防毒软件视为危险的行为。

**在 Linux 上是否真的需要设置 `SUID` 比特?** <br/>
为了创建和管理系统 TUN 接口，需要 root 访问权，没有它，你就必须赋予 Core 一些 `Cap_xxx_admin` 而且每次 TUN 激活时还需要你输入密码 3 到 4 次。
您还可以选择在“基本设置”->“安全”中禁用自动权限升级，但请注意，需要 root 访问权的功能将停止工作，除非您手动授予所需的权限。

**AUR, PKGCONFIG 等是否可以发布，以便 Linux 用户可以使用它们自己的方式安装该应用?** <br/>
这项工作正在进行中，很快该项目将以新的名称重命名，然后发布这类软件包。
