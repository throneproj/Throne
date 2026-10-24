package resolve

import (
	"context"
	"sync"
	"time"

	"github.com/sagernet/sing-box/adapter"
	C "github.com/sagernet/sing-box/constant"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/service"
)

// Matches tags::dnsDirect in the config builders (src/configs/generate.cpp, Android's Tags.DNS_DIRECT).
const DirectDNSTag = "dns-direct"

const (
	lookupTimeout     = 5 * time.Second
	lookupConcurrency = 16
)

type Result struct {
	Domain    string
	Addresses []string
	Error     error
}

// Starts a probe box and returns its service context and the cleanup that closes it.
type ProbeStarter func() (ctx context.Context, close func(), err error)

// Pinned to dns-direct: the OS resolver is what a kill switch blocks, and a running Tun answers it from fakeip.
// running is the service context of the running box, nil when none runs; startProbe is called only when running
// has no dns-direct. Results are in request order.
func Domains(running context.Context, startProbe ProbeStarter, domains []string) ([]Result, error) {
	if len(domains) == 0 {
		return nil, nil
	}
	ctx := running
	transport := directTransport(ctx)
	if transport == nil {
		probeCtx, closeProbe, err := startProbe()
		if err != nil {
			return nil, err
		}
		defer closeProbe()
		ctx = probeCtx
		if transport = directTransport(ctx); transport == nil {
			return nil, E.New("the probe config has no ", DirectDNSTag, " server")
		}
	}
	router := service.FromContext[adapter.DNSRouter](ctx)
	if router == nil {
		return nil, E.New("the sing-box DNS router is not registered")
	}

	results := make([]Result, len(domains))
	semaphore := make(chan struct{}, lookupConcurrency)
	var wg sync.WaitGroup
	for i, domain := range domains {
		wg.Add(1)
		semaphore <- struct{}{}
		go func() {
			defer func() {
				<-semaphore
				wg.Done()
			}()
			results[i] = lookup(ctx, router, transport, domain)
		}()
	}
	wg.Wait()
	return results, nil
}

func lookup(ctx context.Context, router adapter.DNSRouter, transport adapter.DNSTransport, domain string) Result {
	result := Result{Domain: domain}
	// AsIS defers to route.default_domain_resolver's strategy, the one outbounds dial their servers with.
	addresses, err := router.Lookup(ctx, domain, adapter.DNSQueryOptions{
		Transport: transport,
		Strategy:  C.DomainStrategyAsIS,
		Timeout:   lookupTimeout,
	})
	if err != nil {
		result.Error = err
		return result
	}
	for _, address := range addresses {
		result.Addresses = append(result.Addresses, address.Unmap().String())
	}
	return result
}

// nil when the box has none, which a raw DNS object may leave out.
func directTransport(ctx context.Context) adapter.DNSTransport {
	if ctx == nil {
		return nil
	}
	manager := service.FromContext[adapter.DNSTransportManager](ctx)
	if manager == nil {
		return nil
	}
	transport, loaded := manager.Transport(DirectDNSTag)
	if !loaded {
		return nil
	}
	return transport
}
