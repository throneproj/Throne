package probe

import (
	"cmp"
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
	floor, startup time.Duration
}

// Alternative paths don't start in series; budget for the slower one.
func (r xrayProbeRoute) either(other xrayProbeRoute) xrayProbeRoute {
	return xrayProbeRoute{max(r.floor, other.floor), max(r.startup, other.startup)}
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
	if !udp && len(mask.Tcp) != 0 {
		return xrayProbeRoute{floor: finalMaskMinTimeout}
	}
	if !udp || len(mask.Udp) == 0 {
		return xrayProbeRoute{}
	}
	if slices.ContainsFunc(mask.Udp, func(m conf.Mask) bool { return strings.EqualFold(m.Type, "xdns") }) {
		return xrayProbeRoute{finalMaskMinTimeout, DNSTunnelStartupTimeout}
	}
	return xrayProbeRoute{floor: finalMaskMinTimeout}
}

func streamRoute(s *conf.StreamConfig) xrayProbeRoute {
	if network := cmp.Or(s.Method, s.Network); network != nil {
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
		case "xdrive", "xhttp", "splithttp":
			r := maskRoute(s.FinalMask, usesH3(s))
			http := cmp.Or(s.XHTTPSettings, s.SplitHTTPSettings)
			if http == nil || strings.EqualFold(string(*network), "xdrive") {
				return r
			}
			if http.Extra != nil {
				var extra conf.SplitHTTPConfig
				if json.Unmarshal(http.Extra, &extra) != nil {
					return r
				}
				http = &extra
			}
			if d := http.DownloadSettings; d != nil {
				return r.either(maskRoute(d.FinalMask, usesH3(d)))
			}
			return r
		}
	}
	return maskRoute(s.FinalMask, false)
}

// A batch can have several independent Xray instances. Match the generated bridge endpoint exactly.
func CollectXrayProbeRoutes(options *option.Options, config string, fullConfigs []string) XrayProbeRoutes {
	bridges := make(map[netip.AddrPort]xrayProbeRoute)
	collectMaskedBridges(bridges, config)
	for _, full := range fullConfigs {
		collectMaskedBridges(bridges, full)
	}
	if len(bridges) == 0 {
		return nil
	}
	routes := make(XrayProbeRoutes)
	for _, out := range options.Outbounds {
		if socks, ok := out.Options.(*option.SOCKSOutboundOptions); ok && out.Type == C.TypeSOCKS {
			ip, _ := netip.ParseAddr(socks.Server)
			if route, found := bridges[netip.AddrPortFrom(ip.Unmap(), socks.ServerPort)]; found {
				routes[out.Tag] = route
			}
		}
	}
	return routes
}

func collectMaskedBridges(bridges map[netip.AddrPort]xrayProbeRoute, xrayConfig string) {
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
				InboundTag conf.StringList
				conf.RouterRule
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
		anyMasked = anyMasked || hops[i].floor != 0
	}
	if !anyMasked {
		return
	}
	chain := func(start int) (route xrayProbeRoute) {
		seen := make(map[int]bool)
		for i, found := start, true; found && !seen[i]; {
			seen[i] = true
			route.floor = max(route.floor, hops[i].floor)
			route.startup += hops[i].startup
			socket := config.Outbounds[i].StreamSettings.SocketSettings
			if socket == nil {
				break
			}
			i, found = byTag[socket.DialerProxy]
		}
		return
	}
	fallback := chain(0) // Xray sends unmatched traffic to its first outbound, even without a tag.
	for _, in := range config.Inbounds {
		ip, _ := netip.ParseAddr(in.Listen)
		port, err := strconv.ParseUint(strings.Trim(string(in.Port), `"`), 10, 16)
		if in.Protocol != "socks" || !ip.IsLoopback() || err != nil {
			continue
		}
		tag := in.Tag
		route := fallback
		for _, rule := range config.Routing.Rules {
			if len(rule.InboundTag) != 0 && !slices.Contains(rule.InboundTag, tag) {
				continue
			}
			if tag == "throne-bridge" && rule.BalancerTag != "" {
				// The balancer selects at dial time; do not under-budget a slow member.
				for i := range hops {
					route = route.either(chain(i))
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
				route = route.either(candidate)
			}
		}
		if route.floor != 0 {
			bridges[netip.AddrPortFrom(ip.Unmap(), uint16(port))] = route
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
		timeout = max(timeout, route.floor)
		allowance += route.startup
	})
	return timeout, allowance
}
