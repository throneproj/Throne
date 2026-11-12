#!/usr/bin/env bash
# Smoke (not a unit test): both TLS link parsers still read pinSHA256.
# Refactors that move or rename code may need to update this grep.
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
# shellcheck source=lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

grep -q 'else if (query.hasQueryItem("pinSHA256"))' "$ROOT/src/configs/common/xrayStreamSetting.cpp" \
  || fail "xray stream TLS missing pinSHA256 fallback"
grep -q 'else if (query.hasQueryItem("pinSHA256"))' "$ROOT/src/configs/common/TLS.cpp" \
  || fail "sing-box TLS missing pinSHA256 fallback"
pass "pinSHA256 accepted in both TLS parsers"
echo "OK: p0_xhttp_pin_check"
