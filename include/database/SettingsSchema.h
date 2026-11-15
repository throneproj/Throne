#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <QString>
#include <QStringList>

#include "include/database/SettingsValues.h"

namespace Configs::SettingsSchema {
    // Portable values travel with a backup; a restore keeps Host ones from the machine it runs on, and Device ones never leave it.
    enum class Scope : quint8 { Portable, Host, Device };

    // What committing a changed key needs. SettingsService runs the engine bits after the save; the main window runs the Ui bits
    // on EngineEvents::settingsChanged. Theme, fonts and global hotkeys are applied by the window that edits them.
    enum Effect : quint32 {
        NoEffect = 0,
        RestartProfile = 1u << 0,
        RestartRouting = 1u << 1,
        RestartTun = 1u << 2,
        RestartProgram = 1u << 3,
        KillSwitch = 1u << 4,
        Periodic = 1u << 5,
        RemoteApi = 1u << 6,
        AutoRun = 1u << 7,
        UiTray = 1u << 16,
        UiTrayIcon = 1u << 17,
        UiLogLines = 1u << 18,
        UiLogFont = 1u << 19,
        UiLogFilter = 1u << 20,
        UiMode = 1u << 21,
        UiProfileList = 1u << 22,
        UiTrafficStats = 1u << 23,
        UiShortcuts = 1u << 24,
        UiMirrors = 1u << 25,
    };

    // For the members whose type has no generic encoding (the shortcut map, the VLESS preference enum).
    struct Custom {
        std::string (*serialize)(const SettingsValues &values);
        void (*parse)(SettingsValues &values, const QString &raw);
        void (*copy)(const SettingsValues &from, SettingsValues &to);
    };

    using Member = std::variant<bool SettingsValues::*, int SettingsValues::*, qint64 SettingsValues::*, QString SettingsValues::*,
                                QStringList SettingsValues::*, Custom>;

    struct IntRange {
        int min;
        int max;
    };

    struct Key {
        // As stored in the settings table.
        QString name;
        Member member;
        Scope scope = Scope::Portable;
        quint32 effects = NoEffect;
        // The values the API and the settings windows accept; int members only.
        std::optional<IntRange> range;
    };

    // One entry per persisted key, in a fixed order.
    const std::vector<Key> &Keys();
    const Key *Find(const QString &name);

    // The canonical database text: "true"/"false", decimal integers, compact JSON arrays.
    std::string Serialize(const SettingsValues &values, const Key &key);
    // As loadAllSettings always read: "1" also means true, a non-integer reads 0, and a list that is not a JSON array keeps its value.
    void Parse(SettingsValues &values, const Key &key, const QString &raw);

    // Keys whose serialized values differ, in table order.
    QStringList Diff(const SettingsValues &a, const SettingsValues &b);
    // Unknown names are skipped.
    void Copy(const SettingsValues &from, SettingsValues &to, const QStringList &keys);
    quint32 EffectsOf(const QStringList &keys);
    QStringList KeysIn(Scope scope);
} // namespace Configs::SettingsSchema
