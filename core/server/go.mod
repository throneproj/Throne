module nekobox_core

go 1.23.0

toolchain go1.24.4

require (
	github.com/Mahdi-zarei/speedtest-go v1.7.12
	github.com/dustin/go-humanize v1.0.1
	github.com/gofrs/uuid/v5 v5.3.2
	github.com/google/shlex v0.0.0-20191202100458-e7afc7fbc510
	github.com/oschwald/maxminddb-golang v1.13.1
	github.com/sagernet/sing v0.6.11
	github.com/sagernet/sing-box v1.11.14
	github.com/sagernet/sing-dns v0.4.5
	github.com/sagernet/sing-tun v0.6.9
	github.com/sagernet/sing-box v1.11.14
	github.com/spf13/cobra v1.9.1
	golang.org/x/sys v0.33.0
	google.golang.org/grpc v1.73.0
	google.golang.org/protobuf v1.36.6
)

replace github.com/sagernet/sing-box => github.com/Mahdi-zarei/sing-box v1.3.5-0.20250701215754-027a41f0810f

replace github.com/sagernet/sing-dns => github.com/Mahdi-zarei/sing-dns v0.3.0-beta.14.0.20250507225909-53baf4f30fa3

require (
	github.com/ajg/form v1.5.1 // indirect
	github.com/andybalholm/brotli v1.0.6 // indirect
	github.com/caddyserver/certmagic v0.20.0 // indirect
	github.com/cloudflare/circl v1.6.1 // indirect
	github.com/cretz/bine v0.2.0 // indirect
	github.com/fsnotify/fsnotify v1.7.0 // indirect
	github.com/go-chi/chi/v5 v5.2.2 // indirect
	github.com/go-chi/render v1.0.3 // indirect
	github.com/go-ole/go-ole v1.3.0 // indirect
	github.com/go-task/slim-sprig v0.0.0-20230315185526-52ccab3ef572 // indirect
	github.com/gobwas/httphead v0.1.0 // indirect
	github.com/gobwas/pool v0.2.1 // indirect
	github.com/google/btree v1.1.3 // indirect
	github.com/google/go-cmp v0.7.0 // indirect
	github.com/google/pprof v0.0.0-20231101202521-4ca4178f5c7a // indirect
	github.com/hashicorp/yamux v0.1.2 // indirect
	github.com/inconshreveable/mousetrap v1.1.0 // indirect
	github.com/insomniacslk/dhcp v0.0.0-20250109001534-8abf58130905 // indirect
	github.com/josharian/native v1.1.0 // indirect
	github.com/klauspost/compress v1.17.4 // indirect
	github.com/klauspost/cpuid/v2 v2.2.5 // indirect
	github.com/libdns/alidns v1.0.3 // indirect
	github.com/libdns/cloudflare v0.1.1 // indirect
	github.com/libdns/libdns v0.2.2 // indirect
	github.com/logrusorgru/aurora v2.0.3+incompatible // indirect
	github.com/mdlayher/netlink v1.7.2 // indirect
	github.com/mdlayher/socket v0.5.1 // indirect
	github.com/metacubex/tfo-go v0.0.0-20241231083714-66613d49c422 // indirect
	github.com/mholt/acmez v1.2.0 // indirect
	github.com/miekg/dns v1.1.63 // indirect
	github.com/onsi/ginkgo/v2 v2.9.7 // indirect
	github.com/pierrec/lz4/v4 v4.1.14 // indirect
	github.com/quic-go/qpack v0.4.0 // indirect
	github.com/quic-go/qtls-go1-20 v0.4.1 // indirect
	github.com/sagernet/bbolt v0.0.0-20231014093535-ea5cb2fe9f0a // indirect
	github.com/sagernet/cloudflare-tls v0.0.0-20231208171750-a4483c1b7cd1 // indirect
	github.com/sagernet/cors v1.2.1 // indirect
	github.com/sagernet/fswatch v0.1.1 // indirect
	github.com/sagernet/gvisor v0.0.0-20241123041152-536d05261cff // indirect
	github.com/sagernet/netlink v0.0.0-20240612041022-b9a21c07ac6a // indirect
	github.com/sagernet/nftables v0.3.0-beta.4 // indirect
	github.com/sagernet/quic-go v0.49.0-beta.1 // indirect
	github.com/sagernet/reality v0.0.0-20230406110435-ee17307e7691 // indirect
	github.com/sagernet/sing-mux v0.3.2 // indirect
	github.com/sagernet/sing-quic v0.4.3 // indirect
	github.com/sagernet/sing-shadowsocks v0.2.8 // indirect
	github.com/sagernet/sing-shadowsocks2 v0.2.1 // indirect
	github.com/sagernet/sing-shadowtls v0.2.0 // indirect
	github.com/sagernet/sing-vmess v0.2.3 // indirect
	github.com/sagernet/smux v1.5.34-mod.2 // indirect
	github.com/sagernet/utls v1.6.7 // indirect
	github.com/sagernet/wireguard-go v0.0.1-beta.7 // indirect
	github.com/sagernet/ws v0.0.0-20231204124109-acfe8907c854 // indirect
	github.com/spf13/pflag v1.0.6 // indirect
	github.com/u-root/uio v0.0.0-20230220225925-ffce2a382923 // indirect
	github.com/vishvananda/netns v0.0.4 // indirect
	github.com/zeebo/blake3 v0.2.3 // indirect
	go.uber.org/multierr v1.11.0 // indirect
	go.uber.org/zap v1.27.0 // indirect
	go4.org/netipx v0.0.0-20231129151722-fdeea329fbba // indirect
	golang.org/x/crypto v0.36.0 // indirect
	golang.org/x/exp v0.0.0-20241217172543-b2144cdd0a67 // indirect
	golang.org/x/mod v0.22.0 // indirect
	golang.org/x/net v0.38.0 // indirect
	golang.org/x/sync v0.12.0 // indirect
	golang.org/x/text v0.23.0 // indirect
	golang.org/x/time v0.7.0 // indirect
	golang.org/x/tools v0.28.0 // indirect
	golang.zx2c4.com/wintun v0.0.0-20230126152724-0fa3db229ce2 // indirect
	google.golang.org/genproto/googleapis/rpc v0.0.0-20250324211829-b45e905df463 // indirect
	lukechampine.com/blake3 v1.3.0 // indirect
)
