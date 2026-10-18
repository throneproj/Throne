#!/usr/bin/env bash
# M1 of the headless e2e suite, on a macOS runner. "daemon": dist/macos/moe.Throne.headless.plist with its paths pointed at
# the unpacked app and a temp data dir, bootstrapped into the system domain (root): --check, --health, thronectl proxy
# through the mixed inbound, SIGHUP (exit 75, launchd starts it again, the profile resumes), bootout. "foreground": the
# runner user's shell: SIGTERM, SIGHUP exec restart in the same PID, --restart-mode exit + SIGHUP = 75, and a second
# SIGINT during the teardown of a connected profile whose core is frozen.
# Never Tun or the kill switch.
# Env: THRONE_APP  Throne.app, or the dir holding Throne and ThroneCore, or build-cpp's deployment/darwin-arm64
#                  (default: THRONE_DIR, then <repo>/deployment/darwin-arm64/Throne.app)
#      THRONECTL   thronectl built for this runner (default: thronectl on PATH)
#      FIXTURES    script/ci/smoke_fixtures.py (default: the repo's); E2E_TMP scratch dir (default $RUNNER_TEMP/throne-e2e)
# Needs passwordless sudo. Installs and removes /Library/LaunchDaemons/moe.Throne.headless.plist: disposable runners only.
set -euo pipefail

M1_SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)
M1_ROOT=$(cd "$M1_SCRIPT_DIR/../../.." && pwd -P)
: "${E2E_TMP:=${RUNNER_TEMP:-/tmp}/throne-e2e}"
: "${FIXTURES:=$M1_ROOT/script/ci/smoke_fixtures.py}"
: "${THRONECTL:=thronectl}"
if [ -z "${THRONE_DIR:-}" ] && [ -n "${THRONE_APP:-}" ]; then
    THRONE_DIR=$THRONE_APP
    for M1_CANDIDATE in "$THRONE_APP/Contents/MacOS" "$THRONE_APP/Throne.app/Contents/MacOS"; do
        if [ -d "$M1_CANDIDATE" ]; then THRONE_DIR=$M1_CANDIDATE; fi
    done
fi
: "${THRONE_DIR:=$M1_ROOT/deployment/darwin-arm64/Throne.app/Contents/MacOS}"
export E2E_TMP FIXTURES THRONECTL THRONE_DIR
# shellcheck source=lib.sh
source "$M1_SCRIPT_DIR/lib.sh"
set -euo pipefail

M1_LABEL=moe.Throne.headless
M1_PLIST_SRC="$M1_ROOT/dist/macos/$M1_LABEL.plist"
M1_PLIST_DST="/Library/LaunchDaemons/$M1_LABEL.plist"
# The plist passes no --api-listen: the headless default.
M1_DAEMON_API=127.0.0.1:9095
M1_HTTP_PORT=18680
M1_SOCKS_PORT=18681
# The foreground part's mixed inbound, off the daemon part's default 2080.
M1_FG_IN_PORT=${M1_FG_IN_PORT:-12080}
M1_FG_API=127.0.0.1:19396
M1_PROFILE_NAME=e2e-m1-socks
M1_FIXTURE_URL="http://fixture.test:$M1_HTTP_PORT/"
M1_CONNECT_RE="^CONNECT fixture\\.test:$M1_HTTP_PORT -> 127\\.0\\.0\\.1:$M1_HTTP_PORT ok\$"
M1_SESSION_START='===== Throne session start ====='
M1_SESSION_END='===== Throne session end (clean) ====='

# A script's background job starts with SIGINT ignored, which Throne keeps (as for nohup); a terminal's foreground job does not.
M1_EXEC_DEFAULT_SIGNALS='import os, signal, sys
for signum in (signal.SIGINT, signal.SIGHUP):
    signal.signal(signum, signal.SIG_DFL)
os.execv(sys.argv[1], sys.argv[1:])'

THRONE=
M1_BIN_DIR=
M1_WORK=
M1_DWORK=
M1_DATA=
M1_LAUNCHD_LOG=
M1_PLIST_TMP=
M1_KEY_COPY=
M1_FIXTURE_LOG=
M1_FIXTURE_PID=
M1_BOOTSTRAPPED=
M1_FWORK=
M1_FG_DATA=
M1_FG_LOG=
M1_FG_PID=
M1_IN_ADDR=
M1_IN_PORT=
M1_FAILED=

# ---------------------------------------------------------------------------------------------------- helpers

# m1_json PATH: the value at a dotted PATH of the JSON on stdin (list indexes as numbers); strings raw, the rest as JSON,
# "<missing>" when the path is not there.
m1_json() {
    python3 -I -c '
import json, sys
try:
    value = json.load(sys.stdin)
except ValueError:
    print("<not JSON>")
    sys.exit(0)
try:
    for part in filter(None, sys.argv[1].split(".")):
        value = value[int(part)] if isinstance(value, list) else value[part]
except (KeyError, IndexError, TypeError, ValueError):
    print("<missing>")
    sys.exit(0)
print(value if isinstance(value, str) else json.dumps(value))
' "$1"
}

m1_expect() { [ "$2" = "$3" ] || fail "$1 is '$2', want '$3'"; }

m1_summary() {
    if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then printf -- '- %s\n' "$1" >>"$GITHUB_STEP_SUMMARY"; fi
}

m1_show() {
    [ -e "$1" ] || return 0
    echo "::group::$1"
    tail -n "${2:-200}" "$1"
    echo "::endgroup::"
}

m1_show_root() {
    sudo -n test -e "$1" || return 0
    echo "::group::$1"
    sudo -n tail -n "${2:-200}" "$1"
    echo "::endgroup::"
}

m1_copy_root() { sudo -n cat "$1" >"$2" || fail "cannot read $1 as root"; }

# The last "EXIT <code>: <reason>" line of FILE must end with WANT.
m1_expect_exit_line() {
    local last
    last=$(grep -E '[|] EXIT [0-9]+: ' "$1" | tail -n 1 || true)
    [[ $last == *"| $2" ]] || fail "the last EXIT line of $1 is '${last:-none}', want one ending in '| $2'"
}

m1_count() { grep -c "$@" 2>/dev/null || true; }

m1_no_core() { ! pgrep -x ThroneCore >/dev/null; }

m1_pid_gone() { ! ps -p "$1" >/dev/null 2>&1; }

# Qt puts the core's IPC socket in TMPDIR, else in the user's Darwin temp dir (getconf DARWIN_USER_TEMP_DIR).
m1_assert_no_ipc() {
    local dir found=
    for dir in /private/tmp "$@"; do
        [ -n "$dir" ] || continue
        found=$(sudo -n find "$dir" -maxdepth 1 -name 'throneIPC-*' 2>/dev/null || true)
        if [ -n "$found" ]; then
            printf '%s\n' "$found" >&2
            fail "core IPC sockets are left in $dir (above)"
        fi
    done
}

m1_curl_via() {
    curl -sS --max-time 20 -x "$1" -o /dev/null -w '%{http_code}' "$M1_FIXTURE_URL"
}

m1_connects() {
    local n
    n=$(m1_count -E "$M1_CONNECT_RE" "$M1_FIXTURE_LOG")
    echo "${n:-0}"
}

m1_fixtures_ready() {
    kill -0 "$M1_FIXTURE_PID" 2>/dev/null || fail "the fixtures exited: $(cat "$M1_DWORK/fixtures.stderr" 2>/dev/null)"
    [ -s "$M1_DWORK/fixtures.ready" ]
}

m1_start_fixtures() {
    rm -f "$M1_DWORK/fixtures.ready"
    python3 -I "$FIXTURES" --listen 127.0.0.1 --http-port "$M1_HTTP_PORT" --socks-port "$M1_SOCKS_PORT" \
        --log "$M1_FIXTURE_LOG" --ready-file "$M1_DWORK/fixtures.ready" 2>"$M1_DWORK/fixtures.stderr" &
    M1_FIXTURE_PID=$!
    wait_until 15 "the fixtures listening on 127.0.0.1:$M1_HTTP_PORT (HTTP) and 127.0.0.1:$M1_SOCKS_PORT (SOCKS5)" m1_fixtures_ready
}

# Sets M1_IN_ADDR and M1_IN_PORT from a status JSON: where a client on this host reaches the mixed inbound.
m1_inbound() {
    M1_IN_ADDR=$(m1_json inbound.address <<<"$1")
    M1_IN_PORT=$(m1_json inbound.port <<<"$1")
    case $M1_IN_ADDR in
        "" | 0.0.0.0) M1_IN_ADDR=127.0.0.1 ;;
        ::) M1_IN_ADDR='[::1]' ;;
        *:*) M1_IN_ADDR="[$M1_IN_ADDR]" ;;
    esac
    [[ $M1_IN_PORT =~ ^[0-9]+$ ]] || fail "the status names no inbound port: $M1_IN_PORT"
}

m1_proxy_once() {
    local before code
    before=$(m1_connects)
    code=$(m1_curl_via "socks5h://$M1_IN_ADDR:$M1_IN_PORT") ||
        fail "curl through Throne's mixed inbound $M1_IN_ADDR:$M1_IN_PORT failed ($1)"
    m1_expect "HTTP status through Throne ($1)" "$code" 204
    (($(m1_connects) > before)) || fail "the request never reached the SOCKS5 fixture, so Throne did not proxy it ($1)"
}

m1_profile_running() {
    local out
    out=$("$THRONECTL" --json status 2>/dev/null) || return 1
    [ "$(m1_json state <<<"$out")" = running ] && [ "$(m1_json profile.id <<<"$out")" = "$1" ]
}

# ---------------------------------------------------------------------------------------------------- inputs

m1_inputs() {
    step "M1: inputs"
    [ "$(uname -s)" = Darwin ] || fail "macos_launchd.sh runs on macOS only"
    sudo -n true 2>/dev/null || fail "passwordless sudo is required"
    local app=${THRONE_APP:-$THRONE_DIR} out
    [ -d "$app" ] || fail "THRONE_APP / THRONE_DIR $app is not a directory"
    app=$(cd "$app" && pwd -P)
    if [ -x "$app/Contents/MacOS/Throne" ]; then
        M1_BIN_DIR=$app/Contents/MacOS
    elif [ -x "$app/Throne.app/Contents/MacOS/Throne" ]; then
        M1_BIN_DIR=$app/Throne.app/Contents/MacOS
    elif [ -x "$app/Throne" ]; then
        M1_BIN_DIR=$app
    else
        fail "no Throne in $app (looked for Contents/MacOS/Throne, Throne.app/Contents/MacOS/Throne and Throne)"
    fi
    THRONE=$M1_BIN_DIR/Throne
    [ -x "$M1_BIN_DIR/ThroneCore" ] || fail "no ThroneCore next to $THRONE"
    THRONECTL=$(command -v "$THRONECTL") || fail "no thronectl ($THRONECTL)"
    [ -f "$FIXTURES" ] || fail "no fixtures script at $FIXTURES"
    [ -f "$M1_PLIST_SRC" ] || fail "no $M1_PLIST_SRC"
    for out in launchctl plutil python3 curl; do
        command -v "$out" >/dev/null || fail "$out is missing on this runner"
    done

    mkdir -p "$E2E_TMP"
    E2E_TMP=$(cd "$E2E_TMP" && pwd -P)
    M1_WORK=$E2E_TMP/m1
    M1_DWORK=$M1_WORK/daemon
    M1_DATA=$M1_DWORK/data
    M1_LAUNCHD_LOG=$M1_DWORK/launchd/throne.log
    M1_PLIST_TMP=$M1_DWORK/$M1_LABEL.plist
    M1_KEY_COPY=$M1_DWORK/api.key
    M1_FIXTURE_LOG=$M1_DWORK/fixtures.log
    M1_FWORK=$M1_WORK/foreground
    M1_FG_DATA=$M1_FWORK/data

    # Unsigned builds only, as in the docs; artifacts that never passed a browser carry no quarantine flag.
    xattr -dr com.apple.quarantine "${M1_BIN_DIR%/Contents/MacOS}" 2>/dev/null || true
    sw_vers || true
    echo "machine: $(uname -m), bash $BASH_VERSION, user $(id -un)"
    file "$THRONE" "$M1_BIN_DIR/ThroneCore" || true
    codesign -dv "$THRONE" 2>&1 || true
    out=$("$THRONE" --headless --version) || fail "Throne --headless --version failed: $out"
    echo "$out"
    [[ $out == "Throne "* ]] || fail "Throne --headless --version printed '$out', want 'Throne <version>'"
    "$THRONECTL" version || fail "thronectl version failed"
}

# ---------------------------------------------------------------------------------------------------- daemon (launchd, root)

m1_launchd_field() {
    local out
    out=$(sudo -n launchctl print "system/$M1_LABEL" 2>/dev/null) || return 0
    printf '%s\n' "$out" | awk -v key="$1" -F ' = ' '$1 == "\t" key && !seen { print $2; seen = 1 }'
}

m1_daemon_pid() { m1_launchd_field pid; }

m1_service_loaded() { sudo -n launchctl print "system/$M1_LABEL" >/dev/null 2>&1; }

m1_service_gone() { ! m1_service_loaded; }

m1_daemon_health() { sudo -n "$THRONE" --health --data-dir "$M1_DATA"; }

m1_daemon_healthy() { m1_daemon_health >/dev/null 2>&1; }

m1_daemon_first_up() {
    local code
    code=$(m1_launchd_field "last exit code")
    case $code in
        "" | "(never exited)") ;;
        *) fail "Throne exited under launchd before it was healthy (last exit code $code)" ;;
    esac
    m1_daemon_healthy
}

m1_daemon_restarted() {
    local pid
    pid=$(m1_daemon_pid)
    [ -n "$pid" ] && [ "$pid" != "$1" ] && m1_daemon_healthy
}

m1_sed_value() { printf '%s' "$1" | sed -e 's/[\\|&]/\\&/g'; }

m1_plist_get() { /usr/libexec/PlistBuddy -c "Print $1" "$M1_PLIST_TMP"; }

m1_write_plist() {
    local path
    for path in "$THRONE" "$M1_DATA" "$M1_LAUNCHD_LOG"; do
        case $path in
            *'&'* | *'<'* | *'>'* | *'|'* | *'\'*) fail "$path holds a character the plist rewrite does not escape" ;;
        esac
    done
    sed -e "s|/Applications/Throne.app/Contents/MacOS/Throne|$(m1_sed_value "$THRONE")|" \
        -e "s|--data-dir=/Library/Application Support/Throne|--data-dir=$(m1_sed_value "$M1_DATA")|" \
        -e "s|/Library/Logs/Throne/throne.log|$(m1_sed_value "$M1_LAUNCHD_LOG")|g" \
        "$M1_PLIST_SRC" >"$M1_PLIST_TMP"
    cat "$M1_PLIST_TMP"
    plutil -lint "$M1_PLIST_TMP" || fail "the rewritten plist does not parse"
    m1_expect Label "$(m1_plist_get :Label)" "$M1_LABEL"
    m1_expect "ProgramArguments[0]" "$(m1_plist_get :ProgramArguments:0)" "$THRONE"
    m1_expect "ProgramArguments[1]" "$(m1_plist_get :ProgramArguments:1)" --headless
    m1_expect "ProgramArguments[2]" "$(m1_plist_get :ProgramArguments:2)" "--data-dir=$M1_DATA"
    m1_expect StandardOutPath "$(m1_plist_get :StandardOutPath)" "$M1_LAUNCHD_LOG"
    m1_expect StandardErrorPath "$(m1_plist_get :StandardErrorPath)" "$M1_LAUNCHD_LOG"
    m1_expect THRONE_RESTART_MODE "$(m1_plist_get :EnvironmentVariables:THRONE_RESTART_MODE)" exit
    # launchd refuses a system plist that is not root's and read-only for others.
    sudo -n install -m 644 -o root -g wheel "$M1_PLIST_TMP" "$M1_PLIST_DST" || fail "cannot install $M1_PLIST_DST"
}

m1_daemon() {
    local rc out conn code id pid1 pid2 copy
    sudo -n rm -rf "${M1_DWORK:?}"
    mkdir -p "$M1_DWORK" "$(dirname "$M1_LAUNCHD_LOG")"

    step "M1 daemon: Throne --check as root"
    rc=0
    sudo -n env HOME=/var/root THRONE_RESTART_MODE=exit "$THRONE" --headless --check --data-dir "$M1_DATA" \
        >"$M1_DWORK/check.txt" 2>&1 || rc=$?
    cat "$M1_DWORK/check.txt"
    m1_expect "Throne --check exit status" "$rc" 0
    if sudo -n test -e "$M1_DATA"; then fail "Throne --check created $M1_DATA; it must change nothing"; fi

    step "M1 daemon: the LaunchDaemon plist, pointed at this app and $M1_DATA"
    if m1_service_loaded; then
        echo "$M1_LABEL is loaded from an earlier run; booting it out"
        sudo -n launchctl bootout "system/$M1_LABEL" || true
        wait_until 60 "the earlier $M1_LABEL unloading" m1_service_gone
    fi
    m1_write_plist

    step "M1 daemon: launchctl bootstrap system, healthy within 90 s"
    sudo -n launchctl bootstrap system "$M1_PLIST_DST" || fail "launchctl bootstrap system $M1_PLIST_DST failed"
    M1_BOOTSTRAPPED=1
    wait_until 90 "Throne under launchd answering --health (as root, --data-dir $M1_DATA) with exit 0" m1_daemon_first_up
    out=$(m1_daemon_health) || fail "Throne --health failed: $out"
    echo "$out"
    m1_expect ok "$(m1_json ok <<<"$out")" true
    m1_expect ready "$(m1_json ready <<<"$out")" true
    m1_expect api "$(m1_json api <<<"$out")" listening
    m1_expect degraded "$(m1_json degraded <<<"$out")" null
    m1_expect headless_dialogs "$(m1_json headless_dialogs <<<"$out")" 0
    m1_expect previous_crash "$(m1_json previous_crash <<<"$out")" false
    pid1=$(m1_daemon_pid)
    [[ $pid1 =~ ^[0-9]+$ ]] || fail "launchctl print shows no PID for $M1_LABEL: '$pid1'"
    m1_expect "data dir owner and mode" "$(sudo -n stat -f '%Su %Lp' "$M1_DATA")" "root 700"
    m1_expect "config dir owner and mode" "$(sudo -n stat -f '%Su %Lp' "$M1_DATA/config")" "root 700"
    copy=$M1_DWORK/launchd-boot1.log
    m1_copy_root "$M1_LAUNCHD_LOG" "$copy"
    grep -qF "headless, PID $pid1, started by --headless," "$copy" || fail "the launchd log has no boot line for PID $pid1"
    grep -qF "data dir $M1_DATA (--data-dir)" "$copy" || fail "Throne did not take the data dir from the plist's --data-dir"
    grep -qF "restart mode exit (requested)" "$copy" || fail "Throne did not take THRONE_RESTART_MODE=exit from the plist"

    step "M1 daemon: --print-connection as root, thronectl"
    conn=$(sudo -n "$THRONE" --print-connection --data-dir "$M1_DATA") || fail "Throne --print-connection as root failed: $conn"
    echo "$conn"
    eval "$conn"
    m1_expect THRONE_ADDR "${THRONE_ADDR:-}" "$M1_DAEMON_API"
    m1_expect THRONE_KEY_FILE "${THRONE_KEY_FILE:-}" "$M1_DATA/config/api.key"
    # The key file is root's alone; the docs have users copy it for themselves.
    sudo -n install -m 600 -o "$(id -un)" "$THRONE_KEY_FILE" "$M1_KEY_COPY" || fail "cannot copy $THRONE_KEY_FILE"
    export THRONE_KEY_FILE=$M1_KEY_COPY
    out=$("$THRONECTL" --json status) || fail "thronectl status failed"
    echo "$out"
    m1_expect capabilities.headless "$(m1_json capabilities.headless <<<"$out")" true
    m1_expect state "$(m1_json state <<<"$out")" idle
    m1_expect tun "$(m1_json tun <<<"$out")" false
    m1_expect kill_switch "$(m1_json kill_switch <<<"$out")" off

    step "M1 daemon: proxy through the mixed inbound (SOCKS5 profile at the fixture)"
    m1_start_fixtures
    code=$(m1_curl_via "socks5h://127.0.0.1:$M1_SOCKS_PORT") || fail "the fixtures do not answer each other"
    m1_expect "fixture self-test HTTP status" "$code" 204
    (($(m1_connects) > 0)) || fail "the SOCKS5 fixture did not log its own CONNECT"
    out=$(printf 'socks5://127.0.0.1:%s#%s\n' "$M1_SOCKS_PORT" "$M1_PROFILE_NAME" | "$THRONECTL" --json import -) ||
        fail "thronectl import failed: $out"
    echo "$out"
    m1_expect "imported profiles" "$(m1_json imported <<<"$out")" 1
    id=$(m1_json profile_ids.0 <<<"$out")
    [[ $id =~ ^[0-9]+$ ]] || fail "the import returned no profile id: $id"
    "$THRONECTL" --timeout 90s connect "$id" || fail "thronectl connect $id failed"
    out=$("$THRONECTL" --json status) || fail "thronectl status failed"
    echo "$out"
    m1_expect state "$(m1_json state <<<"$out")" running
    m1_expect profile.id "$(m1_json profile.id <<<"$out")" "$id"
    m1_inbound "$out"
    m1_proxy_once "first boot"

    step "M1 daemon: SIGHUP exits 75, launchd starts Throne again, the profile resumes"
    sudo -n kill -HUP "$pid1" || fail "cannot send SIGHUP to PID $pid1"
    wait_until 90 "launchd starting Throne again under a PID other than $pid1, answering --health with exit 0" \
        m1_daemon_restarted "$pid1"
    pid2=$(m1_daemon_pid)
    echo "Throne PID $pid1 -> $pid2"
    code=$(m1_launchd_field "last exit code")
    if [ -z "$code" ]; then
        echo "launchctl print shows no 'last exit code'; Throne's own EXIT line decides"
    else
        m1_expect "launchd's last exit code" "${code%%[!0-9]*}" 75
    fi
    out=$(m1_daemon_health) || fail "Throne --health failed after the restart: $out"
    echo "$out"
    m1_expect "ok after the restart" "$(m1_json ok <<<"$out")" true
    m1_expect "previous_crash after the restart" "$(m1_json previous_crash <<<"$out")" false
    m1_expect "previous_exit.code after the restart" "$(m1_json previous_exit.code <<<"$out")" 75
    m1_expect "previous_exit.reason after the restart" "$(m1_json previous_exit.reason <<<"$out")" SIGHUP
    wait_until 90 "profile $id running again after the restart (resumed)" m1_profile_running "$id"
    copy=$M1_DWORK/launchd-boot2.log
    m1_copy_root "$M1_LAUNCHD_LOG" "$copy"
    m1_expect "'EXIT 75: SIGHUP' lines" "$(m1_count -E '[|] EXIT 75: SIGHUP$' "$copy")" 1
    grep -qF "headless, PID $pid2, started by --headless," "$copy" || fail "the launchd log has no boot line for PID $pid2"
    grep -qF "[Resume] Resuming profile $id " "$copy" || fail "the restarted Throne did not log resuming profile $id"
    out=$("$THRONECTL" --json status) || fail "thronectl status failed"
    m1_inbound "$out"
    m1_proxy_once "after the SIGHUP restart"
    "$THRONECTL" --timeout 60s disconnect || fail "thronectl disconnect failed"
    out=$("$THRONECTL" --json status) || fail "thronectl status failed"
    m1_expect "state after disconnect" "$(m1_json state <<<"$out")" idle

    step "M1 daemon: launchctl bootout, a clean exit"
    rc=0
    sudo -n launchctl bootout "system/$M1_LABEL" || rc=$?
    M1_BOOTSTRAPPED=
    if ((rc != 0)); then echo "launchctl bootout returned $rc; the waits below decide"; fi
    wait_until 60 "Throne (PID $pid2) exiting after the bootout" m1_pid_gone "$pid2"
    wait_until 15 "launchd dropping $M1_LABEL" m1_service_gone
    wait_until 15 "no ThroneCore process left" m1_no_core
    sudo -n rm -f "$M1_PLIST_DST"
    m1_copy_root "$M1_LAUNCHD_LOG" "$M1_DWORK/launchd-final.log"
    m1_copy_root "$M1_DATA/config/logs/throne.log" "$M1_DWORK/throne-final.log"
    assert_log_clean "$M1_DWORK/launchd-final.log" "$M1_DWORK/throne-final.log"
    m1_expect_exit_line "$M1_DWORK/launchd-final.log" "EXIT 0: SIGTERM"
    [[ "$(tail -n 1 "$M1_DWORK/throne-final.log")" == *"$M1_SESSION_END" ]] ||
        fail "config/logs/throne.log does not end with '$M1_SESSION_END'"
    if sudo -n test -e "$M1_DATA/config/logs/running.marker"; then fail "running.marker was left behind"; fi
    m1_assert_no_ipc "$(sudo -n getconf DARWIN_USER_TEMP_DIR 2>/dev/null || true)"
}

m1_daemon_dump() {
    echo "::group::launchctl print system/$M1_LABEL"
    sudo -n launchctl print "system/$M1_LABEL" 2>&1
    echo "::endgroup::"
    m1_show "$M1_DWORK/check.txt"
    m1_show_root "$M1_LAUNCHD_LOG" 400
    sudo -n sh -c 'for f in "$1"/config/logs/*.log; do
        [ -e "$f" ] || continue
        echo "::group::$f"
        tail -n 200 "$f"
        echo "::endgroup::"
    done' sh "$M1_DATA"
    m1_show "$M1_FIXTURE_LOG"
    m1_show "$M1_DWORK/fixtures.stderr"
}

m1_daemon_cleanup() {
    local rc=$? pid
    set +e
    if ((rc != 0)); then
        echo "M1 daemon failed (status $rc); its logs follow"
        m1_daemon_dump
    fi
    if [ -n "$M1_BOOTSTRAPPED" ] || m1_service_loaded; then
        pid=$(m1_daemon_pid)
        sudo -n launchctl bootout "system/$M1_LABEL" >/dev/null 2>&1
        if [ -n "$pid" ] && ! m1_pid_gone "$pid"; then sudo -n kill -KILL "$pid" 2>/dev/null; fi
    fi
    sudo -n rm -f "$M1_PLIST_DST"
    sudo -n pkill -KILL -U 0 -x ThroneCore 2>/dev/null
    if [ -n "$M1_FIXTURE_PID" ]; then
        kill "$M1_FIXTURE_PID" 2>/dev/null
        wait "$M1_FIXTURE_PID" 2>/dev/null
    fi
    # The artifact upload runs as the runner user.
    if [ -d "$M1_DWORK" ]; then
        sudo -n chown -R "$(id -un)" "$M1_DWORK" 2>/dev/null
        chmod -R u+rwX "$M1_DWORK" 2>/dev/null
    fi
    exit "$rc"
}

# ---------------------------------------------------------------------------------------------------- foreground (runner user)

m1_fg_start() {
    M1_FG_LOG=$1
    shift
    python3 -I -c "$M1_EXEC_DEFAULT_SIGNALS" "$THRONE" --headless --data-dir "$M1_FG_DATA" --api-listen "$M1_FG_API" \
        --log-format plain "$@" >"$M1_FG_LOG" 2>&1 &
    M1_FG_PID=$!
    echo "Throne PID $M1_FG_PID, log $M1_FG_LOG"
}

m1_fg_alive() { kill -0 "$M1_FG_PID" 2>/dev/null || fail "Throne (PID $M1_FG_PID) exited early; its log is $M1_FG_LOG"; }

m1_fg_health() { "$THRONE" --health --data-dir "$M1_FG_DATA"; }

m1_fg_healthy() {
    m1_fg_alive
    m1_fg_health >/dev/null 2>&1
}

m1_fg_gone() { ! kill -0 "$M1_FG_PID" 2>/dev/null; }

m1_fg_reexeced() {
    m1_fg_alive
    [ "$(m1_count -F "headless, PID $1, started by --headless," "$M1_FG_LOG")" -ge 2 ] && m1_fg_health >/dev/null 2>&1
}

m1_fg_check_health() {
    local out
    out=$(m1_fg_health) || fail "Throne --health failed ($1): $out"
    echo "$out"
    m1_expect "ok ($1)" "$(m1_json ok <<<"$out")" true
    m1_expect "headless_dialogs ($1)" "$(m1_json headless_dialogs <<<"$out")" 0
    m1_expect "previous_crash ($1)" "$(m1_json previous_crash <<<"$out")" false
}

m1_fg_boot() {
    local log=$1 what=$2
    shift 2
    m1_fg_start "$log" "$@"
    wait_until 60 "Throne ($what) answering --health with exit 0" m1_fg_healthy
    m1_fg_check_health "$what"
}

m1_fg_wait_exit() {
    local rc=0
    wait_until 45 "Throne (PID $M1_FG_PID) exiting after $1" m1_fg_gone
    wait "$M1_FG_PID" || rc=$?
    M1_FG_PID=
    m1_expect "exit status after $1" "$rc" "$2"
}

m1_fg_assert_clean() {
    local file=$M1_FG_DATA/config/logs/throne.log
    [ -f "$file" ] || fail "no $file ($1)"
    assert_log_clean "$M1_FG_LOG" "$file"
    [[ "$(tail -n 1 "$file")" == *"$M1_SESSION_END" ]] || fail "$file does not end with '$M1_SESSION_END' ($1)"
    [ ! -e "$M1_FG_DATA/config/logs/running.marker" ] || fail "running.marker was left behind ($1)"
    wait_until 15 "no ThroneCore process left ($1)" m1_no_core
}

m1_fg_status() {
    local out
    connect_env "$THRONE" "$M1_FG_DATA"
    m1_expect "THRONE_ADDR ($1)" "${THRONE_ADDR:-}" "$M1_FG_API"
    out=$("$THRONECTL" --json status) || fail "thronectl status failed ($1)"
    echo "$out"
    m1_expect "capabilities.headless ($1)" "$(m1_json capabilities.headless <<<"$out")" true
    m1_expect "state ($1)" "$(m1_json state <<<"$out")" idle
}

m1_foreground() {
    local pid file f out id core t0
    rm -rf "${M1_FWORK:?}"
    mkdir -p "$M1_FWORK"
    file=$M1_FG_DATA/config/logs/throne.log

    step "M1 foreground: SIGTERM exits 0"
    m1_fg_boot "$M1_FWORK/run-sigterm.log" "first start"
    grep -qF "restart mode exec (" "$M1_FG_LOG" || fail "Throne started from a shell did not pick the exec restart mode"
    m1_fg_status "first start"
    kill -TERM "$M1_FG_PID"
    m1_fg_wait_exit SIGTERM 0
    m1_expect_exit_line "$M1_FWORK/run-sigterm.log" "EXIT 0: SIGTERM"
    m1_fg_assert_clean SIGTERM

    step "M1 foreground: SIGHUP restarts in place under the same PID"
    m1_fg_boot "$M1_FWORK/run-sighup-exec.log" "second start"
    pid=$M1_FG_PID
    kill -HUP "$pid"
    wait_until 60 "Throne logging a second boot line for PID $pid and answering --health with exit 0" m1_fg_reexeced "$pid"
    m1_expect "'EXIT 75: SIGHUP' lines" "$(m1_count -E '[|] EXIT 75: SIGHUP$' "$M1_FG_LOG")" 1
    grep -qF "restarting in place: " "$M1_FG_LOG" || fail "no 'restarting in place' line before the exec"
    grep -qF "restart mode exec (inherited from the previous image)" "$M1_FG_LOG" ||
        fail "the exec'd image did not inherit the exec restart mode"
    grep -qF "$M1_SESSION_START" "$file" || fail "$file has no session header after the exec restart"
    grep -qE "^  pid +: $pid\$" "$file" || fail "the session header in $file does not name PID $pid"
    m1_fg_check_health "after the exec restart"
    for f in "$M1_FG_DATA"/config/logs/crashed-*.log; do
        if [ -e "$f" ]; then fail "the exec restart left $f: the first image did not end its session cleanly"; fi
    done
    m1_fg_status "after the exec restart"
    kill -TERM "$pid"
    m1_fg_wait_exit "SIGTERM after the exec restart" 0
    m1_expect_exit_line "$M1_FWORK/run-sighup-exec.log" "EXIT 0: SIGTERM"
    m1_fg_assert_clean "exec restart"

    step "M1 foreground: --restart-mode exit, SIGHUP exits 75"
    m1_fg_boot "$M1_FWORK/run-restart-exit.log" "third start" --restart-mode exit
    grep -qF "restart mode exit (requested)" "$M1_FG_LOG" || fail "--restart-mode exit did not take"
    kill -HUP "$M1_FG_PID"
    m1_fg_wait_exit "SIGHUP with --restart-mode exit" 75
    m1_expect_exit_line "$M1_FWORK/run-restart-exit.log" "EXIT 75: SIGHUP"
    m1_fg_assert_clean "--restart-mode exit"

    # The last boot: the profile it connects would be resumed by any later one.
    step "M1 foreground: a second SIGINT during the teardown kills the core"
    m1_fg_boot "$M1_FWORK/run-double-sigint.log" "last start" --no-resume
    m1_fg_status "last start"
    out=$(printf 'socks5://127.0.0.1:%s#%s\n' "$M1_SOCKS_PORT" "$M1_PROFILE_NAME" | "$THRONECTL" --json import -) ||
        fail "thronectl import failed: $out"
    id=$(m1_json profile_ids.0 <<<"$out")
    [[ $id =~ ^[0-9]+$ ]] || fail "the import returned no profile id: $id"
    # Run 2 could not bind 127.0.0.1:2080 here after the root daemon part (a listener, or root's TIME_WAIT sockets, which
    # macOS lets block another user's bind); record what is on the port, and keep this part off it.
    echo "Sockets on port 2080 before the foreground connect:"
    sudo -n netstat -anv -p tcp 2>/dev/null | grep -E '[.]2080 ' || true
    holders=$(sudo -n lsof -nP -iTCP:2080 -sTCP:LISTEN 2>/dev/null || true)
    if [ -n "$holders" ]; then
        echo "::warning::127.0.0.1:2080 is still held before the foreground connect"
        echo "$holders"
        ps -axo pid,ppid,user,comm | grep -i throne || true
    fi
    "$THRONECTL" settings set inbound_port="$M1_FG_IN_PORT" || fail "thronectl settings set inbound_port=$M1_FG_IN_PORT failed"
    "$THRONECTL" --timeout 90s connect "$id" || fail "thronectl connect $id failed"
    # Back-to-back kills can merge into one pending SIGINT. A frozen core keeps the teardown in its 10 s Stop call, so the
    # second SIGINT is a delivery of its own, judged inside the teardown.
    core=$(pgrep -n -P "$M1_FG_PID" -x ThroneCore) || fail "no ThroneCore child of Throne $M1_FG_PID"
    kill -STOP "$core"
    kill -INT "$M1_FG_PID"
    wait_until 15 "Throne logging 'EXIT 0: SIGINT' (its teardown has begun)" grep -qF '| EXIT 0: SIGINT' "$M1_FG_LOG"
    t0=$SECONDS
    kill -INT "$M1_FG_PID"
    m1_fg_wait_exit "two SIGINTs" 0
    ((SECONDS - t0 < 8)) ||
        fail "Throne took $((SECONDS - t0)) s to exit after the second SIGINT: the fast path did not cut the 10 s Stop wait"
    grep -qF "SIGINT: second signal: killing the core" "$M1_FWORK/run-double-sigint.log" ||
        fail "the second SIGINT did not take the fast path (no 'SIGINT: second signal: killing the core' line)"
    m1_expect_exit_line "$M1_FWORK/run-double-sigint.log" "EXIT 0: SIGINT"
    m1_fg_assert_clean "double SIGINT"
    m1_assert_no_ipc "${TMPDIR:-}" "$(getconf DARWIN_USER_TEMP_DIR 2>/dev/null || true)"
}

m1_foreground_cleanup() {
    local rc=$? f i
    set +e
    if ((rc != 0)); then
        echo "M1 foreground failed (status $rc); its logs follow"
        for f in "$M1_FWORK"/run-*.log; do m1_show "$f" 300; done
        for f in "$M1_FG_DATA"/config/logs/*.log; do m1_show "$f" 200; done
    fi
    if [ -n "$M1_FG_PID" ] && kill -0 "$M1_FG_PID" 2>/dev/null; then
        kill -TERM "$M1_FG_PID"
        for i in $(seq 40); do
            kill -0 "$M1_FG_PID" 2>/dev/null || break
            sleep 0.5
        done
        kill -KILL "$M1_FG_PID" 2>/dev/null
    fi
    pkill -KILL -u "$(id -u)" -x ThroneCore 2>/dev/null
    exit "$rc"
}

# ---------------------------------------------------------------------------------------------------- main

# Each part runs in its own subshell with its own cleanup, so a failed daemon part still runs the foreground part.
m1_run() {
    local rc
    set +e
    (
        set -e
        trap "m1_$1_cleanup" EXIT
        "m1_$1"
    )
    rc=$?
    set -e
    if ((rc == 0)); then
        echo "M1 $1: passed"
        m1_summary "M1 $1: passed"
    else
        echo "::error::M1 $1 failed (status $rc)"
        m1_summary "M1 $1: FAILED (status $rc)"
        M1_FAILED="$M1_FAILED $1"
    fi
}

m1_inputs
m1_run daemon
m1_run foreground
[ -z "$M1_FAILED" ] || fail "M1 failed:$M1_FAILED"
echo "M1 passed"
