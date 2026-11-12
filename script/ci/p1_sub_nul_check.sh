#!/usr/bin/env bash
# Smoke (not a unit test): subscription text uses QByteArray length, not a C-string cut at NUL.
# Refactors that move or rename code may need to update this grep.
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
# shellcheck source=lib.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib.sh"

PARSER="$ROOT/src/configs/sub/SubscriptionParser.cpp"
grep -F -q 'fromUtf8(s.data(), static_cast<qsizetype>(s.size()))' "$PARSER" \
  || grep -F -q 'fromUtf8(text.data(), static_cast<qsizetype>(text.size()))' "$PARSER" \
  || fail "SubscriptionParser no longer uses fromUtf8(ptr, size)"
pass "subscription import still size-aware"
echo "OK: p1_sub_nul_check"
