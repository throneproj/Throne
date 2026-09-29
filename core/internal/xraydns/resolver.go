// Package xraydns resolves Xray outbound server domains through the sing-box instance in this same process.
//
// Speaking DNS to sing-box's `dns-in` inbound made outbound resolution depend on an inbound, and inbounds start
// only after the router initializes its rule-sets - so the first remote rule-set download deadlocked against the
// proxy server domain it needed to dial (#1811). Pinning `dns-direct` in DNSQueryOptions is what a sing-box
// outbound's own `domain_resolver` does, minus the socket.
package xraydns

import (
	"context"
	"sync"
	"time"

	"github.com/sagernet/sing-box/adapter"
	C "github.com/sagernet/sing-box/constant"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/service"
	"github.com/xtls/xray-core/common/net"
	dnsfeature "github.com/xtls/xray-core/features/dns"
)

// Matches tags::dnsDirect in the config builder (src/configs/generate.cpp).
const directTransportTag = "dns-direct"

const lookupTimeout = 5 * time.Second

// Read per lookup, not captured: the box is always built after the Xray instances that resolve through it.
type BoxProvider func() context.Context

type resolver struct {
	provider BoxProvider

	access    sync.Mutex
	transport adapter.DNSTransport
}

func New(provider BoxProvider) dnsfeature.Client {
	return &resolver{provider: provider}
}

func (*resolver) Type() interface{} { return dnsfeature.ClientType() }
func (*resolver) Start() error      { return nil }
func (*resolver) Close() error      { return nil }

func (r *resolver) LookupIP(domain string, option dnsfeature.IPOption) ([]net.IP, uint32, error) {
	if !option.IPv4Enable && !option.IPv6Enable {
		// Answering would defeat the dialer's own fallback pass.
		return nil, 0, dnsfeature.ErrEmptyResponse
	}
	ctx := r.provider()
	if ctx == nil {
		return nil, 0, E.New("throne-dns: sing-box instance is not up yet")
	}
	router := service.FromContext[adapter.DNSRouter](ctx)
	if router == nil {
		return nil, 0, E.New("throne-dns: sing-box DNS router is not registered")
	}
	addresses, err := router.Lookup(ctx, domain, adapter.DNSQueryOptions{
		Transport: r.directTransport(ctx),
		Strategy:  strategyFor(option),
		Timeout:   lookupTimeout,
	})
	if err != nil {
		return nil, 0, err
	}
	if len(addresses) == 0 {
		return nil, 0, dnsfeature.ErrEmptyResponse
	}
	ips := make([]net.IP, 0, len(addresses))
	for _, address := range addresses {
		// Unmap: net.IPAddress expects 4 bytes for a v4 answer.
		ips = append(ips, net.IP(address.Unmap().AsSlice()))
	}
	// LookupForIPWithClient discards the TTL and sing-box owns the cache.
	return ips, 0, nil
}

// nil leaves the query unpinned and on the config's own DNS rules, which is what a raw DNS object asks for.
func (r *resolver) directTransport(ctx context.Context) adapter.DNSTransport {
	r.access.Lock()
	defer r.access.Unlock()
	if r.transport != nil {
		return r.transport
	}
	manager := service.FromContext[adapter.DNSTransportManager](ctx)
	if manager == nil {
		return nil
	}
	transport, loaded := manager.Transport(directTransportTag)
	if !loaded {
		return nil
	}
	r.transport = transport
	return transport
}

func strategyFor(option dnsfeature.IPOption) C.DomainStrategy {
	switch {
	case option.IPv4Enable && option.IPv6Enable:
		// Defers to route.default_domain_resolver.strategy.
		return C.DomainStrategyAsIS
	case option.IPv4Enable:
		return C.DomainStrategyIPv4Only
	default:
		return C.DomainStrategyIPv6Only
	}
}
