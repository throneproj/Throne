package rpc

import (
	"context"

	"ThroneCore/gen"

	"github.com/sagernet/sing-box/include"
)

func (s *server) PsiphonRegions(ctx context.Context, in *gen.EmptyReq) (*gen.PsiphonRegionsResponse, error) {
	return &gen.PsiphonRegionsResponse{Regions: include.PsiphonAvailableEgressRegions()}, nil
}
