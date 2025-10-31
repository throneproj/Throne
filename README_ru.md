:uk: [English](README.md)

:cn: [中文](README_zh.md)

# Throne (бывш. Nekoray)
Кросс-платформенный компьютерный прокси-клиент с графическим интерфейсом на базе Qt, преемник [Sing-box](https://github.com/SagerNet/sing-box).


Поддерживает Windows 11/10/8/7 / Linux / MacOS "из коробки".

<img width="1199" height="941" alt="image" src="https://github.com/user-attachments/assets/dcb7c23a-6c81-4955-88db-3fb75df69f3b" />

### Примечание для MacOS
Экосистема Apple имеет строгую политику безопасности, и так как Throne не имеет подписанного сертификата, вам нужно избавиться от карантина, используя `xattr -d com.apple.quarantine /path/to/throne.app`. Также, чтобы встроенная функция повышения полномочий работала, у `Terminal` должен быть доступ к `Full Disk`.

### GitHub установки (Portable ZIP)

[![GitHub All Releases](https://img.shields.io/github/downloads/Mahdi-zarei/nekoray/total?label=downloads-total&logo=github&style=flat-square)](https://github.com/throneproj/Throne/releases)

### AUR пакеты
- [source](https://aur.archlinux.org/packages/throne)
- [git](https://aur.archlinux.org/packages/throne-git)
- [bin](https://aur.archlinux.org/packages/throne-bin)

### RPM репозиторий
[Репозиторий Throne RPM](https://parhelia512.github.io/) для Fedora/RHEL и openSUSE/SLE.

## Поддерживаемые протоколы

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
- Wireguard
- SSH
- Custom Outbound
- Custom Config
- Chaining outbounds
- Extra Core

## Форматы подписок (Subscription)

Полностью поддерживаются различные форматы, включая share links, JSON array of outbounds и v2rayN link, и в ограниченном режиме поддерживаются Shadowsocks и Clash.

## Отдельная благодарность

- [SagerNet/sing-box](https://github.com/SagerNet/sing-box)
- [Qv2ray](https://github.com/Qv2ray/Qv2ray)
- [Qt](https://www.qt.io/)
- [simple-protobuf](https://github.com/tonda-kriz/simple-protobuf)
- [fkYAML](https://github.com/fktn-k/fkYAML)
- [quirc](https://github.com/dlbeer/quirc)
- [QHotkey](https://github.com/Skycoder42/QHotkey)

## FAQ
**Как этот проект отличается от оригинального Nekoray?** <br/>
Разработчик Nekoray частично забросил проект ещё в декабре 2023, и хоть некоторые обновления ещё выпускались, сейчас проект официально заархивирован. Throne создан чтобы продолжить путь оригинального проекта, улучшая существующее, добавляя новое и удаляя пережитки.

**Почему мой антивирус детектит Throne / его ядро как вредносное ПО?** <br/>
Встроенная функция автообновления Throne удаляет старые файлы и создаёт на их месте новые, что несколько схоже с вирусами-шифровальщиками, которые заменяют ваши оригинальные файлы на зашифрованные копии. 
Также, функция `Системный DNS` изменяет настройки DNS на вашей системе, что тоже считается опасным действием и является флагом для некоторых антивирусов.

**Нужна ли настройка `SUID` на Linux?** <br/>
Чтобы создавать и управлять системными интерфейсами TUN, Throne необходимы root-права, ведь иначе вам вручную придётся выдавать ядру некоторые важые права `Cap_xxx_admin` и вводить ваш пароль 3-4 раза каждый раз при запуске TUN. Вы можете отключить автоматическую выдачу root-прав в `Основные настройки`->`Безопасность`, но в таком случае функции, требующие прав администратора, не смогут продолжать работать до тех пор, пока вы самостоятельно не выдадите необходимые права.

**Почему мой интернет перестаёт работать после принудительного выключения Throne?** <br/>
Если Throne был выключен принудительно пока включен `Системный прокси`, процесс завершится моментально и Throne не сможет сбросить прокси.

Решение:
- Всегда завершайте Throne нормально.
- Если вы случайно принудительно выключили Throne, откройте его снова, включите `Системный прокси` и выключите его — это сбросит настройки.

**Откуда загружаются профили/наборы правил маршрутизации?**<br/>
Они расположены в репозитории [routeprofiles](https://github.com/throneproj/routeprofiles).
