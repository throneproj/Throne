#!/bin/bash
# Regenerates res/schema/sing-box.json from the sing-box version pinned in core/go.mod.
# Run it after bumping that pin, otherwise the JSON editors validate against a stale schema.
set -e

TAGS="with_clash_api,with_gvisor,with_quic,with_wireguard,with_utls,with_dhcp,with_tailscale,with_openvpn,with_openconnect,with_naive_outbound,badlinkname,tfogo_checklinkname0"

cd "$(dirname "$0")/.."
OUT="$PWD/res/schema/sing-box.json"

pushd core
go run -tags "$TAGS" ./cmd/schemagen -o "$OUT"
popd

echo "wrote $OUT"
