module ThroneCore

go 1.26.10

require (
	github.com/Mahdi-zarei/speedtest-go v1.7.13-0.20260107171856-79c565dfd83a
	github.com/andybalholm/brotli v1.2.7
	github.com/cretz/bine v0.2.0
	github.com/dustin/go-humanize v1.0.1
	github.com/go-ole/go-ole v1.3.0
	github.com/gofrs/uuid/v5 v5.5.1
	github.com/google/shlex v0.0.0-20191202100458-e7afc7fbc510
	github.com/klauspost/compress v1.20.3
	github.com/miekg/dns v1.1.73
	github.com/sagernet/gomobile v0.1.13
	github.com/sagernet/nftables v0.3.0-mod.4.0.20261114074246-0b310c3e010b
	github.com/sagernet/quic-go v0.61.0-sing-box-mod.14
	github.com/sagernet/sing v0.9.7-0.20261110095940-27b73774b240
	github.com/sagernet/sing-box v1.15.0-alpha.13
	github.com/sagernet/sing-tun v0.9.7-0.20261119022030-4486c14ba472
	github.com/tailscale/go-winio v0.0.0-20231025203758-c4f33415bf55
	github.com/xtls/xray-core v1.260327.1-0.20260908222543-52a412d9e2f5
	gitlab.torproject.org/tpo/anti-censorship/pluggable-transports/goptlib v1.6.0
	gitlab.torproject.org/tpo/anti-censorship/pluggable-transports/lyrebird v0.0.0-20261104192811-59ac7e400c38
	go4.org/netipx v0.0.0-20231129151722-fdeea329fbba
	golang.org/x/crypto v0.59.0
	golang.org/x/net v0.62.0
	golang.org/x/sys v0.50.0
	google.golang.org/grpc v1.85.0
	google.golang.org/protobuf v1.36.12
)

replace github.com/xtls/xray-core => github.com/throneproj/xray-core v1.251015.1-0.20261113120537-eea0cb078b79

replace github.com/sagernet/sing-box => github.com/throneproj/sing-box v1.11.16-0.20261123082941-bf0db9a762a4

replace github.com/sagernet/wireguard-go => github.com/throneproj/wireguard-go v0.0.0-20261112143559-827ce83a4eed

replace github.com/sagernet/sing => github.com/throneproj/sing v0.9.4-0.20261113121621-2ce66de7d09b

replace github.com/metacubex/utls => github.com/throneproj/utls v0.0.0-20261122110438-55d28d9fd9a0

replace gitlab.torproject.org/tpo/anti-censorship/pluggable-transports/snowflake/v2 => github.com/throneproj/snowflake/v2 v2.15.5-0.20261123082518-be5992caba99

require (
	filippo.io/bigmod v0.0.1 // indirect
	filippo.io/edwards25519 v1.2.0 // indirect
	filippo.io/keygen v0.0.0-20230306160926-5201437acf8e // indirect
	github.com/AndreasBriese/bbloom v0.0.0-20190825152654-46b345b51c96 // indirect
	github.com/Jigsaw-Code/outline-sdk v0.0.16 // indirect
	github.com/Psiphon-Labs/bolt v0.0.0-20261020160549-5e39149d57c2 // indirect
	github.com/Psiphon-Labs/consistent v0.0.0-20240322131436-20aaa4e05737 // indirect
	github.com/Psiphon-Labs/covert-dtls v0.0.0-20260423180104-2d3b15cbc8be // indirect
	github.com/Psiphon-Labs/goptlib v0.0.0-20200406165125-c0e32a7a3464 // indirect
	github.com/Psiphon-Labs/pion-ice/v4 v4.0.0-20261026204224-ea8b9a9ff059 // indirect
	github.com/Psiphon-Labs/pion-webrtc/v4 v4.0.0-20261026144143-b3b8f4477c88 // indirect
	github.com/Psiphon-Labs/psiphon-tls v0.0.0-20261019151451-3e697502354d // indirect
	github.com/Psiphon-Labs/psiphon-tunnel-core v0.0.14-beta-ios.0.20261119191828-c8f89a991efc // indirect
	github.com/Psiphon-Labs/quic-go v0.0.0-20261103172121-ba3c8406711b // indirect
	github.com/Psiphon-Labs/utls v1.1.1-0.20260729134728-7a1fc711853d // indirect
	github.com/RyuaNerin/go-krypto v1.3.0 // indirect
	github.com/ajg/form v1.5.1 // indirect
	github.com/akutz/memconn v0.1.0 // indirect
	github.com/alexbrainman/sspi v0.0.0-20231016080023-1a75b4708caa // indirect
	github.com/anchore/go-lzo v0.1.0 // indirect
	github.com/anmitsu/go-shlex v0.0.0-20200514113438-38f4b401e2be // indirect
	github.com/anthropics/anthropic-sdk-go v1.26.0 // indirect
	github.com/apernet/quic-go v0.63.1-0.20261004180939-a10df75c260c // indirect
	github.com/armon/go-proxyproto v0.0.0-20180202201750-5b7edb60ff5f // indirect
	github.com/aws/aws-sdk-go-v2 v1.48.0 // indirect
	github.com/aws/aws-sdk-go-v2/config v1.32.35 // indirect
	github.com/aws/aws-sdk-go-v2/credentials v1.21.0 // indirect
	github.com/aws/aws-sdk-go-v2/feature/ec2/imds v1.20.9 // indirect
	github.com/aws/aws-sdk-go-v2/internal/configsources v1.5.12 // indirect
	github.com/aws/aws-sdk-go-v2/internal/endpoints/v2 v2.8.12 // indirect
	github.com/aws/aws-sdk-go-v2/internal/v4a v1.5.12 // indirect
	github.com/aws/aws-sdk-go-v2/service/internal/accept-encoding v1.13.25 // indirect
	github.com/aws/aws-sdk-go-v2/service/internal/presigned-url v1.14.12 // indirect
	github.com/aws/aws-sdk-go-v2/service/signin v1.13.1 // indirect
	github.com/aws/aws-sdk-go-v2/service/sqs v1.46.4 // indirect
	github.com/aws/aws-sdk-go-v2/service/sso v1.39.5 // indirect
	github.com/aws/aws-sdk-go-v2/service/ssooidc v1.44.5 // indirect
	github.com/aws/aws-sdk-go-v2/service/sts v1.52.0 // indirect
	github.com/aws/smithy-go v1.28.9 // indirect
	github.com/axiomhq/hyperloglog v0.2.6 // indirect
	github.com/bifurcation/mint v0.0.0-20180306135233-198357931e61 // indirect
	github.com/bits-and-blooms/bitset v1.10.0 // indirect
	github.com/bits-and-blooms/bloom/v3 v3.6.0 // indirect
	github.com/caddyserver/certmagic v0.25.3-0.20260421143802-60d9d8b415d6 // indirect
	github.com/caddyserver/zerossl v0.1.5 // indirect
	github.com/cespare/xxhash v1.1.0 // indirect
	github.com/cheekybits/genny v0.0.0-20170328200008-9127e812e1e9 // indirect
	github.com/cloudflare/circl v1.6.5 // indirect
	github.com/coder/websocket v1.8.14 // indirect
	github.com/cognusion/go-cache-lru v0.0.0-20170419142635-f73e2280ecea // indirect
	github.com/coreos/go-iptables v0.7.1-0.20240112124308-65c67c9f46e6 // indirect
	github.com/coreos/go-oidc/v3 v3.17.0 // indirect
	github.com/creack/pty v1.1.24 // indirect
	github.com/database64128/netx-go v0.1.1 // indirect
	github.com/database64128/tfo-go/v2 v2.4.0 // indirect
	github.com/dblohm7/wingoes v0.0.0-20240119213807-a09d6be7affa // indirect
	github.com/dchest/siphash v1.2.3 // indirect
	github.com/dgraph-io/badger v1.5.4-0.20180815194500-3a87f6d9c273 // indirect
	github.com/dgryski/go-camellia v0.0.0-20191119043421-69a8a13fb23d // indirect
	github.com/dgryski/go-farm v0.0.0-20240924180020-3414d57e47da // indirect
	github.com/dgryski/go-metro v0.0.0-20250106013310-edb8663e5e33 // indirect
	github.com/ebitengine/purego v0.10.0 // indirect
	github.com/enfein/mieru/v3 v3.39.0 // indirect
	github.com/florianl/go-nfqueue/v2 v2.1.0 // indirect
	github.com/flynn/noise v1.1.0 // indirect
	github.com/fsnotify/fsnotify v1.9.0 // indirect
	github.com/fxamacker/cbor/v2 v2.9.0 // indirect
	github.com/gaissmai/bart v0.26.1 // indirect
	github.com/ghodss/yaml v1.0.1-0.20220118164431-d8423dcdf344 // indirect
	github.com/go-chi/chi/v5 v5.2.5 // indirect
	github.com/go-chi/render v1.0.3 // indirect
	github.com/go-jose/go-jose/v4 v4.1.4 // indirect
	github.com/go4org/hashtriemap v0.0.0-20251130024219-545ba229f689 // indirect
	github.com/gobwas/httphead v0.1.0 // indirect
	github.com/gobwas/pool v0.2.1 // indirect
	github.com/godbus/dbus/v5 v5.2.2 // indirect
	github.com/golang/groupcache v0.0.0-20241129210726-2c02b8208cf8 // indirect
	github.com/golang/mock v1.7.0-rc.1 // indirect
	github.com/golang/protobuf v1.5.4 // indirect
	github.com/google/btree v1.1.3 // indirect
	github.com/google/certificate-transparency-go v1.3.2 // indirect
	github.com/google/go-cmp v0.7.0 // indirect
	github.com/google/gopacket v1.1.19 // indirect
	github.com/google/nftables v0.2.1-0.20240414091927-5e242ec57806 // indirect
	github.com/google/uuid v1.6.0 // indirect
	github.com/gorilla/websocket v1.5.3 // indirect
	github.com/grafov/m3u8 v0.0.0-20171211212457-6ab8f28ed427 // indirect
	github.com/hashicorp/yamux v0.1.2 // indirect
	github.com/hdevalence/ed25519consensus v0.2.0 // indirect
	github.com/huin/goupnp v1.3.0 // indirect
	github.com/insomniacslk/dhcp v0.0.0-20260220084031-5adc3eb26f91 // indirect
	github.com/jackpal/go-nat-pmp v1.0.2 // indirect
	github.com/jsimonetti/rtnetlink v1.4.1 // indirect
	github.com/juju/ratelimit v1.0.2 // indirect
	github.com/kamstrup/intmap v0.5.2 // indirect
	github.com/keybase/go-keychain v0.0.1 // indirect
	github.com/klauspost/cpuid/v2 v2.4.2 // indirect
	github.com/klauspost/reedsolomon v1.14.1 // indirect
	github.com/koron/go-ssdp v0.0.4 // indirect
	github.com/kr/fs v0.1.0 // indirect
	github.com/libdns/acmedns v0.5.0 // indirect
	github.com/libdns/alidns v1.0.6 // indirect
	github.com/libdns/cloudflare v0.2.2 // indirect
	github.com/libdns/libdns v1.1.1 // indirect
	github.com/libp2p/go-nat v1.0.1-0.20250821073202-01afc089f138 // indirect
	github.com/libp2p/go-netroute v0.2.1 // indirect
	github.com/libp2p/go-reuseport v0.4.0 // indirect
	github.com/logrusorgru/aurora v2.0.3+incompatible // indirect
	github.com/marusama/semaphore v0.0.0-20171214154724-565ffd8e868a // indirect
	github.com/mattn/go-sqlite3 v1.14.50 // indirect
	github.com/mdlayher/netlink v1.11.2 // indirect
	github.com/mdlayher/socket v0.6.0 // indirect
	github.com/metacubex/utls v1.9.0-alpha.1 // indirect
	github.com/mholt/acmez/v3 v3.1.6 // indirect
	github.com/mitchellh/go-ps v1.0.0 // indirect
	github.com/mroth/weightedrand v1.0.0 // indirect
	github.com/openai/openai-go/v3 v3.26.0 // indirect
	github.com/patrickmn/go-cache v2.1.0+incompatible // indirect
	github.com/pelletier/go-toml v1.9.5 // indirect
	github.com/pierrec/lz4/v4 v4.1.26 // indirect
	github.com/pion/datachannel v1.6.2 // indirect
	github.com/pion/dtls/v2 v2.2.7 // indirect
	github.com/pion/dtls/v3 v3.1.5 // indirect
	github.com/pion/ice/v4 v4.4.1 // indirect
	github.com/pion/interceptor v0.1.47 // indirect
	github.com/pion/logging v0.2.4 // indirect
	github.com/pion/mdns/v2 v2.1.0 // indirect
	github.com/pion/randutil v0.1.0 // indirect
	github.com/pion/rtcp v1.2.17 // indirect
	github.com/pion/rtp v1.10.5 // indirect
	github.com/pion/sctp v1.11.1 // indirect
	github.com/pion/sdp/v3 v3.0.19 // indirect
	github.com/pion/srtp/v3 v3.0.13 // indirect
	github.com/pion/stun v0.6.1 // indirect
	github.com/pion/stun/v3 v3.1.7 // indirect
	github.com/pion/transport/v2 v2.2.3 // indirect
	github.com/pion/transport/v4 v4.1.0 // indirect
	github.com/pion/turn/v4 v4.1.4 // indirect
	github.com/pion/turn/v5 v5.0.12 // indirect
	github.com/pion/webrtc/v4 v4.2.18 // indirect
	github.com/pires/go-proxyproto v0.15.0 // indirect
	github.com/pkg/errors v0.9.1 // indirect
	github.com/pkg/sftp v1.13.10 // indirect
	github.com/quic-go/qpack v0.6.0 // indirect
	github.com/realclientip/realclientip-go v1.0.0 // indirect
	github.com/refraction-networking/conjure v0.7.11-0.20240130155008-c8df96195ab2 // indirect
	github.com/refraction-networking/ed25519 v0.1.2 // indirect
	github.com/refraction-networking/gotapdance v1.7.10 // indirect
	github.com/refraction-networking/obfs4 v0.1.2 // indirect
	github.com/refraction-networking/utls v1.8.3-0.20260301010127-aa6edf4b11af // indirect
	github.com/robfig/cron/v3 v3.0.1 // indirect
	github.com/safchain/ethtool v0.3.0 // indirect
	github.com/sagernet/bbolt v0.0.0-20260915102804-500ee1e84832 // indirect
	github.com/sagernet/cors v1.2.1 // indirect
	github.com/sagernet/cronet-go v0.0.0-20261023060337-b3c2ed82ac97 // indirect
	github.com/sagernet/cronet-go/all v0.0.0-20261023060337-b3c2ed82ac97 // indirect
	github.com/sagernet/cronet-go/lib/android_386 v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/android_amd64 v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/android_arm v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/android_arm64 v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/darwin_amd64 v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/darwin_arm64 v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/ios_amd64_simulator v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/ios_arm64 v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/ios_arm64_simulator v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/linux_386 v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/linux_386_musl v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/linux_amd64 v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/linux_amd64_musl v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/linux_arm v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/linux_arm64 v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/linux_arm64_musl v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/linux_arm_musl v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/linux_loong64 v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/linux_loong64_musl v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/linux_mips64le v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/linux_mipsle v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/linux_mipsle_musl v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/linux_riscv64 v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/linux_riscv64_musl v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/tvos_amd64_simulator v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/tvos_arm64 v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/tvos_arm64_simulator v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/windows_amd64 v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/cronet-go/lib/windows_arm64 v0.0.0-20261023055542-d5437a0df01a // indirect
	github.com/sagernet/fswatch v0.1.2 // indirect
	github.com/sagernet/gliderssh v0.3.4-0.20261023092853-e0cad6d28647 // indirect
	github.com/sagernet/gvisor v0.0.0-20260727.0-sing-box-mod.1 // indirect
	github.com/sagernet/netlink v0.0.0-20261116064630-4ea47d5d44b1 // indirect
	github.com/sagernet/sing-anytls v0.0.0-20260928104022-580984e4d8cb // indirect
	github.com/sagernet/sing-cloudflared v0.1.4-0.20261108120326-83391fb7b139 // indirect
	github.com/sagernet/sing-mux v0.3.10-0.20261110064256-375087483ea2 // indirect
	github.com/sagernet/sing-openconnect v0.1.1-0.20261119021124-31db24231feb // indirect
	github.com/sagernet/sing-openvpn v0.1.1-0.20261110111939-ad7a6ce7d11b // indirect
	github.com/sagernet/sing-quic v0.7.2-0.20261114063120-0979e001618d // indirect
	github.com/sagernet/sing-shadowsocks v0.2.9-0.20260929204512-65740e0f0e3e // indirect
	github.com/sagernet/sing-shadowsocks2 v0.2.2-0.20261110092058-52ade8ab7e95 // indirect
	github.com/sagernet/sing-shadowtls v0.2.2-0.20261114073233-97863830a3b3 // indirect
	github.com/sagernet/sing-snell v0.0.0-20261110094602-04667b0a321c // indirect
	github.com/sagernet/sing-usbip v0.0.0-20260817040617-28bd42667eca // indirect
	github.com/sagernet/sing-vmess v0.2.9-0.20261116164733-ca746ebade33 // indirect
	github.com/sagernet/smux v1.5.50-sing-box-mod.1 // indirect
	github.com/sagernet/tailscale v1.102.1-sing-box-1.14-mod.5.0.20261028024127-670967df7dbf // indirect
	github.com/sagernet/wireguard-go v0.0.8-0.20261022215849-fdf0ad046586 // indirect
	github.com/sagernet/ws v0.0.0-20231204124109-acfe8907c854 // indirect
	github.com/sergeyfrolov/bsbuffer v0.0.0-20180903213811-94e85abb8507 // indirect
	github.com/shadowsocks/go-shadowsocks2 v0.1.5 // indirect
	github.com/sirupsen/logrus v1.9.3 // indirect
	github.com/smallstep/pkcs7 v0.1.1 // indirect
	github.com/syndtr/gocapability v0.0.0-20170704070218-db04d3cc01c8 // indirect
	github.com/tailscale/certstore v0.1.1-0.20260409135935-3638fb84b77d // indirect
	github.com/tailscale/hujson v0.0.0-20260302212456-ecc657c15afd // indirect
	github.com/tailscale/netlink v1.1.1-0.20240822203006-4d49adab4de7 // indirect
	github.com/tailscale/peercred v0.0.0-20250107143737-35a0c7bd7edc // indirect
	github.com/tailscale/web-client-prebuilt v0.0.0-20250124233751-d4cd19a26976 // indirect
	github.com/theodorsm/covert-dtls v1.5.1 // indirect
	github.com/tidwall/gjson v1.18.0 // indirect
	github.com/tidwall/match v1.1.1 // indirect
	github.com/tidwall/pretty v1.2.1 // indirect
	github.com/tidwall/sjson v1.2.5 // indirect
	github.com/tjfoc/gmsm v1.4.1 // indirect
	github.com/txthinking/runnergroup v0.0.0-20250224021307-5864ffeb65ae // indirect
	github.com/txthinking/socks5 v0.0.0-20260601051520-339b044ab0eb // indirect
	github.com/u-root/uio v0.0.0-20240224005618-d2acac8f3701 // indirect
	github.com/vishvananda/netlink v1.3.1 // indirect
	github.com/vishvananda/netns v0.0.5 // indirect
	github.com/wlynxg/anet v0.0.5 // indirect
	github.com/x448/float16 v0.8.4 // indirect
	github.com/xtaci/kcp-go/v5 v5.6.73 // indirect
	github.com/xtaci/smux v1.5.57 // indirect
	github.com/xtls/reality v0.0.0-20260908062103-8cdf7bf9c7f0 // indirect
	github.com/youmark/pkcs8 v0.0.0-20240726163527-a2c0da244d78 // indirect
	github.com/yuin/gopher-lua v1.1.2 // indirect
	github.com/zeebo/blake3 v0.2.4 // indirect
	gitlab.com/yawning/edwards25519-extra v0.0.0-20231005122941-2149dcafc266 // indirect
	gitlab.torproject.org/tpo/anti-censorship/pluggable-transports/ptutil v0.0.0-20250815012447-418f76dcf315 // indirect
	gitlab.torproject.org/tpo/anti-censorship/pluggable-transports/snowflake/v2 v2.15.4 // indirect
	gitlab.torproject.org/tpo/anti-censorship/pluggable-transports/webtunnel v0.0.7 // indirect
	go.uber.org/multierr v1.11.0 // indirect
	go.uber.org/zap v1.27.1 // indirect
	go.uber.org/zap/exp v0.3.0 // indirect
	go4.org/mem v0.0.0-20240501181205-ae6ca9944745 // indirect
	golang.org/x/crypto/x509roots/fallback v0.0.0-20261020153306-9f2b28d4331a // indirect
	golang.org/x/exp v0.0.0-20260410095643-746e56fc9e2f // indirect
	golang.org/x/mod v0.42.0 // indirect
	golang.org/x/oauth2 v0.36.0 // indirect
	golang.org/x/sync v0.25.0 // indirect
	golang.org/x/term v0.48.0 // indirect
	golang.org/x/text v0.44.0 // indirect
	golang.org/x/time v0.15.0 // indirect
	golang.org/x/tools v0.52.0 // indirect
	golang.zx2c4.com/wintun v0.0.0-20230126152724-0fa3db229ce2 // indirect
	golang.zx2c4.com/wireguard v0.0.0-20250521234502-f333402bd9cb // indirect
	golang.zx2c4.com/wireguard/windows v1.1.1 // indirect
	google.golang.org/genproto/googleapis/rpc v0.0.0-20260817212433-ac3dfec99bb1 // indirect
	gopkg.in/yaml.v2 v2.4.0 // indirect
	layeh.com/gopher-luar v1.0.11 // indirect
	lukechampine.com/blake3 v1.4.1 // indirect
	tailscale.com v1.58.2 // indirect
	zombiezen.com/go/capnproto2 v2.18.2+incompatible // indirect
)

// Built by github.com/throneproj/cronet-go with parhelia512's macOS 10.15 patch; script/build_go.sh checks it against the binding's headers.
replace github.com/sagernet/cronet-go/lib/darwin_amd64 => github.com/throneproj/cronet-go/lib/darwin_amd64 v0.0.0-20260923015522-0dc5a60c5c47

replace github.com/sagernet/cronet-go/lib/darwin_arm64 => github.com/throneproj/cronet-go/lib/darwin_arm64 v0.0.0-20260923015522-0dc5a60c5c47
