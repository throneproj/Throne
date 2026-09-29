package mobile

import (
	"runtime"
	"time"

	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/common/urltest"
	"github.com/sagernet/sing-box/protocol/group"
	"github.com/sagernet/sing/common"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/memory"
)

type StatusMessage struct {
	Memory           int64
	Goroutines       int32
	ConnectionsIn    int32
	ConnectionsOut   int32
	TrafficAvailable bool
	Uplink           int64
	Downlink         int64
	UplinkTotal      int64
	DownlinkTotal    int64
}

type OutboundGroup struct {
	Tag        string
	Type       string
	Selectable bool
	Selected   string
	IsExpand   bool
	itemList   []*OutboundGroupItem
}

func (g *OutboundGroup) GetItems() OutboundGroupItemIterator {
	return newIterator(g.itemList)
}

type OutboundGroupIterator interface {
	Next() *OutboundGroup
	HasNext() bool
}

type OutboundGroupItem struct {
	Tag          string
	Type         string
	URLTestTime  int64
	URLTestDelay int32
}

type OutboundGroupItemIterator interface {
	Next() *OutboundGroupItem
	HasNext() bool
}

// Uplink/Downlink are the bytes moved since the previous Status() call, so a poller reads
// per-interval rates the way libbox's status stream reports them.
func (i *Instance) Status() *StatusMessage {
	status := &StatusMessage{
		Memory:     int64(memory.Total()),
		Goroutines: int32(runtime.NumGoroutine()),
	}
	if status.Memory == 0 {
		status.Memory = int64(memory.Inuse())
	}
	if !i.running() {
		return status
	}
	if i.connections != nil {
		status.ConnectionsOut = int32(i.connections.Count())
	}
	if i.traffic != nil {
		status.TrafficAvailable = true
		status.UplinkTotal, status.DownlinkTotal = i.traffic.Total()
		status.ConnectionsIn = int32(i.traffic.ConnectionsLen())
		i.statusAccess.Lock()
		if i.statusSampled {
			status.Uplink = status.UplinkTotal - i.lastUplinkTotal
			status.Downlink = status.DownlinkTotal - i.lastDownlinkTotal
		}
		i.statusSampled = true
		i.lastUplinkTotal = status.UplinkTotal
		i.lastDownlinkTotal = status.DownlinkTotal
		i.statusAccess.Unlock()
	}
	return status
}

type pendingTraffic struct {
	uplink   int64
	downlink int64
}

// Bytes of one outbound or endpoint tag since the previous read of that direction; direction is "uplink" or
// "downlink". TotalOutbound zeroes both of the tag's counters, so the direction not asked for is kept for its own read.
func (i *Instance) QueryOutboundStats(tag string, direction string) int64 {
	if i.traffic == nil {
		return 0
	}
	i.outboundTrafficAccess.Lock()
	defer i.outboundTrafficAccess.Unlock()
	uplink, downlink := i.traffic.TotalOutbound(tag)
	pending := i.outboundTraffic[tag]
	pending.uplink += uplink
	pending.downlink += downlink
	var value int64
	switch direction {
	case "uplink", "up", "upload":
		value, pending.uplink = pending.uplink, 0
	case "downlink", "down", "download":
		value, pending.downlink = pending.downlink, 0
	}
	if i.outboundTraffic == nil {
		i.outboundTraffic = make(map[string]pendingTraffic)
	}
	i.outboundTraffic[tag] = pending
	return value
}

func (i *Instance) Groups() OutboundGroupIterator {
	var groups []*OutboundGroup
	if !i.running() {
		return newIterator(groups)
	}
	for _, outbound := range i.outbounds.Outbounds() {
		outboundGroup, isGroup := outbound.(adapter.OutboundGroup)
		if !isGroup {
			continue
		}
		g := &OutboundGroup{
			Tag:      outboundGroup.Tag(),
			Type:     outboundGroup.Type(),
			Selected: outboundGroup.Now(),
		}
		_, g.Selectable = outboundGroup.(*group.Selector)
		if i.cacheFile != nil {
			if isExpand, loaded := i.cacheFile.LoadGroupExpand(g.Tag); loaded {
				g.IsExpand = isExpand
			}
		}
		for _, itemTag := range outboundGroup.All() {
			itemOutbound, loaded := i.outbounds.Outbound(itemTag)
			if !loaded {
				continue
			}
			item := &OutboundGroupItem{
				Tag:  itemTag,
				Type: itemOutbound.Type(),
			}
			if history := i.urlTestHistory.LoadURLTestHistory(group.RealTag(i.outbounds, itemOutbound)); history != nil {
				item.URLTestTime = history.Time.Unix()
				item.URLTestDelay = int32(history.Delay)
			}
			g.itemList = append(g.itemList, item)
		}
		if len(g.itemList) == 0 {
			continue
		}
		groups = append(groups, g)
	}
	return newIterator(groups)
}

func (i *Instance) SelectOutbound(groupTag string, outboundTag string) error {
	if !i.running() {
		return errInstanceNotRunning
	}
	outbound, loaded := i.outbounds.Outbound(groupTag)
	if !loaded {
		return E.New("selector not found: ", groupTag)
	}
	selector, isSelector := outbound.(*group.Selector)
	if !isSelector {
		return E.New("outbound is not a selector: ", groupTag)
	}
	if !selector.SelectOutbound(outboundTag) {
		return E.New("outbound not found in selector: ", outboundTag)
	}
	return nil
}

// Same dispatch as the daemon's URLTest: a url-test group re-checks itself, any other group is
// tested member by member, a plain outbound is tested alone; results land in the history Groups() reads.
func (i *Instance) URLTestGroup(groupTag string) error {
	if !i.running() {
		return errInstanceNotRunning
	}
	outbound, loaded := i.outbounds.Outbound(groupTag)
	if !loaded {
		return E.New("outbound not found: ", groupTag)
	}
	ctx := i.handle.ctx
	switch typed := outbound.(type) {
	case *group.URLTest:
		go typed.CheckOutbounds()
	case adapter.OutboundGroup:
		members := common.FilterNotNil(common.Map(typed.All(), func(tag string) adapter.Outbound {
			member, _ := i.outbounds.Outbound(tag)
			return member
		}))
		go group.URLTestOutbounds(ctx, i.outbounds, i.urlTestHistory, i.logFactory.Logger(), members, "", 0, true)
	default:
		go func() {
			delay, err := urltest.URLTest(ctx, "", outbound)
			if err != nil {
				i.urlTestHistory.DeleteURLTestHistory(groupTag)
				return
			}
			i.urlTestHistory.StoreURLTestHistory(groupTag, &adapter.URLTestHistory{
				Time:  time.Now(),
				Delay: delay,
			})
		}()
	}
	return nil
}

func (i *Instance) ClashMode() string {
	if i.clashMode == nil {
		return ""
	}
	return i.clashMode.Mode()
}

func (i *Instance) ClashModeList() StringIterator {
	if i.clashMode == nil {
		return newIterator[string](nil)
	}
	return newIterator(i.clashMode.ModeList())
}

func (i *Instance) SetClashMode(mode string) error {
	if i.clashMode == nil {
		return E.New("clash mode not available")
	}
	i.clashMode.SetMode(mode)
	return nil
}
