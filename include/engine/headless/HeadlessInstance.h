#pragma once

#include <functional>

#include <QByteArray>
#include <QDir>
#include <QJsonObject>
#include <QString>

#include "include/api/remote/Overlay.hpp"

namespace Engine {
    class InstanceServer;
}

// The instance socket of a headless process and its two client modes.
namespace Engine::HeadlessInstance {
    // Unix: <config>/.instance, or FallbackSocketName(wd, create) when that path encodes to >= 100 bytes. Windows: the pipe
    // "throne-headless-<md5hex(wd)>". Never the GUI's throne-<md5>. "" when the fallback fails its checks (logged).
    QString SocketName(const QDir &wd, bool create = false);

    // Unix: <private dir>/headless-<md5hex(wd)>; the private dir is /run/throne (root, Linux), /var/run/throne (root, macOS) or
    // /tmp/throne-<euid>, mode 0700 and owned by us (checked). The listener also uses it when <config> cannot hold a socket
    // (FAT, exFAT, sshfs). create: make the dir, for the listener only; clients never create anything. "" on Windows, or when
    // the dir fails its checks (logged only with create).
    QString FallbackSocketName(const QDir &wd, bool create = false);

    // The HEALTH/HELLO/REJECT protocol on server; a 5 s single-shot per socket aborts idle clients; no Raise. health runs on main.
    // Windows under a service or LocalSystem: the pipe keeps the default named-pipe ACL (only SYSTEM and Administrators may
    // open it read-write or add instances), and only SYSTEM and Administrators get answers.
    void Install(InstanceServer &server, std::function<QJsonObject()> health);

    struct Reply {
        bool answered = false;
        QByteArray line;
    };

    // Blocking QLocalSocket, no event loop, total budget timeoutMs; sends request + '\n', reads to '\n' or EOF. Unix: talks only
    // to a socket owned by us, root or the data dir's owner. Windows: refuses a pipe
    // whose server Sys::ServiceManager::PipeServerTrusted rejects (named-pipe spoofing); a pipe that denies this account (a
    // service's pipe, to a non-admin) counts as no answer, with a warning.
    Reply Ask(const QString &socketName, const QByteArray &request, int timeoutMs);

    // Ask() on SocketName(wd), then on FallbackSocketName(wd) when the first did not answer.
    Reply AskInstance(const QDir &wd, const QByteArray &request, int timeoutMs);

    // Over AskInstance. > 0: a headless owner's pid; 0: something else answered; -1: nothing answered.
    qint64 Hello(const QDir &wd, const QString &client, int timeoutMs);

    // stdout: one compact line {ok, ready, state, degraded, stuck_s, core_restarts, headless_dialogs, previous_crash,
    // previous_exit {code, reason, at} | null, api}
    // (api: the HEALTH api block's state).
    // 0 ok / 1 not ok or starting / 2 no socket, no answer within 3 s, or unparsable.
    int RunHealth(const QDir &wd);

    // stdout: "export THRONE_ADDR=<host:port>" and "export THRONE_KEY_FILE=<path>", single-quoted only when needed. From HEALTH's
    // api block; with no instance, from the overlay/defaults and <config>/api.key if it exists (with a note on stderr). 0; 2 when
    // the API is off, not listening (failed, key_invalid) or its key is not in a file. Never prints the key.
    int RunPrintConnection(const QDir &wd, const RemoteApi::Overlay &overlay);
} // namespace Engine::HeadlessInstance
