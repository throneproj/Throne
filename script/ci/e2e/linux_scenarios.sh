#!/usr/bin/env bash
# L2 of the headless end-to-end suite: Linux scenarios, each in a fresh container with a fresh data dir, so Tun and the
# kill switch act on the container's own network namespace and never on the runner. A user network 198.18.0.0/24
# (outside the kill switch's LAN set) holds the fixture container (198.18.0.10; per scenario an HTTP 204 server and a
# SOCKS5 proxy that logs every CONNECT, on ports of their own) and one Throne container per scenario (198.18.0.20+N),
# built from ubuntu:24.04 with the bundled deb's runtime libraries, the deployment dir mounted read-only at /opt/Throne.
# Throne runs in the foreground of a shell inside the container (parent PID > 1, as from a terminal); thronectl runs
# inside the container after eval "$(Throne --print-connection --data-dir ...)". Every scenario runs and is reported;
# the script exits 1 when one failed.
#
# Scenarios: 1 cli, 2 signals, 3 proxy, 4 tun, 5 kill_switch, 6 identity, 7 stay_up, 8 resume, 9 tun_conflict,
# 10 no_kill_switch, 11 image.
#
# Env:
#   THRONE_DIR     unpacked linux-amd64 deployment dir (Throne, ThroneCore, usr/lib), mounted read-only at /opt/Throne
#   THRONECTL      thronectl for linux/amd64 (CGO_ENABLED=0), mounted at /usr/local/bin/thronectl in every container
#   FIXTURES       script/ci/smoke_fixtures.py (default: the one next to this directory)
#   E2E_TMP        scratch dir (default $RUNNER_TEMP/throne-e2e). This script works in $E2E_TMP/linux-scenarios:
#                  logs/<scenario>/ holds each scenario's Throne output, data-dir logs and command output, fixture/ the
#                  fixture logs; upload that dir when the job fails
#   E2E_IMAGE      tag of the image the job built from docker/Dockerfile, loaded into this runner's docker, for scenario
#                  11; unset skips scenario 11 with a notice
#   E2E_SCENARIOS  optional: the scenario names to run, separated by spaces (default: all, in the order above)
# Needs docker, python3 and curl on the runner, and passwordless sudo for one command: modprobe dummy, the module of the
# dummy interface that tun_conflict creates inside its container.
set -Eeuo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=lib.sh
source "$SCRIPT_DIR/lib.sh"

THRONE_DIR=${THRONE_DIR:?set THRONE_DIR to the unpacked linux-amd64 deployment dir}
THRONECTL=${THRONECTL:?set THRONECTL to a linux/amd64 thronectl}
: "${E2E_TMP:=${RUNNER_TEMP:-/tmp}/throne-e2e}"
# Made absolute by _setup: bind mounts need absolute paths.
WORK=

TEST_IMAGE=throne-e2e-runtime:ubuntu-24.04
RUN_ID=${GITHUB_RUN_ID:-local}-${GITHUB_RUN_ATTEMPT:-1}-$$
PREFIX=throne-e2e-$RUN_ID
LABEL=throne-e2e=$RUN_ID
NET=$PREFIX
FIX=$PREFIX-fixture
FIX_IP=198.18.0.10
FIX_IP_RE='198\.18\.0\.10'
# Off every subnet of the container (TEST-NET-3): only the tunnel and the profile reach it, through the fixture's alias.
TUN_TARGET_IP=203.0.113.10
TUN_TARGET_RE='203\.0\.113\.10'
# Inside each Throne container; a fresh container is a fresh data dir.
D=/var/lib/throne-e2e
TUN_FLAGS=(--cap-add NET_ADMIN --device /dev/net/tun)
ALL_SCENARIOS=(cli signals proxy tun kill_switch identity stay_up resume tun_conflict no_kill_switch image)
SKIPPED=200
RESULTS=()

# ---------------------------------------------------------------------------------------------------- helpers

_step() { step "L2 $SC: $1"; }

_expect_eq() {
    [ "$2" = "$3" ] || fail "$1 is '$2', want '$3'"
}

# _jget PATH: the value at a dotted PATH (list indexes as numbers) of the JSON on stdin; strings raw, the rest as JSON.
_jget() {
    python3 -I -c '
import json, sys
value = json.load(sys.stdin)
for part in filter(None, sys.argv[1].split(".")):
    value = value[int(part)] if isinstance(value, list) else value[part]
print(value if isinstance(value, str) else json.dumps(value))
' "$1"
}

_show_tail() {
    if [ -f "$1" ]; then
        echo "----- last lines of $1"
        tail -n 80 "$1"
    fi
}

_lines() {
    if [ -f "$1" ]; then
        wc -l <"$1" | tr -d ' '
    else
        echo 0
    fi
}

# _log_has FILE TEXT, FILE relative to the scenario's log dir.
_log_has() {
    grep -qF -- "$2" "$LOGDIR/$1" || fail "$1 has no line with '$2'"
}

_log_q() { grep -qF -- "$2" "$LOGDIR/$1" 2>/dev/null; }

_log_count() {
    local n
    n=$(grep -cF -- "$2" "$LOGDIR/$1" 2>/dev/null) || true
    echo "${n:-0}"
}

# The boot line "Throne <version> headless, PID <n>, started by ..." opens every boot, an exec'd image's too.
_boot_lines() { _log_count "$1.log" " headless, PID "; }

_boot_lines_at_least() { (($(_boot_lines "$1") >= $2)); }

_throne_container() {
    C=$PREFIX-$SC
    C_IP=198.18.0.$((20 + SC_INDEX))
    docker run -d --init --name "$C" --hostname "e2e-${SC//_/-}" --label "$LABEL" \
        --network "$NET" --ip "$C_IP" -e LANG=C.UTF-8 \
        --mount "type=bind,src=$THRONE_DIR,dst=/opt/Throne,readonly" \
        --mount "type=bind,src=$THRONECTL,dst=/usr/local/bin/thronectl,readonly" \
        --mount "type=bind,src=$WORK/bin,dst=/e2e-bin,readonly" \
        --mount "type=bind,src=$LOGDIR,dst=/e2e" \
        "$@" "$TEST_IMAGE" sleep infinity >/dev/null || fail "docker run of $C failed"
    echo "container $C at $C_IP, flags: ${*:-none}"
}

# thronectl inside the Throne container, connected the documented way.
_CTL_SH='d=$1; shift; c=$(/opt/Throne/Throne --print-connection --data-dir "$d") || { echo "Throne --print-connection failed" >&2; exit 97; }; eval "$c"; exec thronectl "$@"'

_ctl() { docker exec "$C" sh -c "$_CTL_SH" sh "$D" "$@"; }

_ctl_stdin() { docker exec -i "$C" sh -c "$_CTL_SH" sh "$D" "$@"; }

# thronectl in the fixture container, over the network, with the key copied out of the Throne container.
_fix_ctl() { docker exec "$FIX" thronectl --addr "$C_IP:9095" --key-file "/fx/$SC.api.key" "$@"; }

_status() {
    STATUS=$(_ctl --json status) || fail "thronectl status failed"
    echo "status: $STATUS"
}

_sfield() { _jget "$1" <<<"$STATUS"; }

_status_is() {
    local out
    out=$(_ctl --json status 2>/dev/null) || return 1
    [ "$(_jget "$1" <<<"$out" 2>/dev/null)" = "$2" ]
}

_health_now() {
    HEALTH_RC=0
    HEALTH=$(docker exec "$C" /opt/Throne/Throne --health --data-dir "$D" 2>>"$LOGDIR/client.stderr") || HEALTH_RC=$?
    echo "--health exit $HEALTH_RC: $HEALTH"
}

_health_degraded() {
    local out
    out=$(docker exec "$C" /opt/Throne/Throne --health --data-dir "$D" 2>/dev/null) || true
    [ -n "$out" ] || return 1
    [ "$(_jget ready <<<"$out" 2>/dev/null)" = true ] && [ "$(_jget degraded <<<"$out" 2>/dev/null)" = "$1" ]
}

# ---------------------------------------------------------------------------------------------------- fixtures

_fixture_start() {
    local before code
    HTTP_PORT=$((18100 + SC_INDEX))
    SOCKS_PORT=$((11100 + SC_INDEX))
    FIX_LOG=$WORK/fixture/$SC.log
    _step "fixtures on $FIX_IP (HTTP $HTTP_PORT, SOCKS5 $SOCKS_PORT)"
    docker exec -d "$FIX" sh -c 'exec python3 -I /fx-src/smoke_fixtures.py --listen 0.0.0.0 --http-port "$1" --socks-port "$2" --log "/fx/$3.log" --ready-file "/fx/$3.ready" --alias "$4" 2>"/fx/$3.stderr"' \
        sh "$HTTP_PORT" "$SOCKS_PORT" "$SC" "$TUN_TARGET_IP"
    wait_until 15 "the $SC fixtures listening" _fixture_ready
    before=$(_connects)
    code=$(curl -sS --max-time 10 -x "socks5h://$FIX_IP:$SOCKS_PORT" -o /dev/null -w '%{http_code}' "http://$FIX_IP:$HTTP_PORT/") ||
        fail "the runner cannot use the $SC fixtures at $FIX_IP"
    _expect_eq "fixture self-test HTTP status" "$code" 204
    wait_until 5 "the SOCKS5 fixture logging its self-test CONNECT" _connects_above "$before"
}

_fixture_ready() {
    if grep -qs 'cannot listen' "$WORK/fixture/$SC.stderr"; then
        fail "the $SC fixtures cannot listen: $(cat "$WORK/fixture/$SC.stderr")"
    fi
    [ -s "$WORK/fixture/$SC.ready" ]
}

_fixture_stop() {
    local pid
    pid=$(head -n 1 "$WORK/fixture/${SC:-none}.ready" 2>/dev/null) || return 0
    if [ -n "$pid" ]; then
        docker exec "$FIX" kill "$pid" >/dev/null 2>&1 || true
    fi
}

# CONNECTs to this scenario's HTTP fixture (as addressed by $1, default its own address) that the SOCKS5 fixture relayed.
_connects() {
    local n
    n=$(grep -cE "^CONNECT ${1:-$FIX_IP_RE}:$HTTP_PORT -> [^ ]+ ok\$" "$FIX_LOG" 2>/dev/null) || true
    echo "${n:-0}"
}

_connects_above() { (($(_connects "${2:-}") > $1)); }

# ---------------------------------------------------------------------------------------------------- Throne processes

_write_wrapper() {
    cat >"$WORK/bin/run-throne.sh" <<'EOF'
#!/bin/bash
# run-throne.sh TAG MAX_BOOTS RESTART_ON THRONE [ARGS...], written by linux_scenarios.sh. Runs Throne in the foreground
# of this shell, as from a terminal (its parent is this shell, PID > 1), again while it exits with RESTART_ON, at most
# MAX_BOOTS times. Output goes to /e2e/TAG.log, a "boot N exit STATUS" line per run to /e2e/TAG.boots, the last status
# to /e2e/TAG.rc.
tag=$1 max=$2 again=$3
shift 3
echo "$$" >"/e2e/$tag.shpid"
boot=0
while :; do
    boot=$((boot + 1))
    rc=0
    "$@" >>"/e2e/$tag.log" 2>&1 || rc=$?
    echo "boot $boot exit $rc" >>"/e2e/$tag.boots"
    if [ "$rc" != "$again" ] || [ "$boot" -ge "$max" ]; then
        break
    fi
done
echo "$rc" >"/e2e/$tag.rc.tmp"
mv -f "/e2e/$tag.rc.tmp" "/e2e/$tag.rc"
EOF
    chmod 0755 "$WORK/bin/run-throne.sh"
}

# _launch TAG MAX_BOOTS RESTART_ON BIN [FLAGS...]: BIN --headless --data-dir $D --log-format plain FLAGS under the
# wrapper shell, in the background; sets THRONE_PID.
_launch() {
    local tag=$1 max=$2 again=$3 bin=$4
    shift 4
    rm -f "$LOGDIR/$tag.shpid" "$LOGDIR/$tag.rc" "$LOGDIR/$tag.boots"
    docker exec -d "$C" /e2e-bin/run-throne.sh "$tag" "$max" "$again" \
        "$bin" --headless --data-dir "$D" --log-format plain "$@"
    echo "started in $C under a shell: $bin --headless --data-dir $D --log-format plain $* (output: logs/$SC/$tag.log)"
    THRONE_PID=
    wait_until 15 "Throne ($tag) starting under its shell" _find_pid "$tag"
    echo "Throne ($tag) PID $THRONE_PID"
}

_start() {
    local tag=$1
    shift
    _launch "$tag" 1 - /opt/Throne/Throne "$@"
}

_exited() { [ -s "$LOGDIR/$1.rc" ]; }

_find_pid() {
    local shpid pid
    if _exited "$1"; then
        _show_tail "$LOGDIR/$1.log"
        fail "Throne ($1) exited with status $(cat "$LOGDIR/$1.rc") (log above)"
    fi
    [ -s "$LOGDIR/$1.shpid" ] || return 1
    shpid=$(cat "$LOGDIR/$1.shpid")
    pid=$(docker exec "$C" pgrep -n -P "$shpid" -x Throne 2>/dev/null) || return 1
    [ -n "$pid" ] || return 1
    THRONE_PID=$pid
}

_healthy() {
    if _exited "$1"; then
        _show_tail "$LOGDIR/$1.log"
        fail "Throne ($1) exited with status $(cat "$LOGDIR/$1.rc") before it was healthy (log above)"
    fi
    docker exec "$C" /opt/Throne/Throne --health --data-dir "$D" >/dev/null 2>&1
}

_wait_healthy() {
    wait_until "${2:-90}" "Throne ($1) reporting healthy (--health exits 0)" _healthy "$1"
    _health_now
}

_signal() {
    local pid=${2:-$THRONE_PID}
    echo "kill -$1 $pid"
    docker exec "$C" kill "-$1" "$pid" || fail "kill -$1 $pid failed in $C"
}

_wait_exit() {
    wait_until "${2:-45}" "Throne ($1) exiting" _exited "$1"
    EXIT_RC=$(cat "$LOGDIR/$1.rc")
    echo "Throne ($1) exited with status $EXIT_RC"
}

_no_core() {
    local procs
    procs=$(docker exec "$C" ps -eo stat=,comm=) || return 1
    ! awk '$2 == "ThroneCore" && $1 !~ /^Z/ { found = 1 } END { exit !found }' <<<"$procs"
}

# _assert_clean_end TAG EXIT_LINE: Throne logged EXIT_LINE ("EXIT 0: SIGTERM"), ended its session cleanly, left no core.
_assert_clean_end() {
    _log_has "$1.log" "$2"
    docker exec "$C" grep -qF '===== Throne session end (clean) =====' "$D/config/logs/throne.log" ||
        fail "$D/config/logs/throne.log has no clean session end after Throne ($1) exited"
    if docker exec "$C" test -e "$D/config/logs/running.marker"; then
        fail "running.marker was left behind after Throne ($1) exited"
    fi
    wait_until 15 "no ThroneCore left in $C" _no_core
}

_stop() {
    _signal TERM
    _wait_exit "$1" 45
    _expect_eq "exit status of Throne ($1) after SIGTERM" "$EXIT_RC" 0
    _assert_clean_end "$1" "EXIT 0: SIGTERM"
}

_kill9() {
    _signal KILL
    _wait_exit "$1" 15
    _expect_eq "exit status of Throne ($1) after kill -9" "$EXIT_RC" 137
    wait_until 30 "the killed Throne's ThroneCore exiting (on the end of its IPC connection)" _no_core
}

# _expect_exit WANT LABEL CMD...: CMD in the Throne container, its output kept as logs/<scenario>/cli-LABEL.log.
_expect_exit() {
    local want=$1 label=$2 rc=0
    shift 2
    echo "+ $*"
    docker exec "$C" "$@" >"$LOGDIR/cli-$label.log" 2>&1 || rc=$?
    cat "$LOGDIR/cli-$label.log"
    _expect_eq "exit status of '$*'" "$rc" "$want"
}

# _expect_ctl_error CODE THRONECTL_ARGS...: thronectl exits 1 with "error: CODE: ..."; the output lands in CTL_ERROR.
_expect_ctl_error() {
    local code=$1 rc=0
    shift
    CTL_ERROR=$(_ctl "$@" 2>&1) || rc=$?
    echo "thronectl $*: exit $rc: $CTL_ERROR"
    _expect_eq "exit status of thronectl $*" "$rc" 1
    [[ $CTL_ERROR == *"error: $code:"* ]] || fail "thronectl $* did not fail with $code: $CTL_ERROR"
}

# ---------------------------------------------------------------------------------------------------- traffic

_import_profile() {
    local out
    out=$(printf 'socks5://%s:%s#e2e-%s\n' "$FIX_IP" "$SOCKS_PORT" "$SC" | _ctl_stdin --json import -) ||
        fail "thronectl import of the fixture's SOCKS5 profile failed"
    echo "import: $out"
    _expect_eq "imported profiles" "$(_jget imported <<<"$out")" 1
    PROFILE_ID=$(_jget profile_ids.0 <<<"$out")
    [[ $PROFILE_ID =~ ^[0-9]+$ ]] || fail "profile_ids.0 is '$PROFILE_ID'"
}

_connect() {
    _ctl --timeout 90s connect "$PROFILE_ID" || fail "thronectl connect $PROFILE_ID failed"
    wait_until 30 "profile $PROFILE_ID running" _status_is state running
}

_disconnect() {
    _ctl --timeout 60s disconnect || fail "thronectl disconnect failed"
    wait_until 30 "Throne idle" _status_is state idle
}

_assert_proxied() {
    local address port before code
    _status
    address=$(_sfield inbound.address)
    port=$(_sfield inbound.port)
    case $address in
        "" | 0.0.0.0 | ::) address=127.0.0.1 ;;
    esac
    before=$(_connects)
    code=$(docker exec "$C" curl -sS --max-time 20 -x "socks5h://$address:$port" -o /dev/null -w '%{http_code}' \
        "http://$FIX_IP:$HTTP_PORT/") || fail "curl through Throne's mixed inbound $address:$port failed"
    _expect_eq "HTTP status through Throne's mixed inbound" "$code" 204
    wait_until 5 "the SOCKS5 fixture logging the CONNECT of the proxied request" _connects_above "$before"
}

_curl_tunnelled() {
    docker exec "$C" curl -sS --noproxy '*' --connect-timeout 5 --max-time 10 -o /dev/null -w '%{http_code}' \
        "http://$TUN_TARGET_IP:$HTTP_PORT/"
}

# A request from the Throne container to the fixture without any proxy setting; prints the HTTP status.
_curl_direct() {
    local wait=${1:-5}
    docker exec "$C" curl -sS --noproxy '*' --connect-timeout "$wait" --max-time $((wait + 5)) -o /dev/null \
        -w '%{http_code}' "http://$FIX_IP:$HTTP_PORT/"
}

# The SOCKS5 fixture logs a CONNECT before it relays a byte, so a proxied request would have logged one by now.
_assert_direct_ok_unproxied() {
    local before code
    before=$(_connects)
    code=$(_curl_direct) || fail "a direct request from $C to $FIX_IP:$HTTP_PORT failed"
    _expect_eq "HTTP status of a direct request" "$code" 204
    _expect_eq "SOCKS5 CONNECTs after a direct request" "$(_connects)" "$before"
}

_direct_ok() {
    local code
    code=$(_curl_direct 2 2>/dev/null) || return 1
    [ "$code" = 204 ]
}

_assert_direct_blocked() {
    local rc=0 out
    out=$(_curl_direct 3 2>&1) || rc=$?
    ((rc != 0)) || fail "a direct request from $C to $FIX_IP:$HTTP_PORT got HTTP $out through the armed kill switch"
    echo "the direct request was blocked (curl status $rc: $out)"
}

# ---------------------------------------------------------------------------------------------------- scenarios

scenario_cli() {
    _step "container"
    _throne_container "${TUN_FLAGS[@]}"

    _step "an unknown option exits 64"
    _expect_exit 64 unknown-option /opt/Throne/Throne --headless --data-dir "$D" --no-such-option

    _step "--data-dir naming a file exits 78"
    docker exec "$C" touch /tmp/a-file
    _expect_exit 78 data-dir-file /opt/Throne/Throne --headless --data-dir /tmp/a-file
    _log_has cli-data-dir-file.log "exists and is not a directory"

    _step "a missing --api-key-file exits 78"
    _expect_exit 78 missing-key-file /opt/Throne/Throne --headless --data-dir /var/lib/throne-e2e-key \
        --api-key-file /nonexistent/api.key
    _log_has cli-missing-key-file.log "is not usable"

    _step "--health with nothing running exits 2"
    _expect_exit 2 health-idle /opt/Throne/Throne --health --data-dir "$D"
    _log_has cli-health-idle.log "No headless Throne answered"

    _step "--check exits 0"
    _expect_exit 0 check /opt/Throne/Throne --check --data-dir "$D"
    _log_has cli-check.log "result: 0 failed"

    _step "a second instance on a busy data dir exits 3"
    _start first
    _wait_healthy first
    _expect_exit 3 second-instance /opt/Throne/Throne --headless --data-dir "$D" --log-format plain
    _log_has cli-second-instance.log "is in use by"
    _healthy first || fail "the first Throne is no longer healthy after the second one was refused"

    _step "SIGTERM"
    _stop first
}

scenario_signals() {
    local pid
    _throne_container "${TUN_FLAGS[@]}"
    _fixture_start

    _step "SIGTERM exits 0 with a clean session end"
    _start term
    _wait_healthy term
    _expect_eq "parent PID of Throne" "$(docker exec "$C" ps -o ppid= -p "$THRONE_PID" | tr -d ' ')" \
        "$(cat "$LOGDIR/term.shpid")"
    # The parent's comm is the wrapper script's name, so only the mode and its kind of reason are fixed.
    _log_has term.log "restart mode exec (launched by "
    _stop term

    _step "SIGHUP restarts in place: same PID, new session"
    _start hup
    _wait_healthy hup
    pid=$THRONE_PID
    _expect_eq "boot lines before SIGHUP" "$(_boot_lines hup)" 1
    _signal HUP
    wait_until 60 "the exec'd image's boot line" _boot_lines_at_least hup 2
    _wait_healthy hup
    _find_pid hup || fail "no Throne under the hup shell after SIGHUP"
    _expect_eq "PID after the SIGHUP restart" "$THRONE_PID" "$pid"
    _expect_eq "previous_crash after an exec restart" "$(_jget previous_crash <<<"$HEALTH")" false
    _log_has hup.log "EXIT 75: SIGHUP"
    _log_has hup.log "restarting in place: /opt/Throne/Throne"
    _log_has hup.log "restart mode exec (inherited from the previous image)"
    # Logging::Init truncates throne.log, so it holds only the new image's session.
    docker exec "$C" grep -qF "pid       : $pid" "$D/config/logs/throne.log" ||
        fail "throne.log has no session header for PID $pid after the restart"
    docker exec "$C" grep -qF "restart mode exec (inherited from the previous image)" "$D/config/logs/throne.log" ||
        fail "throne.log is not the session of the exec'd image"
    _stop hup

    _step "--restart-mode exit: SIGHUP exits 75"
    _start exit-mode --restart-mode exit
    _wait_healthy exit-mode
    _log_has exit-mode.log "restart mode exit (requested)"
    _signal HUP
    _wait_exit exit-mode 45
    _expect_eq "exit status after SIGHUP with --restart-mode exit" "$EXIT_RC" 75
    _assert_clean_end exit-mode "EXIT 75: SIGHUP"

    # A frozen core keeps the teardown in its Stop call (10 s in headless), so the second SIGINT lands during it.
    _step "a second SIGINT during the teardown kills the core"
    _start int --no-resume
    _wait_healthy int
    _import_profile
    _connect
    CORE_PID=$(docker exec "$C" pgrep -n -P "$THRONE_PID" -x ThroneCore) || fail "no ThroneCore child of Throne $THRONE_PID"
    _signal STOP "$CORE_PID"
    _signal INT
    wait_until 15 "Throne logging 'EXIT 0: SIGINT' (its teardown has begun)" _log_q int.log "EXIT 0: SIGINT"
    _signal INT
    # Without the fast path the frozen core lives until the Stop timeout and the teardown's SIGKILL, about 10 s later.
    wait_until 4 "the core killed by the second SIGINT (well before the 10 s Stop timeout)" _no_core
    _wait_exit int 40
    _expect_eq "exit status after two SIGINTs" "$EXIT_RC" 0
    _log_has int.log "SIGINT: second signal: killing the core"
    _assert_clean_end int "EXIT 0: SIGINT"
}

scenario_proxy() {
    local mode
    _throne_container "${TUN_FLAGS[@]}"
    _fixture_start

    _step "start"
    _start main
    _wait_healthy main
    # docker exec's umask is 022, so only Throne's own create mode keeps the key from others.
    mode=$(docker exec "$C" stat -c '%a' "$D/config/api.key") || fail "no $D/config/api.key in $C"
    (((8#$mode & 8#077) == 0)) || fail "$D/config/api.key has mode $mode: Throne created the API key readable by others"
    _status
    _expect_eq "capabilities.headless" "$(_sfield capabilities.headless)" true
    _expect_eq state "$(_sfield state)" idle

    _step "import a SOCKS5 profile pointing at the fixture, connect"
    _import_profile
    _connect

    _step "curl through the mixed inbound reaches the fixture through the profile"
    _assert_proxied

    _step "disconnect, SIGTERM"
    _disconnect
    _stop main
}

scenario_tun() {
    _throne_container "${TUN_FLAGS[@]}"
    _fixture_start

    _step "start"
    _start main
    _wait_healthy main
    wait_until 30 "capabilities.tun true (the core's privilege check)" _status_is capabilities.tun true
    _import_profile

    _step "without Tun a direct request bypasses Throne"
    _assert_direct_ok_unproxied

    _step "tun on, connect"
    _ctl --timeout 60s tun on || fail "thronectl tun on failed"
    _connect
    _status
    _expect_eq tun "$(_sfield tun)" true
    wait_until 30 "the throne-tun interface in $C" _tun_up

    _step "a direct request goes through the tunnel and the profile"
    local before code
    before=$(_connects "$TUN_TARGET_RE")
    # The fixture is on the container's own subnet, which sing-tun leaves out of the tunnel; target an address off it.
    code=$(_curl_tunnelled) || fail "a request to $TUN_TARGET_IP from $C failed with Tun on"
    _expect_eq "HTTP status of a request to $TUN_TARGET_IP with Tun on" "$code" 204
    wait_until 5 "the SOCKS5 fixture logging the CONNECT of the tunnelled request" \
        _connects_above "$before" "$TUN_TARGET_RE"

    _step "tun off: direct requests bypass Throne again"
    _ctl --timeout 90s tun off || fail "thronectl tun off failed"
    wait_until 30 "tun false in the status" _status_is tun false
    wait_until 30 "throne-tun gone from $C" _no_tun_if
    _assert_direct_ok_unproxied

    _step "disconnect, SIGTERM"
    _disconnect
    _stop main
}

_tun_up() { docker exec "$C" ip link show throne-tun >/dev/null 2>&1; }

_no_tun_if() { ! _tun_up; }

scenario_kill_switch() {
    local out
    _throne_container "${TUN_FLAGS[@]}"
    _fixture_start

    _step "start with --api-listen 0.0.0.0:9095 --api-admin-lan"
    _start main --api-listen 0.0.0.0:9095 --api-admin-lan
    _wait_healthy main
    grep -qE '\[API\] Listening on (0\.0\.0\.0|\*):9095' "$LOGDIR/main.log" || fail "main.log has no '[API] Listening on 0.0.0.0:9095'"
    _import_profile
    _assert_direct_ok_unproxied
    docker cp "$C:$D/config/api.key" "$WORK/fixture/$SC.api.key" >/dev/null || fail "docker cp of api.key out of $C failed"
    out=$(_fix_ctl --json status) || fail "thronectl in the fixture container cannot reach $C_IP:9095 even without the kill switch"

    _step "kill_switch=true arms the kill switch"
    _ctl settings set kill_switch=true || fail "thronectl settings set kill_switch=true failed"
    wait_until 60 "kill_switch armed in the status" _status_is kill_switch armed
    # The running core lacks the kill switch's group until Throne restarts it (ConnectionManager::restartCoreForGuard).
    wait_until 30 "Throne restarting its core into the kill switch's group" \
        _log_q main.log "[Kill switch] Restarting the core so that its own traffic passes the kill switch"
    _wait_healthy main 60

    _step "idle: a direct request from the Throne container is blocked"
    _assert_direct_blocked

    _step "the API's port still answers the fixture container"
    out=$(_fix_ctl --json status) || fail "thronectl in the fixture container lost $C_IP:9095 to the kill switch"
    _expect_eq "kill_switch seen from the fixture container" "$(_jget kill_switch <<<"$out")" armed

    _step "a connected profile carries traffic; direct requests stay blocked"
    _connect
    _assert_proxied
    _assert_direct_blocked

    _step "kill_switch=false from the fixture container (an admin request from the LAN)"
    _fix_ctl settings set kill_switch=false || fail "thronectl settings set kill_switch=false from the fixture container failed"
    wait_until 30 "kill_switch off in the status" _status_is kill_switch off
    # The status turns off at once; the guard removes its rules when its input closes, just after.
    wait_until 15 "direct requests from $C passing again" _direct_ok
    _assert_direct_ok_unproxied

    _step "disconnect, SIGTERM"
    _disconnect
    _stop main
}

_identity_failed() {
    local out
    out=$(_ctl --json status 2>/dev/null) || return 1
    [ "$(_jget kill_switch <<<"$out" 2>/dev/null)" = failed ] &&
        [ "$(_jget capabilities.kill_switch <<<"$out" 2>/dev/null)" = false ]
}

_assert_identity_state() {
    _health_now
    _expect_eq "--health exit status (a kill switch that cannot work here leaves ok true)" "$HEALTH_RC" 0
    _expect_eq "--health degraded" "$(_jget degraded <<<"$HEALTH")" kill_switch_failed
    _status
    _expect_eq "status kill_switch" "$(_sfield kill_switch)" failed
    _expect_eq "status capabilities.kill_switch (false with the code unsupported)" "$(_sfield capabilities.kill_switch)" false
    _expect_eq "status kill_switch_error" "$(_sfield kill_switch_error)" unsupported
    _expect_eq "status degraded" "$(_sfield degraded)" kill_switch_failed
    _expect_ctl_error kill_switch_inactive --timeout 60s connect "$PROFILE_ID"
    [[ $CTL_ERROR == *1950896719* ]] || fail "kill_switch_inactive does not name the kill switch's group 1950896719: $CTL_ERROR"
}

scenario_identity() {
    local pid lines
    _throne_container "${TUN_FLAGS[@]}" --cap-drop SETGID
    _fixture_start

    _step "start without CAP_SETGID"
    _start main
    _wait_healthy main
    _import_profile

    _step "kill_switch=true: the core cannot adopt the kill switch's group (exit 86); Throne stays up"
    _ctl settings set kill_switch=true || fail "thronectl settings set kill_switch=true failed"
    wait_until 60 "kill_switch failed with capabilities.kill_switch false" _identity_failed
    _log_has main.log "[Core] The core cannot adopt the kill-switch group"
    _log_has main.log "[Kill switch] Profiles refused (unsupported):"
    _wait_healthy main 60
    _assert_identity_state

    _step "thronectl restart: the same outcome after a fresh core start"
    pid=$THRONE_PID
    lines=$(_boot_lines main)
    _ctl restart || fail "thronectl restart failed"
    wait_until 60 "the restarted image's boot line" _boot_lines_at_least main $((lines + 1))
    _wait_healthy main 90
    _find_pid main || fail "no Throne under the main shell after the restart"
    _expect_eq "PID after thronectl restart (exec in place)" "$THRONE_PID" "$pid"
    wait_until 60 "kill_switch failed with capabilities.kill_switch false after the restart" _identity_failed
    (($(_log_count main.log "[Core] The core cannot adopt the kill-switch group") >= 2)) ||
        fail "the restarted Throne did not log the core's identity exit again"
    _assert_identity_state

    _step "kill_switch=false: a profile connects again"
    _ctl settings set kill_switch=false || fail "thronectl settings set kill_switch=false failed"
    wait_until 30 "kill_switch off in the status" _status_is kill_switch off
    _connect
    _assert_proxied

    _step "disconnect, SIGTERM"
    _disconnect
    _stop main
}

scenario_stay_up() {
    local pid
    _throne_container "${TUN_FLAGS[@]}"

    _step "a writable copy of the deployment whose ThroneCore exits 1"
    docker exec "$C" sh -c 'mkdir /opt/throne-rw && tar -C /opt/Throne --exclude=Throne.debug -cf - . | tar -C /opt/throne-rw -xf - &&
        mv /opt/throne-rw/ThroneCore /opt/throne-rw/ThroneCore.real &&
        printf "#!/bin/sh\nexit 1\n" >/opt/throne-rw/ThroneCore && chmod 0755 /opt/throne-rw/ThroneCore' ||
        fail "preparing /opt/throne-rw failed"

    _step "boots 1-3 give up on the core and exit 70"
    _launch boots 5 70 /opt/throne-rw/Throne
    wait_until 180 "three boots ending" _boots_at_least 3
    _expect_eq "the first three boots" "$(head -n 3 "$LOGDIR/boots.boots")" $'boot 1 exit 70\nboot 2 exit 70\nboot 3 exit 70'

    _step "boot 4 stays up: degraded core_gave_up, the API answering"
    wait_until 120 "boot 4 ready with degraded core_gave_up" _health_degraded core_gave_up
    _expect_eq "boots ended so far" "$(_lines "$LOGDIR/boots.boots")" 3
    _find_pid boots || fail "no Throne under the boots shell"
    _health_now
    _expect_eq "--health exit status while the core is given up" "$HEALTH_RC" 1
    _status
    _expect_eq "status degraded" "$(_sfield degraded)" core_gave_up
    _expect_eq "status core" "$(_sfield core)" false
    _log_has boots.log "stays up (degraded core_gave_up)"
    _log_has boots.log "staying up so the API stays reachable"

    _step "the real ThroneCore back, thronectl restart: healthy"
    docker exec "$C" mv -f /opt/throne-rw/ThroneCore.real /opt/throne-rw/ThroneCore || fail "restoring ThroneCore failed"
    pid=$THRONE_PID
    _ctl restart || fail "thronectl restart failed"
    _wait_healthy boots 90
    _find_pid boots || fail "no Throne under the boots shell after the restart"
    _expect_eq "PID after thronectl restart (exec in place)" "$THRONE_PID" "$pid"
    _expect_eq "boots ended so far" "$(_lines "$LOGDIR/boots.boots")" 3
    _status
    _expect_eq "status degraded" "$(_sfield degraded)" null
    _expect_eq "status core" "$(_sfield core)" true

    _step "SIGTERM ends boot 4 with 0"
    _signal TERM
    _wait_exit boots 45
    _expect_eq "exit status" "$EXIT_RC" 0
    _expect_eq "the last boot" "$(sed -n 4p "$LOGDIR/boots.boots")" "boot 4 exit 0"
    _assert_clean_end boots "EXIT 0: SIGTERM"
}

_boots_at_least() {
    if _exited boots; then
        cat "$LOGDIR/boots.boots"
        fail "the boot loop ended early (boots above)"
    fi
    (($(_lines "$LOGDIR/boots.boots") >= $1))
}

_resumed() {
    local out
    out=$(_ctl --json status 2>/dev/null) || return 1
    [ "$(_jget state <<<"$out" 2>/dev/null)" = running ] && [ "$(_jget profile.id <<<"$out" 2>/dev/null)" = "$PROFILE_ID" ]
}

_state_armed() {
    local out
    out=$(docker exec "$C" cat "$D/config/headless-state.json" 2>/dev/null) || return 1
    [ "$(_jget resume.armed <<<"$out" 2>/dev/null)" = true ] && [ "$(_jget resume.profile <<<"$out" 2>/dev/null)" = "$PROFILE_ID" ]
}

scenario_resume() {
    local n connected
    _throne_container "${TUN_FLAGS[@]}"
    _fixture_start

    # A start through the API never arms the guard; only the resumed boot start does.
    _step "boot 1 connects the profile; kill -9"
    _start boot1
    _wait_healthy boot1
    _import_profile
    _connect
    _kill9 boot1

    for n in 2 3 4; do
        _step "boot $n resumes profile $PROFILE_ID; kill -9 within 60 s of it running"
        _start "boot$n"
        wait_until 60 "boot $n running profile $PROFILE_ID again" _resumed
        connected=$SECONDS
        _log_has "boot$n.log" "[Resume] Resuming profile $PROFILE_ID "
        if ((n > 2)); then
            _log_has "boot$n.log" "of resuming profile $PROFILE_ID ($((n - 2)) of 3 in a row)"
        fi
        wait_until 10 "headless-state.json arming profile $PROFILE_ID" _state_armed
        ((SECONDS - connected < 50)) || fail "boot $n ran profile $PROFILE_ID for $((SECONDS - connected)) s before kill -9"
        _kill9 "boot$n"
    done

    _step "boot 5 does not resume: degraded resume_suppressed"
    _start boot5
    wait_until 90 "boot 5 ready with degraded resume_suppressed" _health_degraded resume_suppressed
    _log_has boot5.log "of resuming profile $PROFILE_ID (3 of 3 in a row)"
    _log_has boot5.log "[Resume] not resuming profile $PROFILE_ID: 3 boots in a row"
    _status
    _expect_eq state "$(_sfield state)" idle
    _expect_eq degraded "$(_sfield degraded)" resume_suppressed
    _stop boot5

    _step "--no-resume boots idle"
    _start boot6 --no-resume
    _wait_healthy boot6
    _log_has boot6.log "[Resume] Off (--no-resume or THRONE_RESUME)"
    _status
    _expect_eq state "$(_sfield state)" idle
    _expect_eq degraded "$(_sfield degraded)" null
    _stop boot6
}

_tun_has() {
    local out
    out=$(docker exec "$C" ip -4 -o addr show dev throne-tun 2>/dev/null) || return 1
    grep -qF " $1/" <<<"$out"
}

scenario_tun_conflict() {
    _throne_container "${TUN_FLAGS[@]}"
    _fixture_start

    # Before Throne starts: its Tun check caches an answer for 5 s.
    _step "dummy0 holds 172.19.0.1/24, Throne's default Tun address"
    docker exec "$C" sh -c 'ip link add dummy0 type dummy && ip addr add 172.19.0.1/24 dev dummy0 && ip link set dummy0 up' ||
        fail "creating dummy0 in $C failed (is the dummy module loaded on the runner?)"

    _step "start"
    _start main
    _wait_healthy main
    wait_until 30 "capabilities.tun true (the core's privilege check)" _status_is capabilities.tun true

    _step "tun on fails with tun_conflict naming tun_ipv4_cidr"
    _expect_ctl_error tun_conflict --timeout 60s tun on
    [[ $CTL_ERROR == *tun_ipv4_cidr* ]] || fail "the tun_conflict message does not name tun_ipv4_cidr: $CTL_ERROR"
    _status
    _expect_eq tun "$(_sfield tun)" false
    [[ $(_sfield tun_conflict) == *tun_ipv4_cidr* ]] || fail "status tun_conflict does not name tun_ipv4_cidr: $STATUS"

    _step "tun_ipv4_cidr=172.29.0.1/24, then tun on succeeds"
    _ctl settings set tun_ipv4_cidr=172.29.0.1/24 || fail "thronectl settings set tun_ipv4_cidr failed"
    # Read while Tun is off: once it is on, tun_conflict is null whatever the check says.
    _status
    _expect_eq "tun_conflict after moving tun_ipv4_cidr" "$(_sfield tun_conflict)" null
    _ctl --timeout 60s tun on || fail "thronectl tun on failed after moving tun_ipv4_cidr"
    _status
    _expect_eq tun "$(_sfield tun)" true

    _step "a profile with Tun uses the new range"
    _import_profile
    _connect
    wait_until 30 "throne-tun carrying 172.29.0.1" _tun_has 172.29.0.1

    _step "tun off, disconnect, SIGTERM"
    _ctl --timeout 90s tun off || fail "thronectl tun off failed"
    _disconnect
    _stop main
}

scenario_no_kill_switch() {
    _throne_container "${TUN_FLAGS[@]}"
    _fixture_start

    _step "kill_switch=true saved"
    _start first
    _wait_healthy first
    _import_profile
    _ctl settings set kill_switch=true || fail "thronectl settings set kill_switch=true failed"
    wait_until 60 "kill_switch armed in the status" _status_is kill_switch armed
    _stop first

    _step "--no-kill-switch: failed with code overridden, profiles refused, the API reachable"
    _start second --no-kill-switch
    _wait_healthy second
    _log_has second.log "kill switch overridden"
    _log_has second.log "[Kill switch] Not active (overridden)"
    _status
    _expect_eq "status kill_switch" "$(_sfield kill_switch)" failed
    _expect_eq "status kill_switch_error" "$(_sfield kill_switch_error)" overridden
    _expect_eq "status degraded" "$(_sfield degraded)" kill_switch_failed
    _expect_ctl_error kill_switch_inactive --timeout 60s connect "$PROFILE_ID"
    _assert_direct_ok_unproxied

    _step "kill_switch=false: profiles start"
    _ctl settings set kill_switch=false || fail "thronectl settings set kill_switch=false failed"
    wait_until 30 "kill_switch off in the status" _status_is kill_switch off
    _status
    _expect_eq "status degraded" "$(_sfield degraded)" null
    _connect
    _assert_proxied

    _step "disconnect, SIGTERM"
    _disconnect
    _stop second
}

_image_healthy() {
    local state
    state=$(docker inspect -f '{{.State.Status}} {{if .State.Health}}{{.State.Health.Status}}{{end}}' "$C") || return 1
    case $state in
        "running healthy") return 0 ;;
        running*) return 1 ;;
    esac
    docker logs "$C" 2>&1 | tail -n 100
    fail "$C is '$state', not running"
}

scenario_image() {
    local started elapsed code
    if [ -z "${E2E_IMAGE:-}" ]; then
        echo "::notice::L2 scenario image skipped: E2E_IMAGE is not set (the tag of the image built from docker/Dockerfile)"
        exit "$SKIPPED"
    fi
    docker image inspect "$E2E_IMAGE" >/dev/null 2>&1 ||
        fail "E2E_IMAGE $E2E_IMAGE is not an image in this docker (did the job's image build fail?)"
    C=$PREFIX-image
    C_DATA=/var/lib/throne
    C_DOCKER_LOG=image.log

    _step "docker run $E2E_IMAGE with docker/compose.yaml's flags"
    docker volume create --label "$LABEL" "$PREFIX-image-data" >/dev/null
    docker run -d --name "$C" --label "$LABEL" --network "$NET" --ip "198.18.0.$((20 + SC_INDEX))" \
        --restart unless-stopped --stop-timeout 45 --cap-add NET_ADMIN --device /dev/net/tun \
        -p 127.0.0.1::2080 --mount "type=volume,src=$PREFIX-image-data,dst=/var/lib/throne" \
        "$E2E_IMAGE" >/dev/null || fail "docker run of $E2E_IMAGE failed"

    _step "the image's HEALTHCHECK turns healthy"
    wait_until 180 "Docker's health status of $C turning healthy (first probe after 30 s)" _image_healthy
    docker exec "$C" /opt/Throne/Throne --health || fail "Throne --health in $C failed"
    docker exec "$C" thronectl --key-file /var/lib/throne/config/api.key status ||
        fail "thronectl --key-file /var/lib/throne/config/api.key status failed in $C"

    _step "docker stop: exit 0 within 45 s"
    started=$SECONDS
    docker stop --time 45 "$C" >/dev/null || fail "docker stop $C failed"
    elapsed=$((SECONDS - started))
    code=$(docker inspect -f '{{.State.ExitCode}}' "$C")
    echo "docker stop took $elapsed s; exit code $code"
    _expect_eq "exit code after docker stop" "$code" 0
    ((elapsed < 45)) || fail "docker stop took $elapsed s; Docker kills at 45 s"
    docker logs "$C" >"$LOGDIR/image.log" 2>&1
    _log_has image.log "EXIT 0: SIGTERM"
}

# ---------------------------------------------------------------------------------------------------- runner

_collect() {
    [ -n "${C:-}" ] || return 0
    mkdir -p "$LOGDIR/data-logs"
    docker cp "$C:$C_DATA/config/logs/." "$LOGDIR/data-logs/" >/dev/null 2>&1 || true
    docker cp "$C:$C_DATA/config/headless-state.json" "$LOGDIR/data-logs/headless-state.json" >/dev/null 2>&1 || true
    if [ -n "${C_DOCKER_LOG:-}" ]; then
        docker logs "$C" >"$LOGDIR/$C_DOCKER_LOG" 2>&1 || true
    fi
}

_dump() {
    local file
    echo "::group::L2 $SC failed: Throne output, throne.log, fixture log, container state"
    for file in "$LOGDIR"/*.log; do
        if [ -f "$file" ]; then
            echo "----- $file (last 300 lines)"
            tail -n 300 "$file"
        fi
    done
    for file in "$LOGDIR"/*.boots "$LOGDIR"/*.rc "$LOGDIR/client.stderr" "$LOGDIR/data-logs/headless-state.json"; do
        if [ -f "$file" ]; then
            echo "----- $file"
            cat "$file"
        fi
    done
    if [ -f "$LOGDIR/data-logs/throne.log" ]; then
        echo "----- config/logs/throne.log (last 200 lines)"
        tail -n 200 "$LOGDIR/data-logs/throne.log"
    fi
    for file in "$WORK/fixture/$SC.log" "$WORK/fixture/$SC.stderr"; do
        if [ -f "$file" ]; then
            echo "----- $file (last 100 lines)"
            tail -n 100 "$file"
        fi
    done
    if [ -n "${C:-}" ]; then
        echo "----- processes, addresses, routes and nftables in $C"
        docker exec "$C" sh -c 'ps -eo pid,ppid,stat,args; ip -brief addr; ip route; ip rule; nft list ruleset 2>&1 | head -n 150' 2>&1
    fi
    echo "::endgroup::"
}

_scenario_exit() {
    local rc=$?
    set +e
    trap - ERR
    _e2e_end_group
    _collect
    if [ "$rc" != 0 ] && [ "$rc" != "$SKIPPED" ]; then
        _dump
    fi
    _fixture_stop
    rm -f "$WORK/fixture/$SC.api.key"
    if [ -n "${C:-}" ]; then
        docker rm -f "$C" >/dev/null 2>&1
    fi
    exit "$rc"
}

_scenario_done() {
    _step "no HEADLESS-DIALOG, HEADLESS-WIDGET or POLICY-UNBOUND in its logs"
    _collect
    assert_log_clean "$LOGDIR"/*.log "$LOGDIR"/data-logs/*.log
    _e2e_end_group
}

_scenario_index() {
    local i
    for i in "${!ALL_SCENARIOS[@]}"; do
        if [ "${ALL_SCENARIOS[$i]}" = "$1" ]; then
            echo $((i + 1))
            return 0
        fi
    done
    return 1
}

# Each scenario runs in a subshell of its own: a fail ends that scenario only, and its EXIT trap removes its container.
_run() {
    local name=$1 index=$2 started=$SECONDS rc result
    _e2e_end_group
    echo "===== L2 scenario $index: $name"
    set +e
    trap - ERR
    (
        set -e
        _e2e_trace_errors
        SC=$name
        SC_INDEX=$index
        LOGDIR=$WORK/logs/$name
        C=
        C_DATA=$D
        C_DOCKER_LOG=
        mkdir -p "$LOGDIR"
        trap _scenario_exit EXIT
        "scenario_$name"
        _scenario_done
    )
    rc=$?
    _e2e_trace_errors
    set -e
    case $rc in
        0) result=passed ;;
        "$SKIPPED") result=skipped ;;
        *) result=failed ;;
    esac
    echo "===== L2 scenario $index: $name $result ($((SECONDS - started)) s)"
    RESULTS+=("$name|$result|$((SECONDS - started))")
}

_report() {
    local row name result secs failed=()
    _e2e_end_group
    echo "L2 results:"
    _e2e_summary ""
    _e2e_summary "| L2 scenario | Result | Seconds |"
    _e2e_summary "|---|---|---|"
    for row in "${RESULTS[@]}"; do
        IFS='|' read -r name result secs <<<"$row"
        printf '  %-16s %-8s %4s s\n' "$name" "$result" "$secs"
        _e2e_summary "| $name | $result | $secs |"
        if [ "$result" = failed ]; then
            failed+=("$name")
        fi
    done
    if ((${#failed[@]} > 0)); then
        echo "::error::L2: ${#failed[@]} of ${#RESULTS[@]} scenarios failed: ${failed[*]} (logs in $WORK/logs)"
        exit 1
    fi
}

_setup() {
    local out
    step "L2 setup: inputs"
    [ -x "$THRONE_DIR/Throne" ] || fail "THRONE_DIR $THRONE_DIR has no executable Throne"
    [ -x "$THRONE_DIR/ThroneCore" ] || fail "THRONE_DIR $THRONE_DIR has no executable ThroneCore"
    [ -x "$THRONECTL" ] || fail "THRONECTL $THRONECTL is not executable"
    [ -f "$FIXTURES" ] || fail "FIXTURES $FIXTURES is not a file"
    for out in docker python3 curl; do
        command -v "$out" >/dev/null || fail "$out is not installed on this runner"
    done
    mkdir -p "$E2E_TMP"
    THRONE_DIR=$(cd "$THRONE_DIR" && pwd)
    THRONECTL=$(cd "$(dirname "$THRONECTL")" && pwd)/$(basename "$THRONECTL")
    FIXTURES=$(cd "$(dirname "$FIXTURES")" && pwd)/$(basename "$FIXTURES")
    WORK=$(cd "$E2E_TMP" && pwd)/linux-scenarios
    echo "THRONE_DIR=$THRONE_DIR THRONECTL=$THRONECTL FIXTURES=$FIXTURES work dir $WORK"
    docker version --format 'docker {{.Server.Version}}' || fail "docker does not answer"
    rm -rf "$WORK"
    mkdir -p "$WORK/bin" "$WORK/fixture" "$WORK/logs"
    _write_wrapper

    # The runtime libraries of the bundled deb (script/pack_debian.sh, Ubuntu 24.04 names), and the scenarios' tools.
    step "L2 setup: test image $TEST_IMAGE"
    docker build -t "$TEST_IMAGE" - <<'EOF' || fail "building $TEST_IMAGE failed"
FROM ubuntu:24.04
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get -o Acquire::Retries=3 update \
 && apt-get -o Acquire::Retries=3 install -y --no-install-recommends \
        ca-certificates libegl1 libgl1 libopengl0 libglx0 libfontconfig1 libfreetype6 libxkbcommon0 libx11-6 libx11-xcb1 \
        libxcb1 libdbus-1-3 libglib2.0-0t64 libssl3t64 libgssapi-krb5-2 \
        iproute2 curl procps python3 nftables \
 && rm -rf /var/lib/apt/lists/* /etc/machine-id /var/lib/dbus/machine-id
EOF
    out=$(docker run --rm --mount "type=bind,src=$THRONE_DIR,dst=/opt/Throne,readonly" "$TEST_IMAGE" \
        sh -c 'ldd /opt/Throne/Throne; ldd /opt/Throne/ThroneCore; /opt/Throne/Throne --headless --version' 2>&1) || true
    echo "$out"
    if grep -F 'not found' <<<"$out"; then
        fail "Throne or ThroneCore misses libraries in $TEST_IMAGE (lines above): the package list here or the deb's Depends lags"
    fi

    step "L2 setup: network $NET (198.18.0.0/24) and the fixture container at $FIX_IP"
    sudo -n modprobe dummy || echo "modprobe dummy failed; tun_conflict needs the dummy module for its dummy0"
    docker network create --driver bridge --subnet 198.18.0.0/24 --gateway 198.18.0.1 --label "$LABEL" "$NET" >/dev/null ||
        fail "docker network create $NET failed"
    docker run -d --init --name "$FIX" --hostname e2e-fixture --label "$LABEL" --network "$NET" --ip "$FIX_IP" \
        -e LANG=C.UTF-8 \
        --mount "type=bind,src=$FIXTURES,dst=/fx-src/smoke_fixtures.py,readonly" \
        --mount "type=bind,src=$WORK/fixture,dst=/fx" \
        --mount "type=bind,src=$THRONECTL,dst=/usr/local/bin/thronectl,readonly" \
        "$TEST_IMAGE" sleep infinity >/dev/null || fail "docker run of $FIX failed"
    docker exec "$FIX" python3 --version || fail "the fixture container does not run"
}

e2e_on_exit() {
    docker ps -aq --filter "label=$LABEL" 2>/dev/null | xargs -r docker rm -f >/dev/null 2>&1
    docker network rm "$NET" >/dev/null 2>&1
    docker volume ls -q --filter "label=$LABEL" 2>/dev/null | xargs -r docker volume rm >/dev/null 2>&1
    return 0
}

main() {
    local name index
    _e2e_trace_errors
    _setup
    for name in ${E2E_SCENARIOS:-${ALL_SCENARIOS[*]}}; do
        index=$(_scenario_index "$name") || fail "unknown scenario '$name' in E2E_SCENARIOS; known: ${ALL_SCENARIOS[*]}"
        _run "$name" "$index"
    done
    _report
}

main "$@"
