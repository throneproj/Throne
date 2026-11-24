package warpprobe

import (
	"context"
	"errors"
	"fmt"
	"net"
	"net/netip"
	"slices"
	"sync"
	"syscall"
	"time"

	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/include"
	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing-box/protocol/warpmasque"
	"github.com/sagernet/sing-box/protocol/wireguard"
	"github.com/sagernet/sing/common/control"
	singjson "github.com/sagernet/sing/common/json"
	"github.com/sagernet/sing/common/ntp"
	"github.com/sagernet/sing/service"
)

var optionsContext = sync.OnceValue(func() context.Context {
	return include.Context(context.Background())
})

// Prober handshakes with WARP endpoints the way one profile's endpoint does; it is safe for concurrent use.
type Prober struct {
	wireGuard *option.WireGuardEndpointOptions
	masque    *option.WARPMASQUEEndpointOptions
}

type Result struct {
	Sent     int
	Answered int
	// Of the answered handshakes.
	Median time.Duration
	// The first error other than a timeout, else the last timeout.
	Err error
}

// New decodes a wireguard or masque endpoint object; ctx carries the services the probes will read (see WithServices).
func New(ctx context.Context, configJSON string) (*Prober, error) {
	endpoint, err := singjson.UnmarshalExtendedContext[option.Endpoint](optionsContext(), []byte(configJSON))
	if err != nil {
		return nil, fmt.Errorf("decode warp endpoint: %w", err)
	}
	prober := &Prober{}
	switch options := endpoint.Options.(type) {
	case *option.WireGuardEndpointOptions:
		prober.wireGuard = options
	case *option.WARPMASQUEEndpointOptions:
		// Only the HTTP/2 leg, alone or as the fallback, fetches a missing ECH config, and the fetch dereferences the box's
		// DNS and network services.
		if (options.HTTPVersion == 2 || !options.DisableVersionFallback) && dynamicECH(options) && !echServices(ctx) {
			return nil, errors.New("the endpoint's ech config is fetched through dns, which needs a running instance")
		}
		prober.masque = options
	default:
		return nil, fmt.Errorf("expected a wireguard or masque endpoint, got %q", endpoint.Type)
	}
	if err = prober.dryRun(ctx); err != nil {
		return nil, fmt.Errorf("warp endpoint: %w", err)
	}
	return prober, nil
}

func dynamicECH(options *option.WARPMASQUEEndpointOptions) bool {
	tls := options.TLS
	return tls != nil && tls.ECH != nil && tls.ECH.Enabled && len(tls.ECH.Config) == 0 && tls.ECH.ConfigPath == ""
}

func echServices(ctx context.Context) bool {
	return service.FromContext[adapter.DNSRouter](ctx) != nil && service.FromContext[adapter.DNSTransportManager](ctx) != nil &&
		service.FromContext[adapter.NetworkManager](ctx) != nil
}

var errDryRun = errors.New("dry run")

// Every probe socket is created through the listener control, so failing it checks the options without sending anything.
func (p *Prober) dryRun(ctx context.Context) error {
	ctx, cancel := context.WithTimeout(ctx, time.Second)
	defer cancel()
	_, err := p.Handshake(ctx, netip.AddrPortFrom(netip.AddrFrom4([4]byte{127, 0, 0, 1}), 9), func(string, string, syscall.RawConn) error {
		return errDryRun
	})
	if err == nil || ctx.Err() != nil || reachedSockets(err) {
		return nil
	}
	return err
}

// Raced HTTP/3 and HTTP/2 legs fail with an error each, and an option error in either must not pass.
func reachedSockets(err error) bool {
	var legs interface{ Unwrap() []error }
	if errors.As(err, &legs) {
		return !slices.ContainsFunc(legs.Unwrap(), func(leg error) bool { return !errors.Is(leg, errDryRun) })
	}
	return errors.Is(err, errDryRun)
}

// DefaultPort is the port a target without one is probed on.
func (p *Prober) DefaultPort() uint16 {
	if p.masque != nil {
		return 443
	}
	return 2408
}

// Handshake runs one handshake from a fresh socket created through listenerControl; ctx bounds the wait.
func (p *Prober) Handshake(ctx context.Context, target netip.AddrPort, listenerControl control.Func) (time.Duration, error) {
	if p.masque != nil {
		return warpmasque.ProbeHandshake(ctx, *p.masque, target, listenerControl)
	}
	return wireguard.ProbeHandshake(ctx, *p.wireGuard, target, listenerControl)
}

// Probe runs attempts handshakes in sequence, each waiting up to timeout.
func (p *Prober) Probe(ctx context.Context, target netip.AddrPort, listenerControl control.Func, attempts int, timeout time.Duration) Result {
	var (
		result Result
		rtts   []time.Duration
	)
	for range attempts {
		if ctx.Err() != nil {
			break
		}
		attemptCtx, cancel := context.WithTimeout(ctx, timeout)
		rtt, err := p.Handshake(attemptCtx, target, listenerControl)
		cancel()
		if err == nil {
			result.Sent++
			rtts = append(rtts, rtt)
			continue
		}
		// An attempt cut short by ctx itself measured nothing.
		if ctx.Err() != nil {
			break
		}
		result.Sent++
		if result.Err == nil || isTimeout(result.Err) {
			result.Err = err
		}
	}
	result.Answered = len(rtts)
	result.Median = median(rtts)
	return result
}

func isTimeout(err error) bool {
	var netErr net.Error
	return errors.As(err, &netErr) && netErr.Timeout()
}

func median(values []time.Duration) time.Duration {
	if len(values) == 0 {
		return 0
	}
	slices.Sort(values)
	middle := len(values) / 2
	if len(values)%2 == 1 {
		return values[middle]
	}
	return (values[middle-1] + values[middle]) / 2
}

// WithServices gives ctx a running box's services that a profile's TLS reads: the ECH fetch's DNS, NTP time and the
// certificate store.
func WithServices(ctx, box context.Context) context.Context {
	ctx = service.ContextWithRegistry(ctx, service.NewRegistry())
	copyService[adapter.DNSRouter](ctx, box)
	copyService[adapter.DNSTransportManager](ctx, box)
	copyService[adapter.NetworkManager](ctx, box)
	copyService[ntp.TimeService](ctx, box)
	copyService[adapter.CertificateStore](ctx, box)
	return ctx
}

func copyService[T any](ctx, from context.Context) {
	if value := service.FromContext[T](from); any(value) != nil {
		service.MustRegister(ctx, value)
	}
}
