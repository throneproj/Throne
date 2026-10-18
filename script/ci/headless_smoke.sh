#!/usr/bin/env bash
# CI smoke test of headless Throne on Linux: leg "a" as the runner user, leg "b" as root under systemd-run Type=notify.
# Env: THRONE_DIR (default <repo>/deployment/linux-amd64), THRONECTL (leg a), SMOKE_WORK (default $RUNNER_TEMP/throne-smoke).
set -Eeuo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
LEG=${1:?usage: headless_smoke.sh a|b}
THRONE_DIR=${THRONE_DIR:-$ROOT/deployment/linux-amd64}
THRONE="$THRONE_DIR/Throne"
THRONECTL=${THRONECTL:-thronectl}
WORK=${SMOKE_WORK:-${RUNNER_TEMP:-/tmp}/throne-smoke}

# A widget or dialog creeping into headless must fail here, not render offscreen.
unset DISPLAY WAYLAND_DISPLAY QT_QPA_PLATFORM

API_LISTEN=127.0.0.1:19095
HTTP_PORT=18080
SOCKS_PORT=1080
FIXTURE_URL="http://fixture.test:$HTTP_PORT/"
FIXTURE_CONNECT="^CONNECT fixture\\.test:$HTTP_PORT -> 127\\.0\\.0\\.1:$HTTP_PORT ok\$"

B_UNIT="throne-smoke-${GITHUB_RUN_ID:-$$}-${GITHUB_RUN_ATTEMPT:-1}"
B_DATA=/var/lib/throne-smoke
B_INSTALL=/opt/throne-smoke
B_API_LISTEN=127.0.0.1:19096
B_RECORD="$WORK/b-exits.txt"

DATA=
THRONE_PID=
FIXTURE_PID=
B_STARTED=

say() { printf '\n=== %s\n' "$*"; }
fail() {
    printf '::error::%s\n' "$*" >&2
    exit 1
}
trap 'printf "::error::line %s: \"%s\" failed with status %s\n" "$LINENO" "$BASH_COMMAND" "$?" >&2' ERR

expect_eq() { [ "$2" = "$3" ] || fail "$1 is '$2', want '$3'"; }

# jget PATH: the value at a dotted PATH (list indexes as numbers) of the JSON on stdin; strings raw, the rest as JSON.
jget() {
    python3 -I -c '
import json, sys
value = json.load(sys.stdin)
for part in filter(None, sys.argv[1].split(".")):
    value = value[int(part)] if isinstance(value, list) else value[part]
print(value if isinstance(value, str) else json.dumps(value))
' "$1"
}

# wait_for SECONDS WHAT COMMAND...: retries COMMAND once a second until it succeeds.
wait_for() {
    local limit=$1 what=$2 deadline=$((SECONDS + $1))
    shift 2
    until "$@"; do
        ((SECONDS < deadline)) || fail "$what: not within $limit s"
        sleep 1
    done
}

group() {
    local title=$1
    shift
    echo "::group::$title"
    "$@" || true
    echo "::endgroup::"
}

show_file() {
    if [ -e "$1" ]; then
        group "$1" cat "$1"
    fi
}

no_core() { ! pgrep -x ThroneCore >/dev/null; }

assert_no_ipc_sockets() {
    local dir
    for dir in /tmp "${TMPDIR:-/tmp}"; do
        if compgen -G "$dir/throneIPC-*" >/dev/null; then
            ls -l "$dir"/throneIPC-* >&2
            fail "core IPC sockets are left in $dir"
        fi
    done
}

check_binaries() {
    [ -x "$THRONE" ] || fail "no Throne at $THRONE"
    [ -x "$THRONE_DIR/ThroneCore" ] || fail "no ThroneCore in $THRONE_DIR"
    local missing
    missing=$(ldd "$THRONE" | grep 'not found' || true)
    if [ -n "$missing" ]; then
        echo "$missing" >&2
        fail "Throne cannot load its shared libraries on this runner (lines above)"
    fi
}

# ---------------------------------------------------------------------------------------------------- leg a: runner user

ctl() { "$THRONECTL" "$@"; }

connects() { grep -cE "$FIXTURE_CONNECT" "$WORK/fixtures.log" || true; }

start_fixtures() {
    rm -f "$WORK/fixtures.ready"
    python3 -I "$ROOT/script/ci/smoke_fixtures.py" --http-port "$HTTP_PORT" --socks-port "$SOCKS_PORT" \
        --log "$WORK/fixtures.log" --ready-file "$WORK/fixtures.ready" 2>"$WORK/fixtures.stderr" &
    FIXTURE_PID=$!
    wait_for 15 "the fixtures listening" fixtures_ready
    local code
    code=$(curl -sS --max-time 10 -x "socks5h://127.0.0.1:$SOCKS_PORT" -o /dev/null -w '%{http_code}' "$FIXTURE_URL") ||
        fail "the fixtures do not answer each other"
    expect_eq "fixture self-test HTTP status" "$code" 204
    (($(connects) > 0)) || fail "the SOCKS5 fixture did not log its own CONNECT"
}

fixtures_ready() {
    kill -0 "$FIXTURE_PID" 2>/dev/null || fail "the fixtures exited: $(cat "$WORK/fixtures.stderr")"
    [ -s "$WORK/fixtures.ready" ]
}

stop_fixtures() {
    [ -n "$FIXTURE_PID" ] || return 0
    kill "$FIXTURE_PID" 2>/dev/null || true
    wait "$FIXTURE_PID" 2>/dev/null || true
    FIXTURE_PID=
}

run_check() {
    local rc=0
    "$THRONE" --headless --check --data-dir "$DATA" --api-listen "$API_LISTEN" >"$1" 2>&1 || rc=$?
    cat "$1"
    expect_eq "Throne --headless --check exit status" "$rc" 0
}

start_throne() {
    local logfile=$1
    shift
    "$THRONE" --headless --data-dir "$DATA" --api-listen "$API_LISTEN" --log-format plain "$@" >"$logfile" 2>&1 &
    THRONE_PID=$!
    echo "Throne PID $THRONE_PID, log $logfile"
}

healthy() {
    if ! kill -0 "$THRONE_PID" 2>/dev/null; then
        local rc=0
        wait "$THRONE_PID" || rc=$?
        THRONE_PID=
        fail "Throne exited with status $rc before it was healthy"
    fi
    "$THRONE" --health --data-dir "$DATA" >/dev/null 2>&1
}

health_json() {
    local out
    out=$("$THRONE" --health --data-dir "$DATA") || fail "Throne --health failed: $out"
    echo "$out"
}

stop_throne() {
    local deadline=$((SECONDS + 45)) rc=0
    if ! kill -TERM "$THRONE_PID" 2>/dev/null; then
        wait "$THRONE_PID" || rc=$?
        THRONE_PID=
        fail "Throne had already exited with status $rc before SIGTERM"
    fi
    while kill -0 "$THRONE_PID" 2>/dev/null; do
        ((SECONDS < deadline)) || fail "Throne did not exit within 45 s of SIGTERM"
        sleep 0.5
    done
    wait "$THRONE_PID" || rc=$?
    THRONE_PID=
    expect_eq "exit status after SIGTERM" "$rc" 0
}

assert_clean_exit() {
    local runlog=$1 filelog="$DATA/config/logs/throne.log"
    [ -f "$filelog" ] || fail "no $filelog"
    if grep -nE 'HEADLESS-(DIALOG|WIDGET)|POLICY-UNBOUND' "$runlog" "$filelog"; then
        fail "headless reached a dialog, a widget or an unbound policy call (lines above)"
    fi
    grep -qF '===== Throne session end (clean) =====' "$filelog" || fail "$filelog has no clean session end"
    [ ! -e "$DATA/config/logs/running.marker" ] || fail "running.marker was left behind"
    wait_for 5 "ThroneCore exiting" no_core
    assert_no_ipc_sockets
}

leg_a() {
    command -v "$THRONECTL" >/dev/null || fail "no thronectl ($THRONECTL)"
    DATA="$WORK/a-data"
    rm -rf "$DATA"
    start_fixtures

    say "Throne --headless --check before the first start"
    run_check "$WORK/a-check-before.txt"

    say "Starting Throne as $(id -un)"
    start_throne "$WORK/a-run.log"
    wait_for 60 "Throne reporting healthy" healthy

    say "Throne --headless --check while it runs"
    run_check "$WORK/a-check-running.txt"

    say "Connecting thronectl"
    local out id address port before code latency
    out=$("$THRONE" --print-connection --data-dir "$DATA") || fail "Throne --print-connection failed"
    echo "$out"
    eval "$out"
    expect_eq THRONE_ADDR "${THRONE_ADDR:-}" "$API_LISTEN"
    [ -r "${THRONE_KEY_FILE:-}" ] || fail "THRONE_KEY_FILE '${THRONE_KEY_FILE:-}' is not a readable file"

    ctl settings set "test_url=$FIXTURE_URL"
    out=$(printf 'socks5://127.0.0.1:%s#smoke\n' "$SOCKS_PORT" | ctl --json import -)
    echo "$out"
    expect_eq "imported profiles" "$(jget imported <<<"$out")" 1
    id=$(jget profile_ids.0 <<<"$out")

    say "thronectl connect $id"
    ctl --timeout 90s connect "$id"
    out=$(ctl --json status)
    echo "$out"
    expect_eq state "$(jget state <<<"$out")" running
    expect_eq tun "$(jget tun <<<"$out")" false
    expect_eq capabilities.headless "$(jget capabilities.headless <<<"$out")" true
    address=$(jget inbound.address <<<"$out")
    port=$(jget inbound.port <<<"$out")
    case $address in
        "" | 0.0.0.0) address=127.0.0.1 ;;
        ::) address='[::1]' ;;
        *:*) address="[$address]" ;;
    esac

    say "curl through the mixed inbound $address:$port"
    before=$(connects)
    code=$(curl -sS --max-time 20 -x "socks5h://$address:$port" -o /dev/null -w '%{http_code}' "$FIXTURE_URL") ||
        fail "curl through Throne's inbound failed"
    expect_eq "HTTP status through Throne" "$code" 204
    (($(connects) > before)) || fail "the request never reached the SOCKS5 fixture, so Throne did not proxy it"

    say "thronectl test latency $id"
    out=$(ctl --json --timeout 120s test latency "$id")
    echo "$out"
    expect_eq "latency test state" "$(jget state <<<"$out")" done
    latency=$(jget results.0.latency_ms <<<"$out")
    [[ $latency =~ ^[0-9]+$ ]] && ((latency > 0)) || fail "latency_ms is $latency, want > 0"

    say "thronectl disconnect"
    ctl --timeout 60s disconnect
    out=$(ctl --json status)
    expect_eq state "$(jget state <<<"$out")" idle

    say "Throne --health"
    out=$(health_json)
    echo "$out"
    expect_eq ok "$(jget ok <<<"$out")" true
    expect_eq headless_dialogs "$(jget headless_dialogs <<<"$out")" 0

    say "SIGTERM"
    stop_throne
    assert_clean_exit "$WORK/a-run.log"

    say "Restart with --no-resume"
    start_throne "$WORK/a-run2.log" --no-resume
    wait_for 60 "Throne reporting healthy after the restart" healthy
    out=$(health_json)
    echo "$out"
    expect_eq previous_crash "$(jget previous_crash <<<"$out")" false
    if grep -n 'did not shut down cleanly' "$WORK/a-run2.log" "$DATA/config/logs/throne.log"; then
        fail "the restart reports an unclean previous session (lines above)"
    fi
    stop_throne
    assert_clean_exit "$WORK/a-run2.log"
    stop_fixtures
}

dump_a() {
    local file
    for file in "$WORK"/a-check-*.txt "$WORK"/a-run*.log "$DATA"/config/logs/*.log "$WORK"/fixtures.log "$WORK"/fixtures.stderr; do
        show_file "$file"
    done
}

cleanup_a() {
    if [ -n "$THRONE_PID" ] && kill -0 "$THRONE_PID" 2>/dev/null; then
        kill -TERM "$THRONE_PID" 2>/dev/null
        local i
        for i in $(seq 20); do
            kill -0 "$THRONE_PID" 2>/dev/null || break
            sleep 0.5
        done
        kill -KILL "$THRONE_PID" 2>/dev/null
    fi
    pkill -KILL -x -u "$(id -u)" ThroneCore 2>/dev/null
    stop_fixtures
}

# ---------------------------------------------------------------------------------------------------- leg b: systemd

prop() { systemctl show "$B_UNIT" -p "$1" --value; }

status_is() { [ "$(prop StatusText)" = "$1" ]; }

watchdog_moved() { [ "$(prop WatchdogTimestampMonotonic)" != "$1" ]; }

restarted() {
    [ "$(prop NRestarts)" = 1 ] && [ "$(prop ActiveState)" = active ] && [ "$(prop SubState)" = running ] &&
        [ "$(prop MainPID)" != "$1" ]
}

b_health() {
    local out
    out=$(sudo "$B_INSTALL/Throne" --health --data-dir="$B_DATA") || fail "Throne --health as root failed: $out"
    echo "$out"
    expect_eq ok "$(jget ok <<<"$out")" true
    expect_eq headless_dialogs "$(jget headless_dialogs <<<"$out")" 0
    expect_eq previous_crash "$(jget previous_crash <<<"$out")" false
}

leg_b() {
    sudo systemctl reset-failed "$B_UNIT" >/dev/null 2>&1 || true
    sudo rm -rf "$B_DATA" "$B_INSTALL"
    rm -f "$B_RECORD"
    # Root runs a root-owned copy, as from a package.
    sudo mkdir -p "$B_INSTALL"
    sudo cp -a "$THRONE_DIR/." "$B_INSTALL/"
    sudo chown -R root:root "$B_INSTALL"
    printf '#!/bin/sh\necho "$SERVICE_RESULT $EXIT_CODE $EXIT_STATUS" >> '\''%s'\''\n' "$B_RECORD" >"$WORK/b-record.sh"

    say "sudo systemd-run $B_UNIT (Type=notify, WatchdogSec=30)"
    B_STARTED=$SECONDS
    sudo systemd-run --unit="$B_UNIT" \
        -p Type=notify -p NotifyAccess=main -p WatchdogSec=30 -p KillMode=mixed \
        -p TimeoutStartSec=90 -p TimeoutStopSec=45 -p Restart=on-failure \
        -p RestartForceExitStatus=75 -p "RestartPreventExitStatus=3 64 78" \
        -p "ExecStopPost=/bin/sh $WORK/b-record.sh" \
        --setenv=HOME="$B_DATA" \
        "$B_INSTALL/Throne" --headless --data-dir="$B_DATA" --api-listen="$B_API_LISTEN" ||
        fail "systemd-run failed: Throne exited, or never sent READY=1 within TimeoutStartSec"

    expect_eq ActiveState "$(prop ActiveState)" active
    expect_eq SubState "$(prop SubState)" running
    expect_eq WatchdogUSec "$(prop WatchdogUSec)" 30s
    wait_for 15 "StatusText Idle" status_is Idle

    say "Watchdog pings"
    local first left pid
    first=$(prop WatchdogTimestampMonotonic)
    wait_for 25 "a WATCHDOG=1 ping" watchdog_moved "$first"
    left=$((B_STARTED + 40 - SECONDS))
    if ((left > 0)); then sleep "$left"; fi
    expect_eq "ActiveState past WatchdogSec" "$(prop ActiveState)" active
    expect_eq "NRestarts past WatchdogSec" "$(prop NRestarts)" 0
    b_health

    say "SIGHUP: exit 75, restarted by systemd"
    pid=$(prop MainPID)
    ((pid > 0)) || fail "the unit has no MainPID"
    sudo kill -HUP "$pid"
    wait_for 90 "the unit restarted and ready again" restarted "$pid"
    wait_for 15 "StatusText Idle after the restart" status_is Idle
    b_health

    say "systemctl stop"
    sudo systemctl stop "$B_UNIT"
    # A transient unit that stopped cleanly is unloaded at once; the ExecStopPost record keeps every exit.
    if [ "$(prop LoadState)" = loaded ]; then
        expect_eq Result "$(prop Result)" success
        expect_eq ExecMainStatus "$(prop ExecMainStatus)" 0
    fi
    cat "$B_RECORD"
    expect_eq "exit after SIGHUP" "$(sed -n 1p "$B_RECORD")" "exit-code exited 75"
    expect_eq "exit after systemctl stop" "$(sed -n 2p "$B_RECORD")" "success exited 0"
    expect_eq "recorded exits" "$(wc -l <"$B_RECORD")" 2

    sudo journalctl -u "$B_UNIT" --no-pager -o cat >"$WORK/b-journal.txt"
    if grep -nE 'HEADLESS-(DIALOG|WIDGET)|POLICY-UNBOUND' "$WORK/b-journal.txt"; then
        fail "headless reached a dialog, a widget or an unbound policy call (lines above)"
    fi
    if grep -niE 'watchdog timeout|did not shut down cleanly' "$WORK/b-journal.txt"; then
        fail "the watchdog fired, or a restart found an unclean previous session (lines above)"
    fi
    sudo cat "$B_DATA/config/logs/throne.log" >"$WORK/b-throne.log"
    grep -qF '===== Throne session end (clean) =====' "$WORK/b-throne.log" || fail "throne.log has no clean session end"
    if sudo test -e "$B_DATA/config/logs/running.marker"; then fail "running.marker was left behind"; fi
    wait_for 5 "ThroneCore exiting" no_core
    assert_no_ipc_sockets
}

dump_b() {
    group "systemctl status $B_UNIT" systemctl status "$B_UNIT" --no-pager -l
    group "journal of $B_UNIT" sudo journalctl -u "$B_UNIT" --no-pager -o short-monotonic
    group "$B_DATA/config/logs" sudo sh -c "ls -la '$B_DATA/config/logs' && tail -n +1 '$B_DATA'/config/logs/*.log"
    show_file "$B_RECORD"
}

cleanup_b() {
    sudo systemctl stop "$B_UNIT" >/dev/null 2>&1
    sudo systemctl reset-failed "$B_UNIT" >/dev/null 2>&1
    sudo rm -rf "$B_DATA" "$B_INSTALL"
}

# ---------------------------------------------------------------------------------------------------- main

on_exit() {
    local rc=$?
    set +e
    trap - ERR
    if ((rc != 0)); then
        say "FAILED (status $rc); logs follow"
        "dump_$LEG"
    fi
    "cleanup_$LEG"
    exit "$rc"
}

case $LEG in
    a | b) ;;
    *) fail "usage: headless_smoke.sh a|b" ;;
esac
mkdir -p "$WORK"
trap on_exit EXIT
check_binaries
"leg_$LEG"
say "Leg $LEG passed"
