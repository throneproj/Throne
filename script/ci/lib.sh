# shellcheck shell=bash
# Shared helpers for script/ci smoke checks. Source it, do not run it:
#   source "$(dirname "$0")/lib.sh"
#
# Config smokes (p0, p1) grep source for one invariant each. A refactor may
# require updating the pattern even when behavior is unchanged.
if [ -n "${CI_LIB_LOADED:-}" ]; then
    return 0
fi
CI_LIB_LOADED=1

fail() {
    echo "FAIL: $*" >&2
    exit 1
}

pass() {
    echo "PASS: $*"
}
