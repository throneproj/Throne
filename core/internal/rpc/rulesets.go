package rpc

import (
	"context"

	"ThroneCore/gen"
	"ThroneCore/internal/rulesets"
)

func (s *server) UpdateRuleSets(ctx context.Context, in *gen.EmptyReq) (*gen.UpdateRuleSetsResponse, error) {
	box := currentBox()
	if box == nil {
		return &gen.UpdateRuleSetsResponse{Error: To("no instance is running")}, nil
	}
	updated, errorText := rulesets.UpdateAll(ctx, box.Router())
	return &gen.UpdateRuleSetsResponse{Updated: To(updated), Error: To(errorText)}, nil
}
