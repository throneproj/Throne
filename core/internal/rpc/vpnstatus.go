package rpc

import (
	"context"
	"net/netip"
	"slices"
	"strings"
	"sync"
	"time"

	"ThroneCore/gen"
	"ThroneCore/internal/boxbox"

	"github.com/sagernet/sing-box/adapter"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/service"
)

// Both protocols spell their terminal failure the same way.
const vpnStateError = adapter.OpenVPNStateError

// sing-openvpn and sing-openconnect both export ErrAuthenticationFailed with this text.
const vpnAuthFailureText = "authentication failed"

// Openconnect has no kind of its own; these stand in for the two shapes it raises.
const (
	vpnChallengeKindForm    = "form"
	vpnChallengeKindBrowser = "browser"
)

type vpnEndpoint struct {
	updated  func() <-chan struct{}
	snapshot func() *gen.VPNEndpointStatus
}

func prefixStrings(prefixes []netip.Prefix) []string {
	out := make([]string, 0, len(prefixes))
	for _, prefix := range prefixes {
		out = append(out, prefix.String())
	}
	return out
}

func addrStrings(addrs []netip.Addr) []string {
	out := make([]string, 0, len(addrs))
	for _, addr := range addrs {
		out = append(out, addr.String())
	}
	return out
}

func unixSecondsOrZero(t time.Time) int64 {
	if t.IsZero() {
		return 0
	}
	return t.Unix()
}

// Whole-segment match: keeps "tls-crypt-v2 client key authentication failed" out.
func vpnErrorIsAuthFailure(text string) bool {
	for _, joined := range strings.Split(text, " | ") {
		for _, segment := range strings.Split(joined, ": ") {
			if strings.TrimLeft(segment, "(") == vpnAuthFailureText {
				return true
			}
		}
	}
	return false
}

func firstNonEmpty(values ...string) string {
	for _, value := range values {
		if value != "" {
			return value
		}
	}
	return ""
}

func openVPNChallengeToProto(tag string, challenge *adapter.OpenVPNChallenge) *gen.VPNChallenge {
	return &gen.VPNChallenge{
		EndpointTag: To(tag),
		Id:          To(challenge.ID),
		Kind:        To(challenge.Kind),
		Username:    To(challenge.Username),
		// Never both: SecretMessage prompts a `credentials` challenge, Message every other kind.
		Message:  To(firstNonEmpty(challenge.Message, challenge.SecretMessage)),
		Error:    To(challenge.PreviousError),
		Url:      To(challenge.URL),
		Echo:     To(challenge.Echo),
		Deadline: To(unixSecondsOrZero(challenge.Deadline)),
	}
}

func openConnectChallengeToProto(tag string, challenge *adapter.OpenConnectAuthChallenge) *gen.VPNChallenge {
	out := &gen.VPNChallenge{
		EndpointTag: To(tag),
		Id:          To(challenge.ID),
		Banner:      To(challenge.Banner),
		Message:     To(challenge.Message),
		Error:       To(challenge.Error),
	}
	switch {
	case challenge.Browser != nil:
		out.Kind = To(vpnChallengeKindBrowser)
		out.Url = To(firstNonEmpty(challenge.Browser.URL, challenge.Browser.FinalURL))
	case challenge.Form != nil:
		out.Kind = To(vpnChallengeKindForm)
		out.Fields = make([]*gen.VPNChallengeField, 0, len(challenge.Form.Fields))
		for _, field := range challenge.Form.Fields {
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
	}
	return out
}

func openVPNStatusToProto(tag string, status adapter.OpenVPNStatus) *gen.VPNEndpointStatus {
	out := &gen.VPNEndpointStatus{
		Tag:        To(tag),
		State:      To(status.State),
		Error:      To(status.Error),
		Connected:  To(status.State == adapter.OpenVPNStateConnected && status.TunnelInfo != nil),
		AuthFailed: To(status.State == vpnStateError && vpnErrorIsAuthFailure(status.Error)),
	}
	if info := status.TunnelInfo; info != nil {
		out.Ipv4 = prefixStrings(info.IPv4)
		out.Ipv6 = prefixStrings(info.IPv6)
		out.Dns = addrStrings(info.DNS)
		out.Mtu = To(int32(info.MTU))
		out.ConnectedSince = To(unixSecondsOrZero(info.ConnectedSince))
		out.Server = To(info.Server)
		out.Network = To(info.Network)
		out.Cipher = To(info.Cipher)
		out.Routes = prefixStrings(info.Routes)
		out.ExcludedRoutes = prefixStrings(info.ExcludedRoutes)
		out.SearchDomains = slices.Clone(info.SearchDomains)
	}
	if status.Challenge != nil {
		out.Challenge = openVPNChallengeToProto(tag, status.Challenge)
	}
	return out
}

func openConnectStatusToProto(tag string, status adapter.OpenConnectStatus) *gen.VPNEndpointStatus {
	out := &gen.VPNEndpointStatus{
		Tag:        To(tag),
		State:      To(status.State),
		Error:      To(status.Error),
		Connected:  To(status.State == adapter.OpenConnectStateConnected && status.TunnelInfo != nil),
		AuthFailed: To(status.State == vpnStateError && vpnErrorIsAuthFailure(status.Error)),
	}
	if info := status.TunnelInfo; info != nil {
		out.Ipv4 = prefixStrings(info.IPv4)
		out.Ipv6 = prefixStrings(info.IPv6)
		out.Dns = addrStrings(info.DNS)
		out.Mtu = To(int32(info.MTU))
		out.ConnectedSince = To(unixSecondsOrZero(info.ConnectedSince))
		out.Server = To(info.Server)
		out.Network = To(info.Transport)
		out.Routes = prefixStrings(info.Routes)
		out.ExcludedRoutes = prefixStrings(info.ExcludedRoutes)
		out.SearchDomains = slices.Clone(info.SearchDomains)
	}
	if status.AuthChallenge != nil {
		out.Challenge = openConnectChallengeToProto(tag, status.AuthChallenge)
	}
	return out
}

func lookupEndpoint(box *boxbox.Box, tag string) (adapter.Endpoint, error) {
	if box == nil {
		return nil, errInstanceNotRunning
	}
	endpoints := service.FromContext[adapter.EndpointManager](box.Context())
	if endpoints == nil {
		return nil, E.New("nil endpoint manager")
	}
	found, loaded := endpoints.Get(tag)
	if !loaded {
		return nil, E.New("endpoint not found: " + tag)
	}
	return found, nil
}

func resolveVPNEndpoint(box *boxbox.Box, tag string) (*vpnEndpoint, error) {
	found, err := lookupEndpoint(box, tag)
	if err != nil {
		return nil, err
	}
	switch typed := found.(type) {
	case adapter.OpenVPNEndpoint:
		return &vpnEndpoint{
			updated:  typed.StatusUpdated,
			snapshot: func() *gen.VPNEndpointStatus { return openVPNStatusToProto(tag, typed.OpenVPNStatus()) },
		}, nil
	case adapter.OpenConnectEndpoint:
		return &vpnEndpoint{
			updated:  typed.StatusUpdated,
			snapshot: func() *gen.VPNEndpointStatus { return openConnectStatusToProto(tag, typed.OpenConnectStatus()) },
		}, nil
	}
	return nil, E.New("endpoint is not an openvpn/openconnect client: " + tag)
}

// A challenge parks the endpoint on a human, so it is as terminal for the wait as connected/error.
func vpnStatusSettled(status *gen.VPNEndpointStatus) bool {
	return status.GetConnected() || status.GetState() == vpnStateError || status.GetChallenge() != nil
}

func awaitVPNStatus(ctx context.Context, endpoint *vpnEndpoint, timeout time.Duration) *gen.VPNEndpointStatus {
	if timeout <= 0 {
		return endpoint.snapshot()
	}
	deadline := time.NewTimer(timeout)
	defer deadline.Stop()
	for {
		// Subscribed before the snapshot, so a change in between still wakes us.
		updated := endpoint.updated()
		status := endpoint.snapshot()
		if vpnStatusSettled(status) {
			return status
		}
		select {
		case <-updated:
		case <-deadline.C:
			return status
		case <-ctx.Done():
			return status
		}
	}
}

func vpnEndpointTags(box *boxbox.Box) []string {
	if box == nil {
		return nil
	}
	endpoints := service.FromContext[adapter.EndpointManager](box.Context())
	if endpoints == nil {
		return nil
	}
	var tags []string
	for _, found := range endpoints.Endpoints() {
		switch found.(type) {
		case adapter.OpenVPNEndpoint, adapter.OpenConnectEndpoint:
			tags = append(tags, found.Tag())
		}
	}
	slices.Sort(tags)
	return tags
}

func collectVPNStatus(ctx context.Context, box *boxbox.Box, tags []string, timeout time.Duration) []*gen.VPNEndpointStatus {
	out := make([]*gen.VPNEndpointStatus, len(tags))
	var wg sync.WaitGroup
	for idx, tag := range tags {
		endpoint, err := resolveVPNEndpoint(box, tag)
		if err != nil {
			out[idx] = &gen.VPNEndpointStatus{Tag: To(tag), Error: To(err.Error()), Connected: To(false)}
			continue
		}
		wg.Add(1)
		go func() {
			defer wg.Done()
			out[idx] = awaitVPNStatus(ctx, endpoint, timeout)
		}()
	}
	wg.Wait()
	return out
}

func (s *server) QueryVPNStatus(ctx context.Context, in *gen.VPNStatusRequest) (*gen.VPNStatusResponse, error) {
	timeout := time.Duration(in.GetTimeoutMs()) * time.Millisecond
	box := currentBox()
	tags := in.EndpointTags
	if len(tags) == 0 {
		tags = vpnEndpointTags(box)
	}
	return &gen.VPNStatusResponse{
		Results: collectVPNStatus(ctx, box, tags, timeout),
	}, nil
}

func vpnChallengeResult(action string, in *gen.SubmitVPNChallengeRequest, err error) *gen.ErrorResp {
	if err == nil {
		return &gen.ErrorResp{Error: To("")}
	}
	return &gen.ErrorResp{
		Error: To(E.Cause(err, action, " challenge ", in.GetChallengeId(), " on ", in.GetEndpointTag()).Error()),
	}
}

func (s *server) SubmitVPNChallenge(ctx context.Context, in *gen.SubmitVPNChallengeRequest) (*gen.ErrorResp, error) {
	found, err := lookupEndpoint(currentBox(), in.GetEndpointTag())
	if err != nil {
		return &gen.ErrorResp{Error: To(err.Error())}, nil
	}
	switch typed := found.(type) {
	case adapter.OpenVPNEndpoint:
		err = typed.CompleteChallenge(in.GetChallengeId(), adapter.OpenVPNChallengeResponse{
			Username: in.GetUsername(),
			Password: in.GetPassword(),
			Secret:   in.GetSecret(),
		})
	case adapter.OpenConnectEndpoint:
		challenge := typed.OpenConnectStatus().AuthChallenge
		// A browser challenge only accepts a BrowserResult; a form response would answer it with nothing.
		if challenge != nil && challenge.ID == in.GetChallengeId() && challenge.Browser != nil {
			return &gen.ErrorResp{Error: To("openconnect browser (SSO) authentication is not supported")}, nil
		}
		values := in.GetFormValues()
		if values == nil {
			values = make(map[string]string)
		}
		err = typed.CompleteAuthChallenge(in.GetChallengeId(), adapter.OpenConnectAuthResponse{
			Form: &adapter.OpenConnectAuthFormResponse{Values: values},
		})
	default:
		return &gen.ErrorResp{Error: To("endpoint is not an openvpn/openconnect client: " + in.GetEndpointTag())}, nil
	}
	return vpnChallengeResult("submit", in, err), nil
}

func (s *server) CancelVPNChallenge(ctx context.Context, in *gen.SubmitVPNChallengeRequest) (*gen.ErrorResp, error) {
	found, err := lookupEndpoint(currentBox(), in.GetEndpointTag())
	if err != nil {
		return &gen.ErrorResp{Error: To(err.Error())}, nil
	}
	switch typed := found.(type) {
	case adapter.OpenVPNEndpoint:
		err = typed.CancelChallenge(in.GetChallengeId())
	case adapter.OpenConnectEndpoint:
		err = typed.CancelAuthChallenge(in.GetChallengeId())
	default:
		return &gen.ErrorResp{Error: To("endpoint is not an openvpn/openconnect client: " + in.GetEndpointTag())}, nil
	}
	return vpnChallengeResult("cancel", in, err), nil
}
