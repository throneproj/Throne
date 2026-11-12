package rpc

import (
	"context"
	"errors"
	"os"
	"strings"

	"ThroneCore/gen"

	"github.com/gofrs/uuid/v5"
	"github.com/sagernet/sing-box/common/trafficcontrol"
	"github.com/sagernet/sing/service"
)

func connMetaToProto(c *trafficcontrol.TrackerMetadata, ruleItem string) *gen.ConnectionMetaData {
	process := ""
	processPath := ""
	if c.Metadata.ProcessInfo != nil {
		if len(c.Metadata.ProcessInfo.ProcessPaths) > 0 {
			processPath = c.Metadata.ProcessInfo.ProcessPaths[0]
		}
		spl := strings.Split(processPath, string(os.PathSeparator))
		process = spl[len(spl)-1]
	}
	var closedAt int64
	if !c.ClosedAt.IsZero() {
		closedAt = c.ClosedAt.UnixMilli()
	}
	source := ""
	if c.Metadata.Source.IsValid() {
		source = c.Metadata.Source.String()
	}
	rule := ""
	if c.Rule != nil {
		rule = c.Rule.String()
	}
	return &gen.ConnectionMetaData{
		Id:          To(c.ID.String()),
		CreatedAt:   To(c.CreatedAt.UnixMilli()),
		Upload:      To(c.Upload.Load()),
		Download:    To(c.Download.Load()),
		Outbound:    To(c.Outbound),
		Network:     To(c.Metadata.Network),
		Dest:        To(c.Metadata.Destination.String()),
		Protocol:    To(c.Metadata.Protocol),
		Domain:      To(c.Metadata.Domain),
		Process:     To(process),
		ProcessPath: To(processPath),
		Chain:       c.Chain,
		ClosedAt:    To(closedAt),
		Source:      To(source),
		Rule:        To(rule),
		RuleItem:    To(ruleItem),
	}
}

// Non-draining: the recently-closed ring is re-reported every poll and the client dedups by id.
func (s *server) QueryConnections(ctx context.Context, in *gen.EmptyReq) (*gen.QueryConnectionsResp, error) {
	box := currentBox()
	if box == nil {
		return &gen.QueryConnectionsResp{}, nil
	}
	tm := service.PtrFromContext[trafficcontrol.Manager](box.Context())
	if tm == nil {
		return &gen.QueryConnectionsResp{}, errors.New("no traffic manager found")
	}

	connRules.begin(box)
	defer connRules.end()
	active := make([]*gen.ConnectionMetaData, 0)
	for _, c := range tm.Connections() {
		active = append(active, connMetaToProto(c, connRules.matched(c)))
	}
	closed := make([]*gen.ConnectionMetaData, 0)
	for _, c := range tm.ClosedConnections() {
		closed = append(closed, connMetaToProto(c, connRules.matched(c)))
	}
	return &gen.QueryConnectionsResp{Active: active, Closed: closed}, nil
}

// Ids that already closed are a silent no-op: the client's table is always a poll behind.
func (s *server) CloseConnections(ctx context.Context, in *gen.CloseConnectionsRequest) (*gen.CloseConnectionsResponse, error) {
	if len(in.Ids) == 0 {
		return &gen.CloseConnectionsResponse{Closed: To(int32(0))}, nil
	}
	box := currentBox()
	if box == nil {
		return &gen.CloseConnectionsResponse{Error: To("no instance is running")}, nil
	}
	tm := service.PtrFromContext[trafficcontrol.Manager](box.Context())
	if tm == nil {
		return &gen.CloseConnectionsResponse{Error: To("no traffic manager found")}, nil
	}

	var closed int32
	for _, raw := range in.Ids {
		id, err := uuid.FromString(raw)
		if err != nil {
			continue
		}
		tracker := tm.Connection(id)
		if tracker == nil {
			continue
		}
		tracker.Close() //nolint:errcheck — the tracker leaves the manager either way
		closed++
	}
	return &gen.CloseConnectionsResponse{Closed: To(closed)}, nil
}
