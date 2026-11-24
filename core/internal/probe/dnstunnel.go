package probe

import (
	"context"
	"encoding/json"
	"net/netip"
	"slices"
	"strconv"
	"strings"
	"time"

	"github.com/sagernet/sing-box/adapter"
	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing/service"
	"github.com/xtls/xray-core/infra/conf"
)

// Matches the GUI's DnsTunnelStartupAllowanceMs: a DNS tunnel opens its session over many slow queries.
const (
	DNSTunnelStartupTimeout = 30 * time.Second
	MasterDNSStartupTimeout = 90 * time.Second
	finalMaskMinTimeout     = 10 * time.Second
)

type xrayProbeRoute struct {
	xdnsHops int
	masked   bool
}

type XrayProbeRoutes map[string]xrayProbeRoute

func (routes XrayProbeRoutes) Register(ctx context.Context) {
	if len(routes) > 0 {
		service.MustRegister(ctx, routes)
	}
}

func usesH3(s *conf.StreamConfig) bool {
	return strings.EqualFold(s.Security, "tls") && s.TLSSettings != nil && s.TLSSettings.ALPN != nil &&
		slices.Equal(*s.TLSSettings.ALPN, conf.StringList{"h3"})
}

func maskRoute(mask *conf.FinalMask, udp bool) xrayProbeRoute {
	if mask == nil {
		return xrayProbeRoute{}
	}
	if !udp {
		return xrayProbeRoute{masked: len(mask.Tcp) > 0}
	}
	for _, m := range mask.Udp {
		if strings.EqualFold(m.Type, "xdns") {
			return xrayProbeRoute{masked: true, xdnsHops: 1}
		}
	}
	return xrayProbeRoute{masked: len(mask.Udp) > 0}
}

func streamRoute(s *conf.StreamConfig) xrayProbeRoute {
	network := s.Network
	if s.Method != nil {
		network = s.Method
	}
	if network != nil {
		switch strings.ToLower(string(*network)) {
		case "mkcp", "kcp", "hysteria":
			return maskRoute(s.FinalMask, true)
		case "masque":
			udp := true
			if s.TLSSettings != nil && s.TLSSettings.ALPN != nil {
				alpn := *s.TLSSettings.ALPN
				udp = !slices.Contains(alpn, "h2") || slices.Contains(alpn, "h3")
			}
			return maskRoute(s.FinalMask, udp)
		case "xdrive":
			return maskRoute(s.FinalMask, usesH3(s))
		case "xhttp", "splithttp":
			r := maskRoute(s.FinalMask, usesH3(s))
			http := s.XHTTPSettings
			if http == nil {
				http = s.SplitHTTPSettings
			}
			if http != nil {
				if http.Extra != nil {
					var extra conf.SplitHTTPConfig
					if json.Unmarshal(http.Extra, &extra) != nil {
						return r
					}
					http = &extra
				}
				if d := http.DownloadSettings; d != nil {
					down := maskRoute(d.FinalMask, usesH3(d))
					r.masked = r.masked || down.masked
					r.xdnsHops = max(r.xdnsHops, down.xdnsHops)
				}
			}
			return r
		}
	}
	return maskRoute(s.FinalMask, false)
}

// A batch can have several independent Xray instances. Match the generated bridge endpoint exactly.
func CollectXrayProbeRoutes(options *option.Options, config string, fullConfigs []string) XrayProbeRoutes {
	routes := make(XrayProbeRoutes)
	routes.collect(options, config)
	for _, full := range fullConfigs {
		routes.collect(options, full)
	}
	return routes
}

func (routes XrayProbeRoutes) collect(options *option.Options, xrayConfig string) {
	var config struct {
		Inbounds []struct {
			Tag, Listen, Protocol string
			Port                  json.RawMessage
		}
		Outbounds []struct {
			Tag            string
			StreamSettings conf.StreamConfig
		}
		Routing struct {
			Rules []struct {
				InboundTag  conf.StringList
				OutboundTag string
				BalancerTag string
			}
		}
	}
	if xrayConfig == "" || json.Unmarshal([]byte(xrayConfig), &config) != nil || len(config.Outbounds) == 0 {
		return
	}
	byTag := make(map[string]int)
	hops := make([]xrayProbeRoute, len(config.Outbounds))
	var anyMasked bool
	for i, out := range config.Outbounds {
		if out.Tag != "" {
			byTag[out.Tag] = i
		}
		hops[i] = streamRoute(&out.StreamSettings)
		anyMasked = anyMasked || hops[i].masked
	}
	if !anyMasked {
		return
	}
	inboundAt := make(map[netip.AddrPort]string)
	for _, in := range config.Inbounds {
		ip, err := netip.ParseAddr(in.Listen)
		if err != nil || !ip.IsLoopback() || in.Protocol != "socks" {
			continue
		}
		if port, err := strconv.ParseUint(strings.Trim(string(in.Port), `"`), 10, 16); err == nil {
			inboundAt[netip.AddrPortFrom(ip.Unmap(), uint16(port))] = in.Tag
		}
	}
	chain := func(start int) (route xrayProbeRoute) {
		seen := make(map[int]bool)
		for i := start; !seen[i]; {
			seen[i] = true
			route.masked = route.masked || hops[i].masked
			route.xdnsHops += hops[i].xdnsHops
			socket := config.Outbounds[i].StreamSettings.SocketSettings
			if socket == nil || socket.DialerProxy == "" {
				break
			}
			next, ok := byTag[socket.DialerProxy]
			if !ok {
				break
			}
			i = next
		}
		return
	}
	for _, out := range options.Outbounds {
		socks, ok := out.Options.(*option.SOCKSOutboundOptions)
		if !ok || out.Type != C.TypeSOCKS {
			continue
		}
		ip, err := netip.ParseAddr(socks.Server)
		if err != nil {
			continue
		}
		tag, ok := inboundAt[netip.AddrPortFrom(ip.Unmap(), socks.ServerPort)]
		if !ok {
			continue
		}
		route := chain(0) // Xray sends unmatched traffic to its first outbound, even when it has no tag.
		for _, rule := range config.Routing.Rules {
			if len(rule.InboundTag) != 0 && !slices.Contains(rule.InboundTag, tag) {
				continue
			}
			if tag == "throne-bridge" && rule.BalancerTag != "" {
				// The balancer selects at dial time; do not under-budget a slow member.
				for i := range hops {
					candidate := chain(i)
					route.masked = route.masked || candidate.masked
					route.xdnsHops = max(route.xdnsHops, candidate.xdnsHops)
				}
				break
			}
			if i, found := byTag[rule.OutboundTag]; found {
				candidate := chain(i)
				if tag != "throne-bridge" {
					route = candidate
					break
				}
				// In a user-supplied full config a rule can also depend on the destination.
				route.masked = route.masked || candidate.masked
				route.xdnsHops = max(route.xdnsHops, candidate.xdnsHops)
			}
		}
		if route.masked {
			routes[out.Tag] = route
		}
	}
}

// Stacked tunnels start one after another, so their allowances add up.
func routeProbeTimeout(i Box, tag string, timeout time.Duration) (regular, startup time.Duration) {
	routes := service.FromContext[XrayProbeRoutes](i.Context())
	var allowance time.Duration
	walkChain(i, tag, func(tag string, outbound adapter.Outbound) {
		switch outbound.Type() {
		case C.TypeDNSTT:
			allowance += DNSTunnelStartupTimeout
		case C.TypeMasterDNS:
			allowance += MasterDNSStartupTimeout
		}
		route := routes[tag]
		if route.masked {
			timeout = max(timeout, finalMaskMinTimeout)
		}
		allowance += time.Duration(route.xdnsHops) * DNSTunnelStartupTimeout
	})
	return timeout, allowance
}
