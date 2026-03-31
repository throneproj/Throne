//go:build with_naive_outbound
package main

import (
	"ThroneCore/gen"
	"context"
)

func (s *server) CheckNaive(ctx context.Context, _ *gen.EmptyReq) (*gen.CheckNaiveResponse, error) {
	return &gen.CheckNaiveResponse{HasNaive: To(true)}, nil
}
