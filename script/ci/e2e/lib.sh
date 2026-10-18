# shellcheck shell=bash
# Shared helpers of the headless end-to-end suite (script/ci/e2e/*.sh). Source it, do not run it:
#   source "$(dirname "$0")/lib.sh"
#
# Public functions (the suite's frozen contract; other scripts use only these):
#   step NAME                       end the open log group, open one named NAME, add "- NAME" to the job summary
#   fail MSG                        ::error:: MSG (prefixed with the current step) and exit 1
#   wait_until TIMEOUT_S DESC CMD   run CMD every 0.5 s until it succeeds; fail "DESC: not within TIMEOUT_S s"
#   connect_env THRONE DATA_DIR     export THRONE_ADDR and THRONE_KEY_FILE from THRONE --print-connection
#   assert_log_clean FILE...        fail when a log names HEADLESS-DIALOG, HEADLESS-WIDGET or POLICY-UNBOUND
#   start_fixtures LISTEN LOGFILE   start smoke_fixtures.py in the background on LISTEN, wait until it listens, set
#                                   FIXTURE_PID; prints nothing. Its ready file is LOGFILE.ready, its stderr LOGFILE.stderr
# Names starting with _ are private helpers of the linux scripts.
#
# Env:
#   FIXTURES               script/ci/smoke_fixtures.py (default: the one next to this directory)
#   FIXTURE_HTTP_PORT      HTTP port of start_fixtures; default $HTTP_PORT when the caller set it, else 18080
#   FIXTURE_SOCKS_PORT     SOCKS5 port of start_fixtures; default $SOCKS_PORT when the caller set it, else 1080
#   FIXTURE_HTTP_DELAY_MS  delay before each HTTP 204 (default 25)
#   GITHUB_STEP_SUMMARY    set by GitHub Actions; step and fail append to it
#
# Sourcing installs an EXIT trap that closes an open log group, calls e2e_on_exit STATUS when the script defines that
# function, and stops the fixtures. A script that sets its own EXIT trap replaces it and must kill $FIXTURE_PID itself.
# Written for bash 3.2 too (macOS /bin/bash).

if [ -n "${_E2E_LIB_LOADED:-}" ]; then
    return 0
fi
_E2E_LIB_LOADED=1

_E2E_LIB_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
: "${FIXTURES:=$_E2E_LIB_DIR/../smoke_fixtures.py}"
FIXTURE_PID=${FIXTURE_PID:-}
_E2E_DIRTY_RE='HEADLESS-(DIALOG|WIDGET)|POLICY-UNBOUND'
_e2e_group_open=
_e2e_step=

# Workflow commands end at a newline and give % a meaning.
_e2e_escape() {
    local text=$1
    text=${text//'%'/'%25'}
    text=${text//$'\r'/'%0D'}
    text=${text//$'\n'/'%0A'}
    printf '%s' "$text"
}

_e2e_summary() {
    if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
        printf '%s\n' "$1" >>"$GITHUB_STEP_SUMMARY" 2>/dev/null || true
    fi
}

_e2e_end_group() {
    if [ -n "$_e2e_group_open" ]; then
        _e2e_group_open=
        echo "::endgroup::"
    fi
}

step() {
    _e2e_end_group
    _e2e_step=$1
    echo "::group::$(_e2e_escape "$1")"
    _e2e_group_open=1
    _e2e_summary "- $1"
}

fail() {
    local message="$*"
    _e2e_end_group
    if [ -n "$_e2e_step" ]; then
        message="$_e2e_step: $message"
    fi
    echo "::error::$(_e2e_escape "$message")" >&2
    _e2e_summary "  - **failed:** $*"
    exit 1
}

wait_until() {
    local limit=$1 what=$2 started=$SECONDS
    shift 2
    echo "Waiting up to $limit s for $what" >&2
    until "$@"; do
        if ((SECONDS - started >= limit)); then
            fail "$what: not within $limit s"
        fi
        sleep 0.5
    done
    echo "... $what after $((SECONDS - started)) s" >&2
}

connect_env() {
    local throne=$1 data=$2 out line re='^export THRONE_(ADDR|KEY_FILE)='
    out=$("$throne" --print-connection --data-dir "$data") ||
        fail "$throne --print-connection --data-dir $data exited with status $?${out:+ after printing: $out}"
    # Only the two export lines are ever evaluated.
    while IFS= read -r line; do
        [[ $line =~ $re ]] || fail "unexpected line from $throne --print-connection: $line"
    done <<<"$out"
    unset THRONE_KEY
    eval "$out"
    [ -n "${THRONE_ADDR:-}" ] || fail "$throne --print-connection printed no THRONE_ADDR"
    [ -r "${THRONE_KEY_FILE:-}" ] || fail "THRONE_KEY_FILE '${THRONE_KEY_FILE:-}' from --print-connection is not a readable file"
    echo "THRONE_ADDR=$THRONE_ADDR THRONE_KEY_FILE=$THRONE_KEY_FILE" >&2
}

assert_log_clean() {
    local file rc checked=0 dirty=0
    for file in "$@"; do
        if [ ! -e "$file" ]; then
            echo "assert_log_clean: no $file; skipped" >&2
            continue
        fi
        rc=0
        grep -aHnE "$_E2E_DIRTY_RE" -- "$file" >&2 || rc=$?
        case $rc in
            0) dirty=1 ;;
            1) ;;
            *) fail "assert_log_clean: cannot read $file" ;;
        esac
        checked=$((checked + 1))
    done
    ((checked > 0)) || fail "assert_log_clean: none of these logs exists: $*"
    ((dirty == 0)) || fail "headless Throne reached a dialog, a widget or an unbound policy call (lines above)"
}

start_fixtures() {
    local listen=$1 log=$2 rc
    local http=${FIXTURE_HTTP_PORT:-${HTTP_PORT:-18080}} socks=${FIXTURE_SOCKS_PORT:-${SOCKS_PORT:-1080}}
    local ready="$log.ready" deadline=$((SECONDS + 15))
    [ -f "$FIXTURES" ] || fail "FIXTURES '$FIXTURES' is not a file"
    _e2e_stop_fixtures
    mkdir -p "$(dirname "$log")"
    rm -f "$ready"
    python3 -I "$FIXTURES" --listen "$listen" --http-port "$http" --socks-port "$socks" \
        --http-delay-ms "${FIXTURE_HTTP_DELAY_MS:-25}" --log "$log" --ready-file "$ready" >/dev/null 2>"$log.stderr" &
    FIXTURE_PID=$!
    until [ -s "$ready" ]; do
        if ! kill -0 "$FIXTURE_PID" 2>/dev/null; then
            rc=0
            wait "$FIXTURE_PID" || rc=$?
            FIXTURE_PID=
            fail "the fixtures exited with status $rc before listening on $listen (HTTP $http, SOCKS5 $socks): $(cat "$log.stderr" 2>/dev/null)"
        fi
        if ((SECONDS >= deadline)); then
            fail "the fixtures did not listen on $listen (HTTP $http, SOCKS5 $socks) within 15 s: $(cat "$log.stderr" 2>/dev/null)"
        fi
        sleep 0.2
    done
}

_e2e_stop_fixtures() {
    if [ -n "${FIXTURE_PID:-}" ]; then
        kill "$FIXTURE_PID" 2>/dev/null || true
        wait "$FIXTURE_PID" 2>/dev/null || true
        FIXTURE_PID=
    fi
}

# Opt-in: with set -E, an unexpected failure names its line before errexit ends the script.
_e2e_err() {
    echo "::error::$(_e2e_escape "${BASH_SOURCE[1]##*/} line $1: \"$2\" failed with status $3")" >&2
}

_e2e_trace_errors() {
    set -E
    trap '_e2e_err "$LINENO" "$BASH_COMMAND" "$?"' ERR
}

_e2e_exit_trap() {
    local rc=$?
    set +e
    trap - ERR
    _e2e_end_group
    if declare -F e2e_on_exit >/dev/null; then
        e2e_on_exit "$rc"
    fi
    _e2e_stop_fixtures
    exit "$rc"
}

trap _e2e_exit_trap EXIT
