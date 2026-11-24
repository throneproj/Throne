package probe

import (
	"context"
	"testing"
	"time"

	"github.com/sagernet/sing-box/adapter"
	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing/service"
)

type probeTestBox struct {
	ctx context.Context
	adapter.OutboundManager
}

func TestFullConfigBalancerProbeTimeout(t *testing.T) {
	const config = `{"inbounds":[{"tag":"throne-bridge","protocol":"socks","listen":"127.0.0.1","port":9000}],
		"outbounds":[{"tag":"direct"},{"tag":"slow","streamSettings":{"network":"kcp","finalmask":{"udp":[{"type":"xdns"}]}}}],
		"routing":{"rules":[{"inboundTag":["throne-bridge"],"balancerTag":"bal"}],"balancers":[{"tag":"bal","selector":["slow"]}]}}`
	s := new(option.SOCKSOutboundOptions)
	s.Server, s.ServerPort = "127.0.0.1", 9000
	o := option.Options{Outbounds: []option.Outbound{{Tag: "full", Type: C.TypeSOCKS, Options: s}}}
	got := CollectXrayProbeRoutes(&o, "", []string{config})["full"]
	if !got.masked || got.xdnsHops != 1 {
		t.Fatalf("full-config balancer: got %+v", got)
	}
}

func (b probeTestBox) Context() context.Context          { return b.ctx }
func (b probeTestBox) Outbound() adapter.OutboundManager { return b.OutboundManager }

type probeTestManager struct{ adapter.OutboundManager }
type probeTestOutbound struct {
	adapter.Outbound
	kind string
	deps []string
}

func (probeTestManager) Outbound(tag string) (adapter.Outbound, bool) {
	switch tag {
	case "dnstt-chain":
		return probeTestOutbound{kind: C.TypeDNSTT, deps: []string{"chain"}}, true
	case "masterdns":
		return probeTestOutbound{kind: C.TypeMasterDNS}, true
	default:
		return probeTestOutbound{kind: C.TypeSOCKS}, true
	}
}
func (o probeTestOutbound) Type() string           { return o.kind }
func (o probeTestOutbound) Dependencies() []string { return o.deps }

func TestFinalMaskProbeTimeout(t *testing.T) {
	var options option.Options
	for i, tag := range []string{"plain", "chain", "xdns", "full", "download", "remote", "wrong-layer"} {
		socks := new(option.SOCKSOutboundOptions)
		socks.Server = "127.0.0.1"
		socks.ServerPort = uint16(2000 + i)
		if tag == "remote" {
			socks.Server, socks.ServerPort = "192.0.2.77", 2002
		}
		options.Outbounds = append(options.Outbounds, option.Outbound{Type: C.TypeSOCKS, Tag: tag, Options: socks})
	}
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
	box := probeTestBox{service.ContextWithDefaultRegistry(context.Background()), probeTestManager{}}
	CollectXrayProbeRoutes(&options, generated, []string{full}).Register(box.Context())
	for _, tc := range []struct {
		tag                 string
		set, first, regular time.Duration
	}{
		{"plain", 3, 3, 3}, {"chain", 3, 10, 10}, {"chain", 15, 15, 15},
		{"xdns", 3, 40, 10}, {"full", 3, 40, 10}, {"download", 3, 40, 10},
		{"remote", 5, 5, 5}, {"wrong-layer", 5, 5, 5},
		{"dnstt-chain", 3, 40, 10}, {"masterdns", 3, 93, 3},
	} {
		first, regular := requestTimeouts(box, tc.tag, false, tc.set*time.Second)
		if first != tc.first*time.Second || regular != tc.regular*time.Second {
			t.Errorf("%s (%ds): got %v, %v; want %ds, %ds", tc.tag, tc.set, first, regular, tc.first, tc.regular)
		}
	}
}
