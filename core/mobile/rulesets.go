package mobile

import (
	"ThroneCore/internal/rulesets"
)

type RuleSetUpdateResult struct {
	Updated int32
	Error   string
}

func (i *Instance) UpdateRuleSets() (*RuleSetUpdateResult, error) {
	if !i.running() {
		return nil, errInstanceNotRunning
	}
	updated, errorText := rulesets.UpdateAll(i.handle.ctx, i.handle.Router())
	return &RuleSetUpdateResult{Updated: updated, Error: errorText}, nil
}
