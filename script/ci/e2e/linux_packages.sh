#!/usr/bin/env bash
# L1 of the headless e2e suite: the bundled amd64 .deb on the runner VM, run by the system unit it installs
# (dist/linux/throne.service). Covers apt install, Type=notify READY and STATUS, the watchdog, exit 75 restarts by
# systemd (thronectl restart, SIGHUP) that resume the running profile, a clean stop, and apt-get remove / purge.
# Env: DEB (Throne-<tag>-debian-amd64.deb), THRONECTL, FIXTURES, E2E_TMP. Needs root and a running systemd, and
# changes the host, so it is for a disposable runner VM only; started as another user it re-runs itself with sudo -E.
set -euo pipefail

if [ "$(id -u)" != 0 ]; then
    exec sudo -E bash "$(readlink -f "${BASH_SOURCE[0]}")" "$@"
fi

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT=$(cd "$SCRIPT_DIR/../../.." && pwd)
: "${E2E_TMP:=${RUNNER_TEMP:-/tmp}/throne-e2e}"
: "${FIXTURES:=$ROOT/script/ci/smoke_fixtures.py}"
export E2E_TMP FIXTURES
# shellcheck source=lib.sh
source "$SCRIPT_DIR/lib.sh"

DEB=${DEB:?set DEB to the bundled amd64 .deb}
THRONECTL=${THRONECTL:?set THRONECTL to a thronectl built for this runner}

UNIT=throne.service
THRONE=/opt/Throne/Throne
DATA=/var/lib/throne
FILE_LOG=$DATA/config/logs/throne.log
API_ADDR=127.0.0.1:9095
OUT=$E2E_TMP/linux-packages
FIXTURE_HTTP_PORT=18181
FIXTURE_SOCKS_PORT=11181
HTTP_PORT=$FIXTURE_HTTP_PORT
SOCKS_PORT=$FIXTURE_SOCKS_PORT
PROFILE_NAME=e2e-l1-socks
FIXTURE_URL="http://fixture.test:$HTTP_PORT/"
FIXTURE_LOG=$OUT/fixtures.log
CONNECT_RE="^CONNECT fixture\\.test:$HTTP_PORT -> 127\\.0\\.0\\.1:$HTTP_PORT ok\$"
EXIT75_RE='Main process exited, code=exited, status=75(/|$)'
JOURNAL_SINCE=$(($(date +%s) - 1))
PROFILE_ID=
# The output of the last pk_health.
PK_HEALTH=
# Set once the preconditions passed: only from then on does the exit hook stop the unit this run installed.
PK_RUNNING=

# ---------------------------------------------------------------------------------------------------- helpers

pk_prop() { systemctl show "$UNIT" -p "$1" --value; }

pk_show() {
    systemctl show "$UNIT" -p ActiveState,SubState,StatusText,MainPID,NRestarts,Result,ExecMainCode,ExecMainStatus || true
}

pk_eq() { [ "$2" = "$3" ] || fail "$1 is '$2', want '$3'"; }

# pk_json PATH: the value at a dotted PATH of the JSON on stdin (list indexes as numbers); strings raw, the rest as
# JSON, "<missing>" when the path is not there.
pk_json() {
    python3 -I -c '
import json, sys
try:
    value = json.load(sys.stdin)
except ValueError:
    print("<not JSON>")
    sys.exit(0)
for part in filter(None, sys.argv[1].split(".")):
    try:
        value = value[int(part)] if isinstance(value, list) else value[part]
    except (KeyError, IndexError, TypeError, ValueError):
        print("<missing>")
        sys.exit(0)
print(value if isinstance(value, str) else json.dumps(value))
' "$1"
}

pk_journal() { journalctl -u "$UNIT" --since "@$JOURNAL_SINCE" --no-pager -o cat 2>/dev/null; }

pk_journal_count() { pk_journal | grep -cE -- "$1" || true; }

pk_journal_has() { (($(pk_journal_count "$1") >= $2)); }

pk_ready() { [ "$(pk_prop ActiveState)" = active ] && [ "$(pk_prop SubState)" = running ]; }

pk_status_is() { [ "$(pk_prop StatusText)" = "$1" ]; }

pk_watchdog_moved() { [ "$(pk_prop WatchdogTimestampMonotonic)" != "$1" ]; }

# pk_restarted OLD_PID NRESTARTS: systemd started the unit again and it sent READY=1.
pk_restarted() {
    local pid
    pid=$(pk_prop MainPID)
    [ "$(pk_prop NRestarts)" = "$2" ] && pk_ready && [ "$pid" != 0 ] && [ "$pid" != "$1" ]
}

pk_gone() { ! kill -0 "$1" 2>/dev/null; }

pk_no_process() { ! pgrep -x "$1" >/dev/null; }

pk_installed() { [ "$(dpkg-query -W -f='${Status}' throne 2>/dev/null || true)" = "install ok installed" ]; }

pk_connects() { grep -cE "$CONNECT_RE" "$FIXTURE_LOG" 2>/dev/null || true; }

pk_connects_above() { (($(pk_connects) > $1)); }

pk_profile_running() {
    local out
    out=$("$THRONECTL" --json status 2>/dev/null) || return 1
    [ "$(pk_json state <<<"$out")" = running ] && [ "$(pk_json profile.id <<<"$out")" = "$PROFILE_ID" ]
}

pk_health() {
    local out
    out=$("$THRONE" --health --data-dir "$DATA") || fail "Throne --health --data-dir $DATA exited $?: $out"
    PK_HEALTH=$out
    echo "$out"
    pk_eq "--health ok" "$(pk_json ok <<<"$out")" true
    pk_eq "--health ready" "$(pk_json ready <<<"$out")" true
    pk_eq "--health api" "$(pk_json api <<<"$out")" listening
    pk_eq "--health headless_dialogs" "$(pk_json headless_dialogs <<<"$out")" 0
    pk_eq "--health previous_crash" "$(pk_json previous_crash <<<"$out")" false
}

# pk_previous_exit CODE REASON LABEL: the last pk_health names the exit of the boot before this one.
pk_previous_exit() {
    pk_eq "--health previous_exit.code ($3)" "$(pk_json previous_exit.code <<<"$PK_HEALTH")" "$1"
    pk_eq "--health previous_exit.reason ($3)" "$(pk_json previous_exit.reason <<<"$PK_HEALTH")" "$2"
}

# pk_proxy_check LABEL: a request through Throne's mixed inbound reaches the SOCKS5 fixture.
pk_proxy_check() {
    local out address port before code
    out=$("$THRONECTL" --json status) || fail "thronectl status failed ($1)"
    pk_eq "inbound.enabled ($1)" "$(pk_json inbound.enabled <<<"$out")" true
    address=$(pk_json inbound.address <<<"$out")
    port=$(pk_json inbound.port <<<"$out")
    [[ $port =~ ^[0-9]+$ ]] || fail "inbound.port is '$port' ($1)"
    case $address in
        "" | 0.0.0.0 | "<missing>") address=127.0.0.1 ;;
        ::) address='[::1]' ;;
        *:*) address="[$address]" ;;
    esac
    before=$(pk_connects)
    code=$(curl -sS --max-time 20 -x "socks5h://$address:$port" -o /dev/null -w '%{http_code}' "$FIXTURE_URL") ||
        fail "curl through Throne's mixed inbound $address:$port failed ($1)"
    pk_eq "HTTP status through Throne ($1)" "$code" 204
    wait_until 5 "the SOCKS5 fixture logging the proxied CONNECT ($1)" pk_connects_above "$before"
}

# pk_session_clean LABEL: the session that just stopped ended cleanly on SIGTERM (throne.log holds one session).
pk_session_clean() {
    local copy="$OUT/throne-$1.log"
    [ -f "$FILE_LOG" ] || fail "no $FILE_LOG after $1"
    cp "$FILE_LOG" "$copy"
    grep -qF 'EXIT 0: SIGTERM' "$copy" || fail "$FILE_LOG does not log 'EXIT 0: SIGTERM' after $1"
    grep -qF '===== Throne session end (clean) =====' "$copy" || fail "$FILE_LOG has no clean session end after $1"
    # KillMode=mixed SIGKILLs a leftover core anyway, so only Throne's own log shows that it reaped its core.
    grep -qE 'core process exited: code=-?[0-9]+ status=(normal|crash) \(during shutdown\)' "$copy" ||
        fail "$FILE_LOG has no 'core process exited ... (during shutdown)' line after $1: Throne did not stop and reap its core"
    [ ! -e "$DATA/config/logs/running.marker" ] || fail "running.marker was left behind after $1"
    assert_log_clean "$copy"
}

pk_api_restart() {
    local out
    out=$("$THRONECTL" --json restart) || fail "thronectl restart failed"
    echo "$out"
    pk_eq "restart response resume" "$(pk_json resume <<<"$out")" true
}

pk_hangup() {
    local pid
    pid=$(pk_prop MainPID)
    ((pid > 0)) || fail "the unit has no MainPID to send SIGHUP to"
    kill -HUP "$pid"
}

# pk_restart_cycle LABEL EXIT_LINE COMMAND...: COMMAND makes Throne exit 75, systemd starts it again (NRestarts + 1,
# a new MainPID, READY=1), and the running profile resumes and proxies again.
pk_restart_cycle() {
    local label=$1 exit_line=$2
    shift 2
    local pid restarts exits lines resumes resume_re
    resume_re="\\[Resume\\] Resuming profile $PROFILE_ID "
    pid=$(pk_prop MainPID)
    restarts=$(pk_prop NRestarts)
    exits=$(pk_journal_count "$EXIT75_RE")
    lines=$(pk_journal_count "$exit_line")
    resumes=$(pk_journal_count "$resume_re")
    "$@"
    wait_until 90 "systemd starting the unit again after $label (NRestarts $((restarts + 1)), a MainPID other than $pid, READY=1)" \
        pk_restarted "$pid" "$((restarts + 1))"
    pk_show
    wait_until 15 "the journal recording the exit with status 75 after $label" pk_journal_has "$EXIT75_RE" "$((exits + 1))"
    wait_until 15 "the journal logging '$exit_line'" pk_journal_has "$exit_line" "$((lines + 1))"
    wait_until 60 "profile $PROFILE_ID running again after $label" pk_profile_running
    wait_until 15 "StatusText 'Connected $PROFILE_NAME' after $label" pk_status_is "Connected $PROFILE_NAME"
    pk_journal_has "$resume_re" "$((resumes + 1))" || fail "no new '[Resume] Resuming profile $PROFILE_ID' line after $label"
    pk_health
    pk_previous_exit 75 "${exit_line#EXIT 75: }" "after $label"
    pk_proxy_check "after $label"
}

# Called by lib.sh's EXIT trap, which then stops the fixtures.
e2e_on_exit() {
    local rc=$1 file
    [ -n "$PK_RUNNING" ] || return 0
    mkdir -p "$OUT"
    journalctl -u "$UNIT" --since "@$JOURNAL_SINCE" --no-pager -o short-monotonic >"$OUT/journal.txt" 2>&1
    [ -f "$FILE_LOG" ] && cp "$FILE_LOG" "$OUT/throne-last.log"
    if ((rc != 0)); then
        echo "::group::FAILED (status $rc): unit, journal, throne.log, fixtures"
        pk_show
        systemctl status "$UNIT" --no-pager -l
        cat "$OUT/journal.txt"
        if [ -f "$FILE_LOG" ]; then
            echo "--- tail of $FILE_LOG"
            tail -n 200 "$FILE_LOG"
        fi
        for file in "$FIXTURE_LOG" "$FIXTURE_LOG.stderr"; do
            [ -f "$file" ] && echo "--- $file" && cat "$file"
        done
        echo "::endgroup::"
    fi
    if pk_installed; then
        timeout 60 systemctl disable --now "$UNIT" >/dev/null 2>&1
    fi
    pkill -KILL -x ThroneCore 2>/dev/null
    chown -R "${SUDO_UID:-0}:${SUDO_GID:-0}" "$OUT" 2>/dev/null
    chmod -R a+rX "$OUT" 2>/dev/null
    return 0
}

# ---------------------------------------------------------------------------------------------------- scenario

main() {
    local deb out t0 elapsed mode first pid
    [ -d /run/systemd/system ] || fail "systemd is not running here; L1 needs the runner VM, not a container"
    [ -f "$DEB" ] || fail "DEB '$DEB' is not a file"
    [ -x "$THRONECTL" ] || fail "THRONECTL '$THRONECTL' is not executable"
    [ -f "$FIXTURES" ] || fail "FIXTURES '$FIXTURES' is not a file"
    deb=$(readlink -f "$DEB")
    rm -rf "$OUT"
    mkdir -p "$OUT"
    if pk_installed || [ -e /usr/lib/systemd/system/throne.service ] || [ -e "$DATA" ]; then
        fail "this host already has Throne installed or a $DATA; L1 needs a clean runner"
    fi
    PK_RUNNING=1

    step "L1: apt-get install $(basename "$deb")"
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -qq
    apt-get install -y --no-install-recommends "$deb"
    pk_installed || fail "dpkg does not list throne as installed after apt-get install"
    [ -x "$THRONE" ] || fail "no $THRONE after the install"
    # A sanity check only: this runner image has most libraries anyway; package-smoke checks Depends in a bare container.
    out=$(ldd "$THRONE" /opt/Throne/ThroneCore 2>&1) || true
    if grep -F 'not found' <<<"$out"; then
        fail "the installed Throne does not load on this runner (lines above)"
    fi
    "$THRONE" --headless --version
    cmp "$ROOT/dist/linux/throne.service" /usr/lib/systemd/system/throne.service ||
        fail "the installed unit differs from dist/linux/throne.service"
    [ -f /etc/throne/throne.env ] || fail "no /etc/throne/throne.env after the install"
    dpkg-query -W -f='${Conffiles}\n' throne | grep -qF /etc/throne/throne.env ||
        fail "/etc/throne/throne.env is not a conffile of the package"

    step "L1: the unit is installed and disabled"
    pk_show
    pk_eq LoadState "$(pk_prop LoadState)" loaded
    pk_eq FragmentPath "$(pk_prop FragmentPath)" /usr/lib/systemd/system/throne.service
    pk_eq UnitFileState "$(pk_prop UnitFileState)" disabled
    pk_eq ActiveState "$(pk_prop ActiveState)" inactive
    pk_no_process Throne || fail "a Throne process runs although the package must not start the unit"

    step "L1: systemctl enable --now throne (READY within 90 s)"
    t0=$SECONDS
    timeout 150 systemctl enable --now "$UNIT" ||
        fail "systemctl enable --now $UNIT failed: Throne exited, or sent no READY=1"
    elapsed=$((SECONDS - t0))
    pk_show
    pk_ready || fail "the unit is not active/running after systemctl enable --now returned"
    ((elapsed <= 90)) || fail "READY=1 took $elapsed s, more than 90 s"
    echo "READY after $elapsed s"
    pk_eq UnitFileState "$(pk_prop UnitFileState)" enabled
    wait_until 15 "StatusText 'Idle'" pk_status_is Idle
    pk_eq NRestarts "$(pk_prop NRestarts)" 0
    # The unit's StateDirectoryMode=0700 and UMask=0077 decide these; L2 and L3 check Throne's own create modes.
    mode=$(stat -c '%a %U' "$DATA")
    pk_eq "$DATA mode and owner (the unit's StateDirectoryMode)" "$mode" "700 root"
    mode=$(stat -c '%a' "$DATA/config/api.key") || fail "no $DATA/config/api.key"
    (((8#$mode & 8#077) == 0)) || fail "$DATA/config/api.key has mode $mode: others can read the API key despite the unit's UMask=0077"
    wait_until 10 "the journal showing the boot line with 'restart mode exit (systemd notify socket)'" \
        pk_journal_has 'restart mode exit \(systemd notify socket\)' 1

    step "L1: watchdog pings (WatchdogSec=60)"
    pk_eq WatchdogUSec "$(pk_prop WatchdogUSec)" 1min
    wait_until 10 "the journal showing '[systemd] Watchdog of 60000 ms; pinging every 20000 ms.'" \
        pk_journal_has '\[systemd\] Watchdog of 60000 ms; pinging every 20000 ms\.' 1
    first=$(pk_prop WatchdogTimestampMonotonic)
    wait_until 30 "a WATCHDOG=1 ping (WatchdogTimestampMonotonic leaving $first)" pk_watchdog_moved "$first"

    step "L1: Throne --health as root"
    pk_health
    pk_eq "--health previous_exit (the first boot on a fresh data dir)" "$(pk_json previous_exit <<<"$PK_HEALTH")" null

    step "L1: --print-connection and thronectl status"
    connect_env "$THRONE" "$DATA"
    pk_eq THRONE_ADDR "${THRONE_ADDR:-}" "$API_ADDR"
    pk_eq THRONE_KEY_FILE "${THRONE_KEY_FILE:-}" "$DATA/config/api.key"
    out=$("$THRONECTL" --json status) || fail "thronectl status failed"
    echo "$out"
    pk_eq capabilities.headless "$(pk_json capabilities.headless <<<"$out")" true
    pk_eq state "$(pk_json state <<<"$out")" idle
    "$THRONECTL" status

    step "L1: fixtures (HTTP $HTTP_PORT, SOCKS5 $SOCKS_PORT)"
    start_fixtures 127.0.0.1 "$FIXTURE_LOG"
    out=$(curl -sS --max-time 10 -x "socks5h://127.0.0.1:$SOCKS_PORT" -o /dev/null -w '%{http_code}' "$FIXTURE_URL") ||
        fail "the fixtures do not answer each other"
    pk_eq "fixture self-test HTTP status" "$out" 204
    (($(pk_connects) > 0)) || fail "the SOCKS5 fixture did not log its own CONNECT"

    step "L1: import a SOCKS profile, connect, proxy through the mixed inbound"
    out=$(printf 'socks5://127.0.0.1:%s#%s\n' "$SOCKS_PORT" "$PROFILE_NAME" | "$THRONECTL" --json import -) ||
        fail "thronectl import failed"
    echo "$out"
    pk_eq imported "$(pk_json imported <<<"$out")" 1
    PROFILE_ID=$(pk_json profile_ids.0 <<<"$out")
    [[ $PROFILE_ID =~ ^[0-9]+$ ]] || fail "profile_ids.0 is '$PROFILE_ID'"
    "$THRONECTL" --timeout 90s connect "$PROFILE_ID"
    wait_until 30 "profile $PROFILE_ID running" pk_profile_running
    wait_until 15 "StatusText 'Connected $PROFILE_NAME'" pk_status_is "Connected $PROFILE_NAME"
    pk_proxy_check "after connect"

    step "L1: thronectl restart: exit 75, restarted by systemd, profile resumed"
    pk_restart_cycle "thronectl restart" 'EXIT 75: restart requested' pk_api_restart

    step "L1: SIGHUP to MainPID: exit 75, restarted by systemd, profile resumed"
    pk_restart_cycle "SIGHUP" 'EXIT 75: SIGHUP' pk_hangup
    pk_journal_has 'Watchdog timeout' 1 && fail "systemd's watchdog fired (journal above)"
    pk_journal_has 'did not shut down cleanly' 1 && fail "a restart found an unclean previous session"

    step "L1: systemctl stop: exit 0, Result=success, Throne reaped its core"
    pid=$(pk_prop MainPID)
    timeout 90 systemctl stop "$UNIT" || fail "systemctl stop $UNIT failed"
    pk_show
    pk_eq ActiveState "$(pk_prop ActiveState)" inactive
    pk_eq Result "$(pk_prop Result)" success
    pk_eq "ExecMainCode (1 = exited)" "$(pk_prop ExecMainCode)" 1
    pk_eq ExecMainStatus "$(pk_prop ExecMainStatus)" 0
    wait_until 10 "Throne (PID $pid) gone" pk_gone "$pid"
    # Sanity only: the cgroup kill removes a leftover core too; pk_session_clean checks that Throne reaped it.
    wait_until 10 "no ThroneCore process left" pk_no_process ThroneCore
    pk_session_clean systemctl-stop
    pk_journal >"$OUT/journal-messages.txt"
    assert_log_clean "$OUT/journal-messages.txt"

    step "L1: apt-get remove stops and disables the running unit"
    timeout 150 systemctl start "$UNIT" || fail "systemctl start $UNIT failed"
    pk_ready || fail "the unit is not active/running after systemctl start returned"
    wait_until 60 "profile $PROFILE_ID resumed before the remove" pk_profile_running
    pk_health
    pk_previous_exit 0 SIGTERM "the boot after systemctl stop"
    pid=$(pk_prop MainPID)
    apt-get remove -y throne
    wait_until 10 "Throne (PID $pid) gone after apt-get remove" pk_gone "$pid"
    pk_no_process Throne || fail "a Throne process survived apt-get remove"
    wait_until 10 "no ThroneCore process left after apt-get remove" pk_no_process ThroneCore
    [ "$(systemctl is-active "$UNIT" 2>/dev/null || true)" != active ] || fail "the unit is still active after apt-get remove"
    [ ! -e /etc/systemd/system/multi-user.target.wants/throne.service ] &&
        [ ! -L /etc/systemd/system/multi-user.target.wants/throne.service ] ||
        fail "apt-get remove left the unit enabled (multi-user.target.wants/throne.service)"
    [ ! -e /usr/lib/systemd/system/throne.service ] || fail "apt-get remove left the unit file"
    [ ! -e "$THRONE" ] || fail "apt-get remove left $THRONE"
    pk_eq "package state after remove" "$(dpkg-query -W -f='${Status}' throne 2>/dev/null || true)" "deinstall ok config-files"
    [ -f /etc/throne/throne.env ] || fail "apt-get remove deleted the conffile /etc/throne/throne.env; only purge may"
    [ -d "$DATA" ] || fail "apt-get remove deleted $DATA, which the headless docs say stays"
    pk_session_clean apt-get-remove

    step "L1: apt-get purge leaves no /etc/throne"
    apt-get purge -y throne
    if [ -e /etc/throne ]; then
        ls -la /etc/throne
        fail "apt-get purge left /etc/throne"
    fi
    case "$(dpkg-query -W -f='${Status}' throne 2>/dev/null || true)" in
        "" | *not-installed) ;;
        *) fail "dpkg still knows throne after purge: $(dpkg-query -W -f='${Status}' throne)" ;;
    esac
    [ ! -e /usr/share/applications/Throne.desktop ] || fail "apt-get purge left /usr/share/applications/Throne.desktop (postrm removes it)"
    echo "L1 passed"
}

main "$@"
