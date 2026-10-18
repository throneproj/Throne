#pragma once

#include <optional>

#include <QByteArray>
#include <QHostAddress>
#include <QList>
#include <QPair>
#include <QString>

#include "include/api/remote/Server.hpp"

namespace RemoteApi {
    enum class ConfigSource { Settings, Flags, Environment, Default };

    // Headless flags/env over the settings; never saved. The GUI never installs one.
    struct Overlay {
        // The default listener applies when the settings leave the API off.
        bool headless = false;
        // false: "off".
        std::optional<bool> enabled;
        // Null: unset.
        QHostAddress address;
        std::optional<quint16> port;
        // Absolute; empty: unset.
        QString keyFile;
        std::optional<bool> adminLan;
        // remote_api_allow syntax.
        std::optional<QString> allow;
        ConfigSource listenSource = ConfigSource::Default;
        ConfigSource keyFileSource = ConfigSource::Default;
        // Filled by HeadlessApi::CheckExplicitKeyFile (an explicit file, read once) or ResolveKeys.
        QByteArray key;
        // <config>/api.key.
        QString defaultKeyFile;
    };

    // "off"|"none"; "PORT"; "IPv4:PORT"; "[IPv6]:PORT"; "*:PORT" / ":PORT" (every address). No host names.
    bool ParseListen(const QString &text, Overlay *overlay, QString *error);

    // The remote_api_allow parser ConfigFromSettings uses.
    QList<QPair<QHostAddress, int>> ParseAllowList(const QString &text);

    struct KeyFile {
        QByteArray key;
        QString error;
        bool created = false;
        // createIfMissing found our own default file holding no key at all (empty, or only NULs/whitespace: an interrupted first
        // start) and replaced it with a new key; created is set too.
        bool replaced = false;
    };

    // At most 4 KiB of a regular file (a FIFO is refused, never waited on); a BOM picks UTF-8/16/32, then KeyFromText.
    // createIfMissing (our own <config>/api.key): created owner-only under a temporary name (Unix O_EXCL 0600; elsewhere NewOnly
    // + 0600), synced to disk, then linked into place without replacing; never QSaveFile. Such a file that holds no key at all
    // (empty, NULs, whitespace), regular, ours and not reached through a link, is replaced; anything else stays an error.
    // Unix: only that default file, ours and not reached through a link, is narrowed to 0600 (fchmod, after the key validated);
    // any other file open to other users only gets a warning.
    KeyFile LoadKeyFile(const QString &path, bool createIfMissing);

    // Once, main thread, before EngineHost::init().
    void SetOverlay(const Overlay &overlay);

    const Overlay &CurrentOverlay();

    struct Effective {
        Config config;
        ConfigSource source = ConfigSource::Settings;
        // Flags/env named the listener.
        bool explicitListen = false;
        // Empty when the key is remote_api_key.
        QString keyFile;
    };

    // Main thread. Without an overlay (the GUI): exactly ConfigFromSettings().
    Effective EffectiveConfig();
} // namespace RemoteApi
