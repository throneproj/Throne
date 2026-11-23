package rpc

import (
	"context"
	"time"

	"ThroneCore/gen"
	"ThroneCore/internal/boxmain"
	"ThroneCore/internal/vpnauth"
)

func vpnChallengeToProto(challenge *vpnauth.Challenge) *gen.VPNChallenge {
	out := &gen.VPNChallenge{
		EndpointTag: To(challenge.EndpointTag),
		Id:          To(challenge.ID),
		Kind:        To(challenge.Kind),
		Username:    To(challenge.Username),
		Message:     To(challenge.Message),
		Banner:      To(challenge.Banner),
		Error:       To(challenge.Error),
		Url:         To(challenge.URL),
		Echo:        To(challenge.Echo),
		Deadline:    To(challenge.Deadline),
	}
	for _, field := range challenge.Fields {
		mapped := &gen.VPNChallengeField{
			SubmissionKey: To(field.SubmissionKey),
			Name:          To(field.Name),
			Label:         To(field.Label),
			Kind:          To(field.Kind),
			Value:         To(field.Value),
		}
		for _, choice := range field.Options {
			mapped.Options = append(mapped.Options, &gen.VPNChallengeChoice{
				Value: To(choice.Value),
				Label: To(choice.Label),
			})
		}
		out.Fields = append(out.Fields, mapped)
	}
	return out
}

func vpnStatusToProto(status *vpnauth.Status) *gen.VPNEndpointStatus {
	out := &gen.VPNEndpointStatus{
		Tag:            To(status.Tag),
		State:          To(status.State),
		Error:          To(status.Error),
		Connected:      To(status.Connected),
		AuthFailed:     To(status.AuthFailed),
		Ipv4:           status.IPv4,
		Ipv6:           status.IPv6,
		Dns:            status.DNS,
		Mtu:            To(status.MTU),
		ConnectedSince: To(status.ConnectedSince),
		Server:         To(status.Server),
		Network:        To(status.Network),
		Routes:         status.Routes,
		ExcludedRoutes: status.ExcludedRoutes,
		SearchDomains:  status.SearchDomains,
		Cipher:         To(status.Cipher),
	}
	if status.Challenge != nil {
		out.Challenge = vpnChallengeToProto(status.Challenge)
	}
	return out
}

func collectVPNStatus(ctx context.Context, box *boxmain.Box, tags []string, timeout time.Duration) []*gen.VPNEndpointStatus {
	out := make([]*gen.VPNEndpointStatus, len(tags))
	if box == nil {
		for idx, tag := range tags {
			out[idx] = &gen.VPNEndpointStatus{Tag: To(tag), Error: To(errInstanceNotRunning.Error()), Connected: To(false)}
		}
		return out
	}
	for idx, status := range vpnauth.Collect(ctx, box.Context(), tags, timeout) {
		out[idx] = vpnStatusToProto(status)
	}
	return out
}

func (s *server) QueryVPNStatus(ctx context.Context, in *gen.VPNStatusRequest) (*gen.VPNStatusResponse, error) {
	timeout := time.Duration(in.GetTimeoutMs()) * time.Millisecond
	box := currentBox()
	tags := in.EndpointTags
	if len(tags) == 0 && box != nil {
		tags = vpnauth.Tags(box.Context())
	}
	return &gen.VPNStatusResponse{
		Results: collectVPNStatus(ctx, box, tags, timeout),
	}, nil
}

func vpnChallengeResult(err error) *gen.ErrorResp {
	if err == nil {
		return &gen.ErrorResp{Error: To("")}
	}
	return &gen.ErrorResp{Error: To(err.Error())}
}

func (s *server) SubmitVPNChallenge(ctx context.Context, in *gen.SubmitVPNChallengeRequest) (*gen.ErrorResp, error) {
	box := currentBox()
	if box == nil {
		return vpnChallengeResult(errInstanceNotRunning), nil
	}
	return vpnChallengeResult(vpnauth.Submit(box.Context(), in.GetEndpointTag(), in.GetChallengeId(), vpnauth.Answer{
		Username:   in.GetUsername(),
		Password:   in.GetPassword(),
		Secret:     in.GetSecret(),
		FormValues: in.GetFormValues(),
	})), nil
}

func (s *server) CancelVPNChallenge(ctx context.Context, in *gen.SubmitVPNChallengeRequest) (*gen.ErrorResp, error) {
	box := currentBox()
	if box == nil {
		return vpnChallengeResult(errInstanceNotRunning), nil
	}
	return vpnChallengeResult(vpnauth.Cancel(box.Context(), in.GetEndpointTag(), in.GetChallengeId())), nil
}
