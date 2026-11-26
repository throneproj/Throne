module ThroneCore

go 1.27

require (
	github.com/Mahdi-zarei/speedtest-go v1.7.13-0.20261125001431-254983598f6d
	github.com/andybalholm/brotli v1.2.7
	github.com/dustin/go-humanize v1.0.1
	github.com/go-ole/go-ole v1.3.0
	github.com/gofrs/uuid/v5 v5.5.1
	github.com/google/shlex v0.0.0-20191202100458-e7afc7fbc510
	github.com/klauspost/compress v1.20.3
	github.com/miekg/dns v1.1.73
	github.com/sagernet/gomobile v0.1.13
	github.com/sagernet/nftables v0.3.0-mod.4.0.20261114074246-0b310c3e010b
	github.com/sagernet/quic-go v0.66.0-sing-box-mod.1
	github.com/sagernet/sing v0.9.7-0.20261125122036-9b11dbe28a7d
	github.com/sagernet/sing-box v1.15.0-alpha.13
	github.com/sagernet/sing-tun v0.9.7-0.20261126024559-1a13df1ac21f
	github.com/tailscale/go-winio v0.0.0-20231025203758-c4f33415bf55
	github.com/xtls/xray-core v1.260327.1-0.20260908222543-52a412d9e2f5
	go4.org/netipx v0.0.0-20260823151212-3075585bcbeb
	golang.org/x/crypto v0.59.0
	golang.org/x/net v0.63.0
	golang.org/x/sys v0.50.0
	google.golang.org/grpc v1.85.0
	google.golang.org/protobuf v1.36.12
)

replace github.com/xtls/xray-core => github.com/throneproj/xray-core v1.251015.1-0.20261113120537-eea0cb078b79

replace github.com/sagernet/sing-box => github.com/throneproj/sing-box v1.11.16-0.20261126130245-d1ea6bc3293e

replace github.com/sagernet/wireguard-go => github.com/throneproj/wireguard-go v0.0.0-20261126125753-d7dafa30c13f

replace github.com/sagernet/sing => github.com/throneproj/sing v0.9.4-0.20261125171526-5979c7963e80

replace github.com/sagernet/sing-openvpn => github.com/throneproj/sing-openvpn v0.1.1-0.20261125171531-a4fa5f58f661

replace github.com/metacubex/utls => github.com/throneproj/utls v0.0.0-20261122110438-55d28d9fd9a0

require (
	filippo.io/edwards25519 v1.2.0 // indirect
	github.com/RyuaNerin/go-krypto v1.3.0 // indirect
	github.com/ajg/form v1.5.1 // indirect
	github.com/akutz/memconn v0.1.0 // indirect
	github.com/alexbrainman/sspi v0.0.0-20250919150558-7d374ff0d59e // indirect
	github.com/anchore/go-lzo v0.1.0 // indirect
	github.com/anmitsu/go-shlex v0.0.0-20200514113438-38f4b401e2be // indirect
	github.com/anthropics/anthropic-sdk-go v1.26.0 // indirect
	github.com/apernet/quic-go v0.63.1-0.20261004180939-a10df75c260c // indirect
	github.com/axiomhq/hyperloglog v0.2.6 // indirect
	github.com/caddyserver/certmagic v0.25.3-0.20260421143802-60d9d8b415d6 // indirect
	github.com/caddyserver/zerossl v0.1.5 // indirect
	github.com/cloudflare/circl v1.6.5 // indirect
	github.com/coder/websocket v1.8.15 // indirect
	github.com/coreos/go-iptables v0.8.0 // indirect
	github.com/coreos/go-oidc/v3 v3.17.0 // indirect
	github.com/creack/pty v1.1.24 // indirect
	github.com/cretz/bine v0.2.0 // indirect
	github.com/database64128/netx-go v0.1.1 // indirect
	github.com/database64128/tfo-go/v2 v2.4.0 // indirect
	github.com/dblohm7/wingoes v0.0.0-20260526185140-fb298caac7ca // indirect
	github.com/dgryski/go-camellia v0.0.0-20191119043421-69a8a13fb23d // indirect
	github.com/dgryski/go-metro v0.0.0-20250106013310-edb8663e5e33 // indirect
	github.com/ebitengine/purego v0.10.0 // indirect
	github.com/enfein/mieru/v3 v3.39.0 // indirect
	github.com/florianl/go-nfqueue/v2 v2.1.0 // indirect
	github.com/flynn/noise v1.1.0 // indirect
	github.com/fsnotify/fsnotify v1.10.1 // indirect
	github.com/fxamacker/cbor/v2 v2.9.3 // indirect
	github.com/gaissmai/bart v0.29.0 // indirect
	github.com/ghodss/yaml v1.0.1-0.20220118164431-d8423dcdf344 // indirect
	github.com/go-chi/chi/v5 v5.2.5 // indirect
	github.com/go-chi/render v1.0.3 // indirect
	github.com/go-jose/go-jose/v4 v4.1.4 // indirect
	github.com/go-json-experiment/json v0.0.0-20260820222146-c27c302e5fc3 // indirect
	github.com/go4org/hashtriemap v0.0.0-20260925222741-44e5305f85d9 // indirect
	github.com/gobwas/httphead v0.1.0 // indirect
	github.com/gobwas/pool v0.2.1 // indirect
	github.com/godbus/dbus/v5 v5.2.2 // indirect
	github.com/golang/groupcache v0.0.0-20241129210726-2c02b8208cf8 // indirect
	github.com/google/btree v1.1.3 // indirect
	github.com/google/certificate-transparency-go v1.3.2 // indirect
	github.com/google/go-cmp v0.7.0 // indirect
	github.com/google/gopacket v1.1.19 // indirect
	github.com/google/nftables v0.3.0 // indirect
	github.com/google/uuid v1.6.0 // indirect
	github.com/gorilla/websocket v1.5.3 // indirect
	github.com/hashicorp/yamux v0.1.2 // indirect
	github.com/hdevalence/ed25519consensus v0.2.0 // indirect
	github.com/huin/goupnp v1.3.0 // indirect
	github.com/insomniacslk/dhcp v0.0.0-20260901064844-234b97448fae // indirect
	github.com/jackpal/go-nat-pmp v1.0.2 // indirect
	github.com/jsimonetti/rtnetlink v1.4.2 // indirect
	github.com/juju/ratelimit v1.0.2 // indirect
	github.com/kamstrup/intmap v0.5.2 // indirect
	github.com/keybase/go-keychain v0.0.1 // indirect
	github.com/klauspost/cpuid/v2 v2.4.2 // indirect
	github.com/klauspost/reedsolomon v1.14.1 // indirect
	github.com/koron/go-ssdp v0.0.4 // indirect
	github.com/kr/fs v0.1.0 // indirect
	github.com/kr/pretty v0.3.1 // indirect
	github.com/libdns/acmedns v0.5.0 // indirect
	github.com/libdns/alidns v1.0.6 // indirect
	github.com/libdns/cloudflare v0.2.2 // indirect
	github.com/libdns/libdns v1.1.1 // indirect
	github.com/libp2p/go-nat v1.0.1-0.20250821073202-01afc089f138 // indirect
	github.com/libp2p/go-netroute v0.2.1 // indirect
	github.com/logrusorgru/aurora v2.0.3+incompatible // indirect
	github.com/mdlayher/netlink v1.11.2 // indirect
	github.com/mdlayher/socket v0.7.0 // indirect
	github.com/metacubex/utls v1.9.0-alpha.1 // indirect
	github.com/mholt/acmez/v3 v3.1.6 // indirect
	github.com/mitchellh/go-ps v1.0.0 // indirect
	github.com/molecule-man/go-brrr v1.2.0 // indirect
	github.com/openai/openai-go/v3 v3.26.0 // indirect
	github.com/pelletier/go-toml v1.9.5 // indirect
	github.com/pierrec/lz4/v4 v4.1.26 // indirect
	github.com/pion/dtls/v3 v3.1.5 // indirect
	github.com/pion/logging v0.2.4 // indirect
	github.com/pion/stun/v3 v3.1.7 // indirect
	github.com/pion/transport/v4 v4.1.0 // indirect
	github.com/pires/go-proxyproto v0.15.0 // indirect
	github.com/pkg/errors v0.9.1 // indirect
	github.com/pkg/sftp v1.13.10 // indirect
	github.com/quic-go/qpack v0.6.0 // indirect
	github.com/refraction-networking/utls v1.8.3-0.20261108164504-a13c59ffe98b // indirect
	github.com/robfig/cron/v3 v3.0.1 // indirect
	github.com/safchain/ethtool v0.7.0 // indirect
	github.com/sagernet/bbolt v0.0.0-20261124060517-2ab7bd6d2bff // indirect
	github.com/sagernet/cors v1.2.1 // indirect
	github.com/sagernet/cronet-go v0.0.0-20261126083600-ccefd17767e7 // indirect
	github.com/sagernet/cronet-go/all v0.0.0-20261126083600-ccefd17767e7 // indirect
	github.com/sagernet/cronet-go/lib/android_386 v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/android_amd64 v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/android_arm v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/android_arm64 v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/darwin_amd64 v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/darwin_arm64 v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/ios_amd64_simulator v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/ios_arm64 v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/ios_arm64_simulator v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/linux_386 v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/linux_386_musl v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/linux_amd64 v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/linux_amd64_musl v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/linux_arm v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/linux_arm64 v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/linux_arm64_musl v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/linux_arm_musl v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/linux_loong64 v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/linux_loong64_musl v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/linux_mips64le v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/linux_mipsle v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/linux_mipsle_musl v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/linux_riscv64 v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/linux_riscv64_musl v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/tvos_amd64_simulator v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/tvos_arm64 v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/tvos_arm64_simulator v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/windows_amd64 v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/cronet-go/lib/windows_arm64 v0.0.0-20261126082848-2767273ce7ab // indirect
	github.com/sagernet/fswatch v0.1.2 // indirect
	github.com/sagernet/gliderssh v0.3.4-0.20261023092853-e0cad6d28647 // indirect
	github.com/sagernet/gvisor v0.0.0-20260727.0-sing-box-mod.1 // indirect
	github.com/sagernet/netlink v0.0.0-20261125120807-b006b90aa077 // indirect
	github.com/sagernet/sing-anytls v0.0.0-20261124060518-f3865241cc12 // indirect
	github.com/sagernet/sing-cloudflared v0.1.4-0.20261125132439-71e2e5ed8ed0 // indirect
	github.com/sagernet/sing-mux v0.3.10-0.20261125130604-1dd833b8fc3a // indirect
	github.com/sagernet/sing-openconnect v0.1.1-0.20261126032024-bc53c1ef8ca9 // indirect
	github.com/sagernet/sing-openvpn v0.1.1-0.20261125120933-28a40b4ceb1f // indirect
	github.com/sagernet/sing-quic v0.7.2-0.20261126024618-27f15a98d9c9 // indirect
	github.com/sagernet/sing-shadowsocks v0.2.9-0.20260929204512-65740e0f0e3e // indirect
	github.com/sagernet/sing-shadowsocks2 v0.2.2-0.20261110092058-52ade8ab7e95 // indirect
	github.com/sagernet/sing-shadowtls v0.2.2-0.20261125124310-5028b2558ebf // indirect
	github.com/sagernet/sing-snell v0.0.0-20261125120456-6dfe590430a8 // indirect
	github.com/sagernet/sing-usbip v0.0.0-20260817040617-28bd42667eca // indirect
	github.com/sagernet/sing-vmess v0.2.9-0.20261124075649-4acc92a6f6c6 // indirect
	github.com/sagernet/smux v1.5.50-sing-box-mod.1 // indirect
	github.com/sagernet/tailscale v1.106.1-sing-box-1.15-mod.1 // indirect
	github.com/sagernet/wireguard-go v0.0.8-0.20261126003404-5825d8004736 // indirect
	github.com/sagernet/ws v0.0.0-20231204124109-acfe8907c854 // indirect
	github.com/smallstep/pkcs7 v0.1.1 // indirect
	github.com/tailscale/certstore v0.1.1-0.20260409135935-3638fb84b77d // indirect
	github.com/tailscale/hujson v0.0.0-20260727124030-b80ff77dac4f // indirect
	github.com/tailscale/netlink v1.1.1-0.20240822203006-4d49adab4de7 // indirect
	github.com/tailscale/peercred v0.0.0-20250107143737-35a0c7bd7edc // indirect
	github.com/tailscale/web-client-prebuilt v0.0.0-20260917222731-e0ed2d0d0fea // indirect
	github.com/tidwall/gjson v1.18.0 // indirect
	github.com/tidwall/match v1.1.1 // indirect
	github.com/tidwall/pretty v1.2.1 // indirect
	github.com/tidwall/sjson v1.2.5 // indirect
	github.com/tjfoc/gmsm v1.4.1 // indirect
	github.com/u-root/uio v0.0.0-20240224005618-d2acac8f3701 // indirect
	github.com/vishvananda/netlink v1.3.1 // indirect
	github.com/vishvananda/netns v0.0.5 // indirect
	github.com/wlynxg/anet v0.0.5 // indirect
	github.com/x448/float16 v0.8.4 // indirect
	github.com/xtaci/kcp-go/v5 v5.6.73 // indirect
	github.com/xtls/reality v0.0.0-20260908062103-8cdf7bf9c7f0 // indirect
	github.com/youmark/pkcs8 v0.0.0-20240726163527-a2c0da244d78 // indirect
	github.com/yuin/gopher-lua v1.1.2 // indirect
	github.com/zeebo/blake3 v0.2.4 // indirect
	go.uber.org/multierr v1.11.0 // indirect
	go.uber.org/zap v1.27.1 // indirect
	go.uber.org/zap/exp v0.3.0 // indirect
	go4.org/mem v0.0.0-20240501181205-ae6ca9944745 // indirect
	golang.org/x/crypto/x509roots/fallback v0.0.0-20261020153306-9f2b28d4331a // indirect
	golang.org/x/exp v0.0.0-20261124000051-d6646c509b6d // indirect
	golang.org/x/mod v0.43.0 // indirect
	golang.org/x/oauth2 v0.38.0 // indirect
	golang.org/x/sync v0.25.0 // indirect
	golang.org/x/term v0.48.0 // indirect
	golang.org/x/text v0.44.0 // indirect
	golang.org/x/time v0.16.0 // indirect
	golang.zx2c4.com/wintun v0.0.0-20230126152724-0fa3db229ce2 // indirect
	golang.zx2c4.com/wireguard v0.0.0-20250521234502-f333402bd9cb // indirect
	golang.zx2c4.com/wireguard/windows v1.1.1 // indirect
	google.golang.org/genproto/googleapis/rpc v0.0.0-20260817212433-ac3dfec99bb1 // indirect
	gopkg.in/yaml.v2 v2.4.0 // indirect
	layeh.com/gopher-luar v1.0.11 // indirect
	lukechampine.com/blake3 v1.4.1 // indirect
	zombiezen.com/go/capnproto2 v2.18.2+incompatible // indirect
)

// Built by github.com/throneproj/cronet-go with parhelia512's macOS 10.15 patch; script/build_go.sh checks it against the binding's headers.
replace github.com/sagernet/cronet-go/lib/darwin_amd64 => github.com/throneproj/cronet-go/lib/darwin_amd64 v0.0.0-20261126125156-0e8a9f8b2c2f

replace github.com/sagernet/cronet-go/lib/darwin_arm64 => github.com/throneproj/cronet-go/lib/darwin_arm64 v0.0.0-20261126125156-0e8a9f8b2c2f
