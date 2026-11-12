package mobile

import (
	"context"

	"ThroneCore/internal/resolve"

	E "github.com/sagernet/sing/common/exceptions"
)

// The desktop's ResolveDomains RPC. The domain list stays unexported because gomobile binds no slice but []byte.
type ResolveRequest struct {
	// The probe box config, started only when no instance runs or the running config has no dns-direct.
	CoreConfig string
	domains    []string
}

func (r *ResolveRequest) AddDomain(domain string) {
	r.domains = append(r.domains, domain)
}

type ResolvedDomain struct {
	Domain string
	// Empty when the lookup answered; Addresses may still be empty then.
	Error     string
	addresses []string
}

func (d *ResolvedDomain) Addresses() StringIterator {
	return newIterator(d.addresses)
}

type ResolvedDomainIterator interface {
	Len() int32
	HasNext() bool
	Next() *ResolvedDomain
}

// Blocks until every lookup has answered or timed out (5 s each, 16 at a time); results are in request order.
// current may be nil; platform wires protect and the local DNS transport into the probe box.
func ResolveDomains(current *Instance, platform PlatformInterface, request *ResolveRequest) (ResolvedDomainIterator, error) {
	if request == nil {
		return nil, E.New("nil request")
	}
	var running context.Context
	if current != nil && current.running() {
		running = current.handle.ctx
	}
	results, err := resolve.Domains(running, func() (context.Context, func(), error) {
		env, err := prepareTestEnv(nil, false, platform, &TestRequest{CoreConfig: request.CoreConfig})
		if err != nil {
			return nil, nil, err
		}
		return env.box.Context(), env.close, nil
	}, request.domains)
	if err != nil {
		return nil, err
	}
	resolved := make([]*ResolvedDomain, len(results))
	for i, result := range results {
		resolved[i] = &ResolvedDomain{Domain: result.Domain, Error: errorString(result.Error), addresses: result.Addresses}
	}
	return newIterator(resolved), nil
}
