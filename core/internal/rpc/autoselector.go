package rpc

import (
	"context"
	"time"

	"ThroneCore/gen"

	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/protocol/group"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/service"
)

// The live box owns them, so an absent box simply means "none".
func autoSelectors() map[string]group.AutoSelectorGroup {
	found := make(map[string]group.AutoSelectorGroup)
	box := currentBox()
	if box == nil {
		return found
	}
	outbounds := service.FromContext[adapter.OutboundManager](box.Context())
	if outbounds == nil {
		return found
	}
	for _, outbound := range outbounds.Outbounds() {
		if selector, isAuto := outbound.(group.AutoSelectorGroup); isAuto {
			found[outbound.Tag()] = selector
		}
	}
	return found
}

func unixMilliOrZero(t time.Time) int64 {
	if t.IsZero() {
		return 0
	}
	return t.UnixMilli()
}

func autoSelectorStatusToProto(status group.AutoSelectorStatus) *gen.AutoSelectorStatus {
	out := &gen.AutoSelectorStatus{
		Tag:              To(status.Tag),
		Phase:            To(status.Phase),
		Selected:         To(status.Selected),
		SelectedUdp:      To(status.SelectedUDP),
		Pinned:           To(status.Pinned),
		Balance:          To(status.Balance),
		BalanceMode:      To(status.BalanceMode),
		Suspended:        To(status.Suspended),
		SuspendedSinceMs: To(unixMilliOrZero(status.SuspendedSince)),
		MembersTotal:     To(int32(status.MembersTotal)),
		MembersProbed:    To(int32(status.MembersProbed)),
		MembersAlive:     To(int32(status.MembersAlive)),
		MembersQualified: To(int32(status.MembersQualified)),
		MembersCooldown:  To(int32(status.MembersCooldown)),
		ProbesInFlight:   To(int32(status.ProbesInFlight)),
		RoundsCompleted:  To(int32(status.RoundsCompleted)),
		LastRoundMs:      To(unixMilliOrZero(status.LastRoundAt)),
		NextRoundMs:      To(unixMilliOrZero(status.NextRoundAt)),
		LastSwitchMs:     To(unixMilliOrZero(status.LastSwitchAt)),
		LastSwitchReason: To(status.LastSwitchReason),
	}
	out.Members = make([]*gen.AutoSelectorMember, 0, len(status.Members))
	for _, member := range status.Members {
		out.Members = append(out.Members, &gen.AutoSelectorMember{
			Tag:             To(member.Tag),
			Rank:            To(int32(member.Rank)),
			State:           To(member.State),
			Selected:        To(member.Selected),
			SelectedUdp:     To(member.SelectedUDP),
			Qualified:       To(member.Qualified),
			Active:          To(member.Active),
			AverageMs:       To(int32(member.AverageMs)),
			DeviationMs:     To(int32(member.DeviationMs)),
			MinMs:           To(int32(member.MinMs)),
			MaxMs:           To(int32(member.MaxMs)),
			Samples:         To(int32(member.Samples)),
			Failures:        To(int32(member.Failures)),
			Probes:          To(int32(member.Probes)),
			DialTotal:       To(int32(member.DialTotal)),
			DialFail:        To(int32(member.DialFail)),
			LastOkMs:        To(unixMilliOrZero(member.LastOK)),
			LastProbeMs:     To(unixMilliOrZero(member.LastProbe)),
			CooldownUntilMs: To(unixMilliOrZero(member.CooldownUntil)),
			LastError:       To(member.LastError),
		})
	}
	return out
}

// Idempotent: unlike QueryStats it clears no core-side counters, so any number of consumers may poll it.
func (s *server) QueryAutoSelectors(ctx context.Context, _ *gen.EmptyReq) (*gen.QueryAutoSelectorsResponse, error) {
	out := &gen.QueryAutoSelectorsResponse{}
	for _, selector := range autoSelectors() {
		out.Groups = append(out.Groups, autoSelectorStatusToProto(selector.Status()))
	}
	return out, nil
}

func (s *server) AutoSelectorAction(ctx context.Context, in *gen.AutoSelectorActionRequest) (*gen.ErrorResp, error) {
	selectors := autoSelectors()
	if len(selectors) == 0 {
		return &gen.ErrorResp{Error: To("no auto selector is running")}, nil
	}

	targets := make([]group.AutoSelectorGroup, 0, len(selectors))
	if tag := in.GetTag(); tag != "" {
		selector, found := selectors[tag]
		if !found {
			return &gen.ErrorResp{Error: To("no auto selector with tag " + tag)}, nil
		}
		targets = append(targets, selector)
	} else {
		for _, selector := range selectors {
			targets = append(targets, selector)
		}
	}

	switch in.GetAction() {
	case "recheck":
		for _, selector := range targets {
			selector.CheckOutbounds()
		}
	case "select":
		// An empty member releases the pin and hands the group back to automatic selection.
		member := in.GetMember()
		for _, selector := range targets {
			if !selector.SelectOutbound(member) {
				return &gen.ErrorResp{Error: To("no member " + member + " in group " + selector.Tag())}, nil
			}
		}
	default:
		return nil, E.New("unknown auto selector action: ", in.GetAction())
	}
	return &gen.ErrorResp{Error: To("")}, nil
}
