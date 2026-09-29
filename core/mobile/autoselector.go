package mobile

import (
	"time"

	"github.com/sagernet/sing-box/protocol/group"
	"github.com/sagernet/sing/common"
	E "github.com/sagernet/sing/common/exceptions"
)

// The same flattening and action semantics as the desktop's QueryAutoSelectors / AutoSelectorAction RPC
// (internal/rpc/autoselector.go); times are unix milliseconds, 0 = never.

type AutoSelectorMember struct {
	Tag             string
	Rank            int32
	State           string
	Selected        bool
	SelectedUDP     bool
	Qualified       bool
	Active          bool
	AverageMs       int32
	DeviationMs     int32
	MinMs           int32
	MaxMs           int32
	Samples         int32
	Failures        int32
	Probes          int32
	DialTotal       int32
	DialFail        int32
	LastOkMs        int64
	LastProbeMs     int64
	CooldownUntilMs int64
	LastError       string
}

type AutoSelectorMemberIterator interface {
	Len() int32
	HasNext() bool
	Next() *AutoSelectorMember
}

type AutoSelectorStatus struct {
	Tag              string
	Phase            string
	Selected         string
	SelectedUDP      string
	Pinned           string
	Balance          bool
	BalanceMode      string
	Suspended        bool
	SuspendedSinceMs int64
	MembersTotal     int32
	MembersProbed    int32
	MembersAlive     int32
	MembersQualified int32
	MembersCooldown  int32
	ProbesInFlight   int32
	RoundsCompleted  int32
	LastRoundMs      int64
	NextRoundMs      int64
	LastSwitchMs     int64
	LastSwitchReason string
	members          []*AutoSelectorMember
}

// Members in rank order.
func (s *AutoSelectorStatus) Members() AutoSelectorMemberIterator {
	return newIterator(s.members)
}

type AutoSelectorStatusIterator interface {
	Len() int32
	HasNext() bool
	Next() *AutoSelectorStatus
}

// Idempotent: reading clears nothing, so any number of pollers may call it. Empty when none runs.
func (i *Instance) QueryAutoSelectors() AutoSelectorStatusIterator {
	var statuses []*AutoSelectorStatus
	for _, selector := range i.autoSelectors() {
		statuses = append(statuses, newAutoSelectorStatus(selector.Status()))
	}
	return newIterator(statuses)
}

// action "recheck" re-probes every member now; "select" pins member, and an empty member hands the group back to
// automatic selection. An empty tag applies the action to every running auto-selector.
func (i *Instance) AutoSelectorAction(tag string, action string, member string) error {
	selectors := i.autoSelectors()
	if len(selectors) == 0 {
		return E.New("no auto selector is running")
	}
	if tag != "" {
		selectors = common.Filter(selectors, func(selector group.AutoSelectorGroup) bool {
			return selector.Tag() == tag
		})
		if len(selectors) == 0 {
			return E.New("no auto selector with tag ", tag)
		}
	}
	switch action {
	case "recheck":
		for _, selector := range selectors {
			selector.CheckOutbounds()
		}
	case "select":
		for _, selector := range selectors {
			if !selector.SelectOutbound(member) {
				return E.New("no member ", member, " in group ", selector.Tag())
			}
		}
	default:
		return E.New("unknown auto selector action: ", action)
	}
	return nil
}

func (i *Instance) autoSelectors() []group.AutoSelectorGroup {
	if !i.running() {
		return nil
	}
	var selectors []group.AutoSelectorGroup
	for _, outbound := range i.outbounds.Outbounds() {
		if selector, isAuto := outbound.(group.AutoSelectorGroup); isAuto {
			selectors = append(selectors, selector)
		}
	}
	return selectors
}

func unixMilliOrZero(t time.Time) int64 {
	if t.IsZero() {
		return 0
	}
	return t.UnixMilli()
}

func newAutoSelectorStatus(status group.AutoSelectorStatus) *AutoSelectorStatus {
	out := &AutoSelectorStatus{
		Tag:              status.Tag,
		Phase:            status.Phase,
		Selected:         status.Selected,
		SelectedUDP:      status.SelectedUDP,
		Pinned:           status.Pinned,
		Balance:          status.Balance,
		BalanceMode:      status.BalanceMode,
		Suspended:        status.Suspended,
		SuspendedSinceMs: unixMilliOrZero(status.SuspendedSince),
		MembersTotal:     int32(status.MembersTotal),
		MembersProbed:    int32(status.MembersProbed),
		MembersAlive:     int32(status.MembersAlive),
		MembersQualified: int32(status.MembersQualified),
		MembersCooldown:  int32(status.MembersCooldown),
		ProbesInFlight:   int32(status.ProbesInFlight),
		RoundsCompleted:  int32(status.RoundsCompleted),
		LastRoundMs:      unixMilliOrZero(status.LastRoundAt),
		NextRoundMs:      unixMilliOrZero(status.NextRoundAt),
		LastSwitchMs:     unixMilliOrZero(status.LastSwitchAt),
		LastSwitchReason: status.LastSwitchReason,
		members:          make([]*AutoSelectorMember, 0, len(status.Members)),
	}
	for _, member := range status.Members {
		out.members = append(out.members, &AutoSelectorMember{
			Tag:             member.Tag,
			Rank:            int32(member.Rank),
			State:           member.State,
			Selected:        member.Selected,
			SelectedUDP:     member.SelectedUDP,
			Qualified:       member.Qualified,
			Active:          member.Active,
			AverageMs:       int32(member.AverageMs),
			DeviationMs:     int32(member.DeviationMs),
			MinMs:           int32(member.MinMs),
			MaxMs:           int32(member.MaxMs),
			Samples:         int32(member.Samples),
			Failures:        int32(member.Failures),
			Probes:          int32(member.Probes),
			DialTotal:       int32(member.DialTotal),
			DialFail:        int32(member.DialFail),
			LastOkMs:        unixMilliOrZero(member.LastOK),
			LastProbeMs:     unixMilliOrZero(member.LastProbe),
			CooldownUntilMs: unixMilliOrZero(member.CooldownUntil),
			LastError:       member.LastError,
		})
	}
	return out
}
