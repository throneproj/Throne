package probe

import (
	"encoding/json"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/sagernet/sing-box/adapter"
	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/option"
)

// Matches the GUI's DnsTunnelStartupAllowanceMs: a DNS tunnel opens its session over many slow queries.
const (
	DNSTunnelStartupTimeout = 30 * time.Second
	MasterDNSStartupTimeout = 90 * time.Second
)

// Box → map[string]int from XrayDNSTunnelHops.
var xrayDNSTunnels sync.Map

// TrackDNSTunnels lets first requests through i wait for the Xray xdns hops; the returned func forgets them.
func TrackDNSTunnels(i Box, xrayHops map[string]int) func() {
	if len(xrayHops) == 0 {
		return func() {}
	}
	xrayDNSTunnels.Store(i, xrayHops)
	return func() { xrayDNSTunnels.Delete(i) }
}

type xrayOutboundShape struct {
	Tag            string `json:"tag"`
	StreamSettings struct {
		Finalmask struct {
			UDP []struct {
				Type string `json:"type"`
			} `json:"udp"`
		} `json:"finalmask"`
		Sockopt struct {
			DialerProxy string `json:"dialerProxy"`
		} `json:"sockopt"`
	} `json:"streamSettings"`
}

type xrayInboundShape struct {
	Tag  string          `json:"tag"`
	Port json.RawMessage `json:"port"`
}

type xrayRuleShape struct {
	InboundTag  json.RawMessage `json:"inboundTag"`
	OutboundTag string          `json:"outboundTag"`
}

// XrayDNSTunnelHops maps each sing-box socks bridge into Xray to the xdns hops of the Xray chain behind it;
// the bridge reaches that chain through the Xray inbound on its server port.
func XrayDNSTunnelHops(options *option.Options, xrayConfig string) map[string]int {
	var config struct {
		Inbounds  []json.RawMessage `json:"inbounds"`
		Outbounds []json.RawMessage `json:"outbounds"`
		Routing   struct {
			Rules []json.RawMessage `json:"rules"`
		} `json:"routing"`
	}
	if xrayConfig == "" || json.Unmarshal([]byte(xrayConfig), &config) != nil {
		return nil
	}
	xdns := make(map[string]bool)
	next := make(map[string]string)
	for _, raw := range config.Outbounds {
		var outbound xrayOutboundShape
		if json.Unmarshal(raw, &outbound) != nil || outbound.Tag == "" {
			continue
		}
		next[outbound.Tag] = outbound.StreamSettings.Sockopt.DialerProxy
		for _, mask := range outbound.StreamSettings.Finalmask.UDP {
			if strings.EqualFold(mask.Type, "xdns") {
				xdns[outbound.Tag] = true
			}
		}
	}
	if len(xdns) == 0 {
		return nil
	}
	inboundAt := make(map[uint16]string)
	for _, raw := range config.Inbounds {
		var inbound xrayInboundShape
		if json.Unmarshal(raw, &inbound) != nil {
			continue
		}
		if port, err := strconv.ParseUint(strings.Trim(string(inbound.Port), `"`), 10, 16); err == nil {
			inboundAt[uint16(port)] = inbound.Tag
		}
	}
	routedTo := make(map[string]string)
	for _, raw := range config.Routing.Rules {
		var rule xrayRuleShape
		if json.Unmarshal(raw, &rule) != nil {
			continue
		}
		var inboundTags []string
		if json.Unmarshal(rule.InboundTag, &inboundTags) != nil {
			var single string
			if json.Unmarshal(rule.InboundTag, &single) != nil {
				continue
			}
			inboundTags = []string{single}
		}
		for _, tag := range inboundTags {
			if _, routed := routedTo[tag]; !routed {
				routedTo[tag] = rule.OutboundTag
			}
		}
	}
	hops := make(map[string]int)
	for _, outbound := range options.Outbounds {
		socks, isSOCKS := outbound.Options.(*option.SOCKSOutboundOptions)
		if outbound.Type != C.TypeSOCKS || !isSOCKS {
			continue
		}
		inboundTag, bridged := inboundAt[socks.ServerPort]
		if !bridged {
			continue
		}
		count := 0
		visited := make(map[string]bool)
		for hop := routedTo[inboundTag]; hop != "" && !visited[hop]; hop = next[hop] {
			visited[hop] = true
			if xdns[hop] {
				count++
			}
		}
		if count > 0 {
			hops[outbound.Tag] = count
		}
	}
	return hops
}

// Stacked tunnels start one after another, so their allowances add up.
func dnsTunnelAllowance(i Box, tag string) time.Duration {
	var xrayHops map[string]int
	if tracked, loaded := xrayDNSTunnels.Load(i); loaded {
		xrayHops = tracked.(map[string]int)
	}
	var allowance time.Duration
	walkChain(i, tag, func(tag string, outbound adapter.Outbound) {
		switch outbound.Type() {
		case C.TypeDNSTT:
			allowance += DNSTunnelStartupTimeout
		case C.TypeMasterDNS:
			allowance += MasterDNSStartupTimeout
		}
		allowance += time.Duration(xrayHops[tag]) * DNSTunnelStartupTimeout
	})
	return allowance
}
