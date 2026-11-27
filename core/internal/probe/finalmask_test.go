package probe

import (
	"context"
	"testing"
	"time"

	"ThroneCore/internal/boxmain"
	"github.com/sagernet/sing-box/option"
)

func TestFinalMaskProbeTimeout(t *testing.T) {
	const balancer = `{"inbounds":[{"tag":"throne-bridge","protocol":"socks","listen":"127.0.0.1","port":9000}],
		"outbounds":[{"tag":"direct"},{"tag":"slow","streamSettings":{"network":"kcp","finalmask":{"udp":[{"type":"xdns"}]}}}],
		"routing":{"rules":[{"inboundTag":["throne-bridge"],"balancerTag":"bal"}],"balancers":[{"tag":"bal","selector":["slow"]}]}}`
	const sing = `{"outbounds":[
		{"tag":"plain","type":"socks","server":"127.0.0.1","server_port":2000},
		{"tag":"chain","type":"socks","server":"127.0.0.1","server_port":2001},
		{"tag":"xdns","type":"socks","server":"127.0.0.1","server_port":2002},
		{"tag":"full","type":"socks","server":"127.0.0.1","server_port":2003},
		{"tag":"download","type":"socks","server":"127.0.0.1","server_port":2004},
		{"tag":"wrong-layer","type":"socks","server":"127.0.0.1","server_port":2006},
		{"tag":"balancer","type":"socks","server":"127.0.0.1","server_port":9000},
		{"tag":"remote","type":"socks","server":"192.0.2.77","server_port":2002},
		{"tag":"xdns-alias","type":"socks","server":"::ffff:127.0.0.1","server_port":2002}]}`
	const generated = `{
		"inbounds":[{"tag":"a","protocol":"socks","listen":"127.0.0.1","port":2000},
			{"tag":"b","protocol":"socks","listen":"127.0.0.1","port":2001},
			{"tag":"c","protocol":"socks","listen":"127.0.0.1","port":2002},
			{"tag":"d","protocol":"socks","listen":"127.0.0.1","port":2004},
			{"tag":"e","protocol":"socks","listen":"127.0.0.1","port":2006}],
		"routing":{"rules":[{"inboundTag":["a"],"outboundTag":"plain"},{"inboundTag":"b","outboundTag":"chain"},
			{"inboundTag":"unused,c","outboundTag":"xdns"},{"inboundTag":"d","outboundTag":"download"},
			{"inboundTag":"e","outboundTag":"wrong-layer"}]},
		"outbounds":[{"tag":"plain","streamSettings":{"finalmask":{"tcp":[]}}},
			{"tag":"chain","streamSettings":{"sockopt":{"dialerProxy":"masked"}}},
			{"tag":"masked","streamSettings":{"finalmask":{"tcp":[{"type":"xmc"}]}}},
			{"tag":"xdns","streamSettings":{"network":"kcp","finalmask":{"udp":[{"type":"xdns"}]}}},
			{"tag":"wrong-layer","streamSettings":{"network":"tcp","finalmask":{"udp":[{"type":"xdns"}]}}},
			{"tag":"download","streamSettings":{"network":"xhttp","xhttpSettings":{"extra":{"downloadSettings":{
				"security":"tls","tlsSettings":{"alpn":["h3"]},"finalmask":{"udp":[{"type":"xdns"}]}}}}}}]
	}`
	const full = `{"inbounds":[{"tag":"throne-bridge","protocol":"socks","listen":"127.0.0.1","port":2003}],
		"outbounds":[{"streamSettings":{"network":"kcp","finalmask":{"udp":[{"type":"xdns"}]}}}]}`
	var routes XrayProbeRoutes
	box, cancel, err := boxmain.Create([]byte(sing), func(ctx context.Context) { routes.Register(ctx) }, func(opts *option.Options) {
		routes = CollectXrayProbeRoutes(opts, generated, []string{full, balancer})
	})
	if err != nil {
		t.Fatal(err)
	}
	defer box.CloseWithTimeout(cancel, time.Second, t.Log, true)
	for _, tc := range []struct {
		tag                 string
		set, first, regular time.Duration
	}{
		{"plain", 3, 3, 3}, {"chain", 3, 10, 10}, {"chain", 15, 15, 15},
		{"xdns", 3, 40, 10}, {"xdns-alias", 3, 40, 10}, {"full", 3, 40, 10}, {"download", 3, 40, 10},
		{"balancer", 3, 40, 10}, {"remote", 5, 5, 5}, {"wrong-layer", 5, 5, 5},
	} {
		first, regular := requestTimeouts(box, tc.tag, false, tc.set*time.Second)
		if first != tc.first*time.Second || regular != tc.regular*time.Second {
			t.Errorf("%s (%ds): got %v, %v; want %ds, %ds", tc.tag, tc.set, first, regular, tc.first, tc.regular)
		}
	}
}
