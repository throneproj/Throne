#!/usr/bin/env bash
# L3 of the headless e2e suite, on the arm64 runner. In debian:bookworm, the stand-in for Raspberry Pi OS bookworm
# (glibc 2.36): apt refuses the bundled arm64 .deb for its glibc floor, and installs the system-qt one, which then runs
# headless in the foreground through the proxy scenario.
# Env: SYSTEMQT_DEB, BUNDLED_DEB (Throne-<tag>-debian-arm64-system-qt.deb, Throne-<tag>-debian-arm64.deb), THRONECTL
# (a linux/arm64 thronectl), FIXTURES, E2E_TMP; BOOKWORM_IMAGE (default debian:bookworm). Needs docker.
# The system-qt part re-runs this script inside its container, with the argument "inside".
set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
ROOT=$(cd "$SCRIPT_DIR/../../.." && pwd)
: "${E2E_TMP:=${RUNNER_TEMP:-/tmp}/throne-e2e}"
: "${FIXTURES:=$ROOT/script/ci/smoke_fixtures.py}"
export E2E_TMP FIXTURES
# shellcheck source=lib.sh
source "$SCRIPT_DIR/lib.sh"

IMAGE=${BOOKWORM_IMAGE:-debian:bookworm}
PLATFORM=linux/arm64

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

# ==================================================================================================== host (runner)

CONTAINER=
OUT=$E2E_TMP/linux-arm64

host_cleanup() {
    if [ -n "$CONTAINER" ]; then
        docker rm -f "$CONTAINER" >/dev/null 2>&1
    fi
    return 0
}

# The bundled build carries its own Qt but needs the glibc of the image it was built on, which bookworm lacks.
host_bundled_refused() {
    local deb=$1 floor have out rc=0
    floor=$(dpkg-deb -f "$deb" Depends | grep -oE 'libc6 \(>= [^)]+\)' | head -n1 | sed -E 's/^libc6 \(>= ([^)]+)\)$/\1/' || true)
    dpkg-deb -f "$deb" Depends
    [ -n "$floor" ] || fail "$(basename "$deb") declares no libc6 (>= ...) floor, so apt cannot refuse it on an older glibc"
    out=$(docker run --rm --platform "$PLATFORM" -e DEBIAN_FRONTEND=noninteractive \
        -v "$deb:/pkg/throne-bundled.deb:ro" "$IMAGE" bash -c '
            apt-get update -qq >/dev/null || { echo "apt-get update failed"; exit 90; }
            echo "bookworm libc6 $(dpkg-query -W -f="\${Version}" libc6)"
            rc=0
            apt-get install -y --no-install-recommends /pkg/throne-bundled.deb || rc=$?
            echo "apt-get install exit status $rc"
            if dpkg -s throne >/dev/null 2>&1; then echo "throne is installed"; fi
        ' 2>&1) || rc=$?
    echo "$out"
    ((rc == 0)) || fail "the bookworm container for the bundled deb failed with status $rc"
    have=$(sed -n 's/^bookworm libc6 //p' <<<"$out")
    [ -n "$have" ] || fail "could not read bookworm's libc6 version"
    if ! dpkg --compare-versions "$have" lt "$floor"; then
        fail "bookworm's libc6 $have satisfies the bundled deb's floor libc6 (>= $floor), so apt cannot refuse it there"
    fi
    if grep -qx 'apt-get install exit status 0' <<<"$out" || grep -qx 'throne is installed' <<<"$out"; then
        fail "apt installed the bundled arm64 deb on bookworm (glibc $have) although it needs libc6 (>= $floor)"
    fi
    grep -qF "Depends: libc6 (>= $floor)" <<<"$out" ||
        fail "apt refused the bundled deb, but not for libc6 (>= $floor): no 'Depends: libc6 (>= $floor)' line above"
    echo "apt refused the bundled deb: it needs libc6 (>= $floor), bookworm has $have"
}

host_main() {
    local systemqt bundled thronectl fixtures args=()
    systemqt=${SYSTEMQT_DEB:?set SYSTEMQT_DEB to the arm64 system-qt .deb}
    bundled=${BUNDLED_DEB:?set BUNDLED_DEB to the bundled arm64 .deb}
    thronectl=${THRONECTL:?set THRONECTL to a linux/arm64 thronectl}
    for file in "$systemqt" "$bundled" "$thronectl" "$FIXTURES"; do
        [ -f "$file" ] || fail "'$file' is not a file"
    done
    systemqt=$(readlink -f "$systemqt")
    bundled=$(readlink -f "$bundled")
    thronectl=$(readlink -f "$thronectl")
    fixtures=$(readlink -f "$FIXTURES")
    [ "$(uname -m)" = aarch64 ] || fail "L3 runs on an arm64 runner; this one is $(uname -m)"
    command -v docker >/dev/null || fail "no docker on this runner"
    rm -rf "$OUT"
    mkdir -p "$OUT"

    step "L3: pulling $IMAGE ($PLATFORM)"
    docker pull -q --platform "$PLATFORM" "$IMAGE"

    step "L3: apt refuses the bundled arm64 deb on bookworm (glibc floor)"
    host_bundled_refused "$bundled"

    step "L3: the arm64 system-qt deb in bookworm (container)"
    CONTAINER="throne-e2e-arm64-${GITHUB_RUN_ID:-local}-$$"
    args=(--rm --init --name "$CONTAINER" --platform "$PLATFORM"
        -e E2E_INSIDE=1 -e DEBIAN_FRONTEND=noninteractive -e E2E_TMP=/e2e/out
        -e THRONECTL=/usr/local/bin/thronectl -e FIXTURES=/e2e/smoke_fixtures.py
        -v "$SCRIPT_DIR:/e2e/ci:ro" -v "$fixtures:/e2e/smoke_fixtures.py:ro"
        -v "$systemqt:/pkg/throne-system-qt.deb:ro" -v "$thronectl:/usr/local/bin/thronectl:ro"
        -v "$OUT:/e2e/out")
    for var in CI GITHUB_ACTIONS; do
        if [ -n "${!var:-}" ]; then args+=(-e "$var"); fi
    done
    if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then args+=(-e GITHUB_STEP_SUMMARY=/e2e/out/step-summary.md); fi
    local rc=0
    docker run "${args[@]}" "$IMAGE" bash /e2e/ci/linux_arm64.sh inside || rc=$?
    CONTAINER=
    if [ -n "${GITHUB_STEP_SUMMARY:-}" ] && [ -f "$OUT/step-summary.md" ]; then
        cat "$OUT/step-summary.md" >>"$GITHUB_STEP_SUMMARY"
    fi
    ((rc == 0)) || fail "the system-qt scenario in bookworm failed with status $rc (its log is above)"
    echo "L3 passed"
}

# ==================================================================================================== inside (bookworm)

THRONE=/opt/Throne/Throne
DATA=/var/lib/throne-e2e
FILE_LOG=$DATA/config/logs/throne.log
API_LISTEN=127.0.0.1:19383
FIXTURE_HTTP_PORT=18383
FIXTURE_SOCKS_PORT=11383
HTTP_PORT=$FIXTURE_HTTP_PORT
SOCKS_PORT=$FIXTURE_SOCKS_PORT
PROFILE_NAME=e2e-l3-socks
FIXTURE_URL="http://fixture.test:$HTTP_PORT/"
CONNECT_RE="^CONNECT fixture\\.test:$HTTP_PORT -> 127\\.0\\.0\\.1:$HTTP_PORT ok\$"
RUN_LOG=
FIXTURE_LOG=
THRONE_PID=
PROFILE_ID=

in_connects() { grep -cE "$CONNECT_RE" "$FIXTURE_LOG" 2>/dev/null || true; }

in_connects_above() { (($(in_connects) > $1)); }

in_alive() { kill -0 "$THRONE_PID" 2>/dev/null; }

in_gone() { ! in_alive; }

in_no_core() { ! pgrep -x ThroneCore >/dev/null; }

in_healthy() {
    in_alive || fail "Throne (PID $THRONE_PID) exited before it was healthy"
    "$THRONE" --health --data-dir "$DATA" >/dev/null 2>&1
}

in_state_is() {
    local out
    out=$("$THRONECTL" --json status 2>/dev/null) || return 1
    [ "$(pk_json state <<<"$out")" = "$1" ]
}

in_cleanup() {
    local rc=$1 file
    [ -n "$RUN_LOG" ] || return 0
    [ -f "$FILE_LOG" ] && cp "$FILE_LOG" "$E2E_TMP/throne.log"
    if ((rc != 0)); then
        echo "::group::FAILED (status $rc): Throne's stderr, throne.log, fixtures"
        for file in "$RUN_LOG" "$FIXTURE_LOG" "$FIXTURE_LOG.stderr"; do
            [ -f "$file" ] && echo "--- $file" && cat "$file"
        done
        if [ -f "$FILE_LOG" ]; then
            echo "--- tail of $FILE_LOG"
            tail -n 200 "$FILE_LOG"
        fi
        echo "::endgroup::"
    fi
    if [ -n "$THRONE_PID" ] && in_alive; then
        kill -TERM "$THRONE_PID"
        for _ in $(seq 40); do
            in_alive || break
            sleep 0.5
        done
        kill -KILL "$THRONE_PID" 2>/dev/null
    fi
    pkill -KILL -x ThroneCore 2>/dev/null
    chmod -R a+rX "$E2E_TMP" 2>/dev/null
    return 0
}

inside_main() {
    local out address port before code rc mode
    [ "${E2E_INSIDE:-}" = 1 ] && [ -f /.dockerenv ] || fail "the inside part runs only in the container that the host part starts"
    unset DISPLAY WAYLAND_DISPLAY QT_QPA_PLATFORM
    mkdir -p "$E2E_TMP"
    RUN_LOG=$E2E_TMP/throne-stderr.log
    FIXTURE_LOG=$E2E_TMP/fixtures.log

    step "L3: apt-get install the arm64 system-qt deb"
    apt-get update -qq
    apt-get install -y --no-install-recommends /pkg/throne-system-qt.deb
    [ "$(dpkg-query -W -f='${Status}' throne)" = "install ok installed" ] || fail "throne is not installed after apt-get install"
    out=$(ldd "$THRONE" /opt/Throne/ThroneCore 2>&1) || true
    if grep -F 'not found' <<<"$out"; then
        fail "Throne does not load on bookworm (lines above): the system-qt package misses a dependency"
    fi
    "$THRONE" --headless --version
    # Test tools only after the package, so its own Depends had to pull in everything Throne loads.
    apt-get install -y --no-install-recommends python3 curl procps ca-certificates

    step "L3: fixtures (HTTP $HTTP_PORT, SOCKS5 $SOCKS_PORT)"
    start_fixtures 127.0.0.1 "$FIXTURE_LOG"
    code=$(curl -sS --max-time 10 -x "socks5h://127.0.0.1:$SOCKS_PORT" -o /dev/null -w '%{http_code}' "$FIXTURE_URL") ||
        fail "the fixtures do not answer each other"
    pk_eq "fixture self-test HTTP status" "$code" 204
    (($(in_connects) > 0)) || fail "the SOCKS5 fixture did not log its own CONNECT"

    step "L3: Throne --headless in the foreground, health 0"
    "$THRONE" --headless --data-dir "$DATA" --api-listen "$API_LISTEN" --log-format plain >"$RUN_LOG" 2>&1 &
    THRONE_PID=$!
    echo "Throne PID $THRONE_PID, data dir $DATA, stderr $RUN_LOG"
    wait_until 90 "Throne --health exiting 0" in_healthy
    out=$("$THRONE" --health --data-dir "$DATA") || fail "Throne --health exited $?: $out"
    echo "$out"
    pk_eq "--health ok" "$(pk_json ok <<<"$out")" true
    pk_eq "--health ready" "$(pk_json ready <<<"$out")" true
    pk_eq "--health api" "$(pk_json api <<<"$out")" listening
    pk_eq "--health headless_dialogs" "$(pk_json headless_dialogs <<<"$out")" 0
    pk_eq "--health previous_crash" "$(pk_json previous_crash <<<"$out")" false
    pk_eq "$DATA mode" "$(stat -c '%a' "$DATA")" 700
    # Under docker's umask 022 and a single start, only Throne's own create mode keeps the key from others.
    mode=$(stat -c '%a' "$DATA/config/api.key") || fail "no $DATA/config/api.key"
    (((8#$mode & 8#077) == 0)) || fail "$DATA/config/api.key has mode $mode: Throne created the API key readable by others"

    step "L3: --print-connection and thronectl status"
    connect_env "$THRONE" "$DATA"
    pk_eq THRONE_ADDR "${THRONE_ADDR:-}" "$API_LISTEN"
    pk_eq THRONE_KEY_FILE "${THRONE_KEY_FILE:-}" "$DATA/config/api.key"
    out=$("$THRONECTL" --json status) || fail "thronectl status failed"
    echo "$out"
    pk_eq capabilities.headless "$(pk_json capabilities.headless <<<"$out")" true
    pk_eq state "$(pk_json state <<<"$out")" idle

    step "L3: import a SOCKS profile, connect, proxy through the mixed inbound, disconnect"
    out=$(printf 'socks5://127.0.0.1:%s#%s\n' "$SOCKS_PORT" "$PROFILE_NAME" | "$THRONECTL" --json import -) ||
        fail "thronectl import failed"
    echo "$out"
    pk_eq imported "$(pk_json imported <<<"$out")" 1
    PROFILE_ID=$(pk_json profile_ids.0 <<<"$out")
    [[ $PROFILE_ID =~ ^[0-9]+$ ]] || fail "profile_ids.0 is '$PROFILE_ID'"
    "$THRONECTL" --timeout 90s connect "$PROFILE_ID"
    out=$("$THRONECTL" --json status) || fail "thronectl status failed"
    echo "$out"
    pk_eq state "$(pk_json state <<<"$out")" running
    pk_eq profile.id "$(pk_json profile.id <<<"$out")" "$PROFILE_ID"
    pk_eq inbound.enabled "$(pk_json inbound.enabled <<<"$out")" true
    address=$(pk_json inbound.address <<<"$out")
    port=$(pk_json inbound.port <<<"$out")
    [[ $port =~ ^[0-9]+$ ]] || fail "inbound.port is '$port'"
    case $address in
        "" | 0.0.0.0 | "<missing>") address=127.0.0.1 ;;
        ::) address='[::1]' ;;
        *:*) address="[$address]" ;;
    esac
    before=$(in_connects)
    code=$(curl -sS --max-time 20 -x "socks5h://$address:$port" -o /dev/null -w '%{http_code}' "$FIXTURE_URL") ||
        fail "curl through Throne's mixed inbound $address:$port failed"
    pk_eq "HTTP status through Throne" "$code" 204
    wait_until 5 "the SOCKS5 fixture logging the proxied CONNECT" in_connects_above "$before"
    "$THRONECTL" --timeout 60s disconnect
    wait_until 15 "state idle after thronectl disconnect" in_state_is idle

    step "L3: SIGTERM: exit 0, clean session end"
    kill -TERM "$THRONE_PID"
    wait_until 45 "Throne exiting after SIGTERM" in_gone
    rc=0
    wait "$THRONE_PID" || rc=$?
    THRONE_PID=
    pk_eq "exit status after SIGTERM" "$rc" 0
    # Sanity only: a core Throne leaked exits by itself once its IPC connection closes; the log line below is the check.
    wait_until 10 "no ThroneCore process left" in_no_core
    [ -f "$FILE_LOG" ] || fail "no $FILE_LOG"
    grep -qE 'core process exited: code=-?[0-9]+ status=(normal|crash) \(during shutdown\)' "$FILE_LOG" ||
        fail "$FILE_LOG has no 'core process exited ... (during shutdown)' line: Throne did not stop and reap its core"
    grep -qF 'EXIT 0: SIGTERM' "$FILE_LOG" || fail "$FILE_LOG does not log 'EXIT 0: SIGTERM'"
    grep -qF '===== Throne session end (clean) =====' "$FILE_LOG" || fail "$FILE_LOG has no clean session end"
    [ ! -e "$DATA/config/logs/running.marker" ] || fail "running.marker was left behind"
    assert_log_clean "$RUN_LOG" "$FILE_LOG"
    echo "L3 system-qt scenario passed"
}

# Called by lib.sh's EXIT trap, which then stops the fixtures.
e2e_on_exit() {
    if [ "$MODE" = inside ]; then
        in_cleanup "$1"
    else
        host_cleanup
    fi
}

MODE=host
if [ "${1:-}" = inside ]; then
    MODE=inside
    inside_main
else
    host_main
fi
