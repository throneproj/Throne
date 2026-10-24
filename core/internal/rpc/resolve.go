package rpc

import (
	"context"

	"ThroneCore/gen"
	"ThroneCore/internal/resolve"
)

func (s *server) ResolveDomains(ctx context.Context, in *gen.ResolveDomainsRequest) (*gen.ResolveDomainsResponse, error) {
	var running context.Context
	if box := currentBox(); box != nil {
		running = box.Context()
	}
	results, err := resolve.Domains(running, func() (context.Context, func(), error) {
		env, err := prepareTestEnv(false, false, "", nil, in.GetConfig(), nil, false, "")
		if err != nil {
			return nil, nil, err
		}
		return env.box.Context(), env.close, nil
	}, in.GetDomains())
	if err != nil {
		return nil, err
	}
	response := &gen.ResolveDomainsResponse{}
	for _, result := range results {
		resolved := &gen.ResolvedDomain{Domain: To(result.Domain), Addresses: result.Addresses}
		if result.Error != nil {
			resolved.Error = To(result.Error.Error())
		}
		response.Results = append(response.Results, resolved)
	}
	return response, nil
}
