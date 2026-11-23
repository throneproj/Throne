// Package vpnauth reads the openvpn/openconnect endpoints of a started box and answers the sign-in challenges they
// park on, for the desktop's VPN status RPCs and the mobile instance alike. JSON keys follow libcore.proto.
package vpnauth

import (
	"context"
	"net/netip"
	"slices"
	"strings"
	"sync"
	"time"

	"github.com/sagernet/sing-box/adapter"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/service"
)

// Both protocols spell their terminal failure the same way.
const StateError = adapter.OpenVPNStateError

// sing-openvpn and sing-openconnect both export ErrAuthenticationFailed with this text.
const authFailureText = "authentication failed"

// Openconnect has no kind of its own; these stand in for the two shapes it raises.
const (
	KindForm    = "form"
	KindBrowser = "browser"
)

type Choice struct {
	Value string `json:"value,omitempty"`
	Label string `json:"label,omitempty"`
}

type Field struct {
	SubmissionKey string `json:"submission_key,omitempty"`
	Name          string `json:"name,omitempty"`
	Label         string `json:"label,omitempty"`
	// text | password | select | hidden
	Kind    string   `json:"kind,omitempty"`
	Value   string   `json:"value,omitempty"`
	Options []Choice `json:"options,omitempty"`
}

// Challenge is an endpoint parked waiting for a human; nothing moves until Submit or Cancel.
type Challenge struct {
	EndpointTag string `json:"endpoint_tag"`
	ID          string `json:"id"`
	// openvpn: credentials|secret|message|open-url   openconnect: form|browser
	Kind     string `json:"kind,omitempty"`
	Username string `json:"username,omitempty"`
	Message  string `json:"message,omitempty"`
	Banner   string `json:"banner,omitempty"`
	Error    string `json:"error,omitempty"`
	URL      string `json:"url,omitempty"`
	// false => the answer is a secret and must be masked.
	Echo bool `json:"echo,omitempty"`
	// Unix seconds, 0 = none.
	Deadline int64   `json:"deadline,omitempty"`
	Fields   []Field `json:"fields,omitempty"`
}

// Status is one openvpn/openconnect endpoint's tunnel state; the tunnel fields are present only while it has one.
type Status struct {
	Tag string `json:"tag"`
	// connecting | auth-pending | connected | error
	State     string `json:"state,omitempty"`
	Error     string `json:"error,omitempty"`
	Connected bool   `json:"connected"`
	// Error is a rejected login; neither core prompts for config-supplied credentials.
	AuthFailed     bool     `json:"auth_failed,omitempty"`
	IPv4           []string `json:"ipv4,omitempty"`
	IPv6           []string `json:"ipv6,omitempty"`
	DNS            []string `json:"dns,omitempty"`
	MTU            int32    `json:"mtu,omitempty"`
	ConnectedSince int64    `json:"connected_since,omitempty"`
	Server         string   `json:"server,omitempty"`
	// openvpn: tunnel network; openconnect: transport.
	Network        string     `json:"network,omitempty"`
	Routes         []string   `json:"routes,omitempty"`
	ExcludedRoutes []string   `json:"excluded_routes,omitempty"`
	SearchDomains  []string   `json:"search_domains,omitempty"`
	Cipher         string     `json:"cipher,omitempty"`
	Challenge      *Challenge `json:"challenge,omitempty"`
}

// Answer to a challenge: openvpn reads username/password/secret, openconnect form_values keyed by submission_key. An
// empty answer acknowledges.
type Answer struct {
	Username   string            `json:"username,omitempty"`
	Password   string            `json:"password,omitempty"`
	Secret     string            `json:"secret,omitempty"`
	FormValues map[string]string `json:"form_values,omitempty"`
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
func errorIsAuthFailure(text string) bool {
	for _, joined := range strings.Split(text, " | ") {
		for _, segment := range strings.Split(joined, ": ") {
			if strings.TrimLeft(segment, "(") == authFailureText {
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

func openVPNChallenge(tag string, challenge *adapter.OpenVPNChallenge) *Challenge {
	return &Challenge{
		EndpointTag: tag,
		ID:          challenge.ID,
		Kind:        challenge.Kind,
		Username:    challenge.Username,
		// Never both: SecretMessage prompts a `credentials` challenge, Message every other kind.
		Message:  firstNonEmpty(challenge.Message, challenge.SecretMessage),
		Error:    challenge.PreviousError,
		URL:      challenge.URL,
		Echo:     challenge.Echo,
		Deadline: unixSecondsOrZero(challenge.Deadline),
	}
}

func openConnectChallenge(tag string, challenge *adapter.OpenConnectAuthChallenge) *Challenge {
	out := &Challenge{
		EndpointTag: tag,
		ID:          challenge.ID,
		Banner:      challenge.Banner,
		Message:     challenge.Message,
		Error:       challenge.Error,
	}
	switch {
	case challenge.Browser != nil:
		out.Kind = KindBrowser
		out.URL = firstNonEmpty(challenge.Browser.URL, challenge.Browser.FinalURL)
	case challenge.Form != nil:
		out.Kind = KindForm
		out.Fields = make([]Field, 0, len(challenge.Form.Fields))
		for _, field := range challenge.Form.Fields {
			mapped := Field{
				SubmissionKey: field.SubmissionKey,
				Name:          field.Name,
				Label:         field.Label,
				Kind:          field.Kind,
				Value:         field.Value,
			}
			for _, choice := range field.Options {
				mapped.Options = append(mapped.Options, Choice{Value: choice.Value, Label: choice.Label})
			}
			out.Fields = append(out.Fields, mapped)
		}
	}
	return out
}

func openVPNStatus(tag string, status adapter.OpenVPNStatus) *Status {
	out := &Status{
		Tag:        tag,
		State:      status.State,
		Error:      status.Error,
		Connected:  status.State == adapter.OpenVPNStateConnected && status.TunnelInfo != nil,
		AuthFailed: status.State == StateError && errorIsAuthFailure(status.Error),
	}
	if info := status.TunnelInfo; info != nil {
		out.IPv4 = prefixStrings(info.IPv4)
		out.IPv6 = prefixStrings(info.IPv6)
		out.DNS = addrStrings(info.DNS)
		out.MTU = int32(info.MTU)
		out.ConnectedSince = unixSecondsOrZero(info.ConnectedSince)
		out.Server = info.Server
		out.Network = info.Network
		out.Cipher = info.Cipher
		out.Routes = prefixStrings(info.Routes)
		out.ExcludedRoutes = prefixStrings(info.ExcludedRoutes)
		out.SearchDomains = slices.Clone(info.SearchDomains)
	}
	if status.Challenge != nil {
		out.Challenge = openVPNChallenge(tag, status.Challenge)
	}
	return out
}

func openConnectStatus(tag string, status adapter.OpenConnectStatus) *Status {
	out := &Status{
		Tag:        tag,
		State:      status.State,
		Error:      status.Error,
		Connected:  status.State == adapter.OpenConnectStateConnected && status.TunnelInfo != nil,
		AuthFailed: status.State == StateError && errorIsAuthFailure(status.Error),
	}
	if info := status.TunnelInfo; info != nil {
		out.IPv4 = prefixStrings(info.IPv4)
		out.IPv6 = prefixStrings(info.IPv6)
		out.DNS = addrStrings(info.DNS)
		out.MTU = int32(info.MTU)
		out.ConnectedSince = unixSecondsOrZero(info.ConnectedSince)
		out.Server = info.Server
		out.Network = info.Transport
		out.Routes = prefixStrings(info.Routes)
		out.ExcludedRoutes = prefixStrings(info.ExcludedRoutes)
		out.SearchDomains = slices.Clone(info.SearchDomains)
	}
	if status.AuthChallenge != nil {
		out.Challenge = openConnectChallenge(tag, status.AuthChallenge)
	}
	return out
}

func notVPNEndpoint(tag string) error {
	return E.New("endpoint is not an openvpn/openconnect client: " + tag)
}

// Lookup finds endpoint tag of the box whose service context is boxCtx.
func Lookup(boxCtx context.Context, tag string) (adapter.Endpoint, error) {
	endpoints := service.FromContext[adapter.EndpointManager](boxCtx)
	if endpoints == nil {
		return nil, E.New("nil endpoint manager")
	}
	found, loaded := endpoints.Get(tag)
	if !loaded {
		return nil, E.New("endpoint not found: " + tag)
	}
	return found, nil
}

// Endpoint is an openvpn/openconnect endpoint seen through its status.
type Endpoint struct {
	updated  func() <-chan struct{}
	snapshot func() *Status
}

// Updated closes on the endpoint's next status change.
func (e *Endpoint) Updated() <-chan struct{} {
	return e.updated()
}

func (e *Endpoint) Snapshot() *Status {
	return e.snapshot()
}

func Resolve(boxCtx context.Context, tag string) (*Endpoint, error) {
	found, err := Lookup(boxCtx, tag)
	if err != nil {
		return nil, err
	}
	switch typed := found.(type) {
	case adapter.OpenVPNEndpoint:
		return &Endpoint{
			updated:  typed.StatusUpdated,
			snapshot: func() *Status { return openVPNStatus(tag, typed.OpenVPNStatus()) },
		}, nil
	case adapter.OpenConnectEndpoint:
		return &Endpoint{
			updated:  typed.StatusUpdated,
			snapshot: func() *Status { return openConnectStatus(tag, typed.OpenConnectStatus()) },
		}, nil
	}
	return nil, notVPNEndpoint(tag)
}

// A challenge parks the endpoint on a human, so it is as terminal for the wait as connected/error.
func settled(status *Status) bool {
	return status.Connected || status.State == StateError || status.Challenge != nil
}

// Await waits up to timeout for the endpoint to connect, fail or raise a challenge; 0 answers at once.
func Await(ctx context.Context, endpoint *Endpoint, timeout time.Duration) *Status {
	if timeout <= 0 {
		return endpoint.Snapshot()
	}
	deadline := time.NewTimer(timeout)
	defer deadline.Stop()
	for {
		// Subscribed before the snapshot, so a change in between still wakes us.
		updated := endpoint.Updated()
		status := endpoint.Snapshot()
		if settled(status) {
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

// Tags lists the box's openvpn/openconnect endpoints, sorted.
func Tags(boxCtx context.Context) []string {
	endpoints := service.FromContext[adapter.EndpointManager](boxCtx)
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

// Collect reads every tag at once, each waiting up to timeout as Await does; a tag that cannot be read says why in
// its Error.
func Collect(ctx context.Context, boxCtx context.Context, tags []string, timeout time.Duration) []*Status {
	out := make([]*Status, len(tags))
	var wg sync.WaitGroup
	for idx, tag := range tags {
		endpoint, err := Resolve(boxCtx, tag)
		if err != nil {
			out[idx] = &Status{Tag: tag, Error: err.Error()}
			continue
		}
		wg.Add(1)
		go func() {
			defer wg.Done()
			out[idx] = Await(ctx, endpoint, timeout)
		}()
	}
	wg.Wait()
	return out
}

func challengeError(action string, tag string, id string, err error) error {
	if err == nil {
		return nil
	}
	return E.Cause(err, action, " challenge ", id, " on ", tag)
}

// Submit answers challenge id on endpoint tag.
func Submit(boxCtx context.Context, tag string, id string, answer Answer) error {
	found, err := Lookup(boxCtx, tag)
	if err != nil {
		return err
	}
	switch typed := found.(type) {
	case adapter.OpenVPNEndpoint:
		err = typed.CompleteChallenge(id, adapter.OpenVPNChallengeResponse{
			Username: answer.Username,
			Password: answer.Password,
			Secret:   answer.Secret,
		})
	case adapter.OpenConnectEndpoint:
		challenge := typed.OpenConnectStatus().AuthChallenge
		// A browser challenge only accepts a BrowserResult; a form response would answer it with nothing.
		if challenge != nil && challenge.ID == id && challenge.Browser != nil {
			return E.New("openconnect browser (SSO) authentication is not supported")
		}
		values := answer.FormValues
		if values == nil {
			values = make(map[string]string)
		}
		err = typed.CompleteAuthChallenge(id, adapter.OpenConnectAuthResponse{
			Form: &adapter.OpenConnectAuthFormResponse{Values: values},
		})
	default:
		return notVPNEndpoint(tag)
	}
	return challengeError("submit", tag, id, err)
}

// Cancel gives up challenge id on endpoint tag.
func Cancel(boxCtx context.Context, tag string, id string) error {
	found, err := Lookup(boxCtx, tag)
	if err != nil {
		return err
	}
	switch typed := found.(type) {
	case adapter.OpenVPNEndpoint:
		err = typed.CancelChallenge(id)
	case adapter.OpenConnectEndpoint:
		err = typed.CancelAuthChallenge(id)
	default:
		return notVPNEndpoint(tag)
	}
	return challengeError("cancel", tag, id, err)
}
