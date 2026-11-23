package mobile

import (
	"context"
	"encoding/json"
	"time"

	"ThroneCore/internal/vpnauth"

	E "github.com/sagernet/sing/common/exceptions"
)

// The desktop's QueryVPNStatus / SubmitVPNChallenge / CancelVPNChallenge (internal/rpc/vpnstatus.go) on the running
// instance. A status list is JSON keyed like libcore.proto's VPNEndpointStatus, empty fields left out except tag and
// connected:
// [{"tag", "state", "error", "connected", "auth_failed", "ipv4": [], "ipv6": [], "dns": [], "mtu", "connected_since",
// "server", "network", "routes": [], "excluded_routes": [], "search_domains": [], "cipher", "challenge": {"endpoint_tag",
// "id", "kind", "username", "message", "banner", "error", "url", "echo", "deadline", "fields": [{"submission_key",
// "name", "label", "kind", "value", "options": [{"value", "label"}]}]}}].

// VPNStatusListener hears the openvpn/openconnect endpoints of an instance; statusJSON is what VPNStatus(0) returns.
type VPNStatusListener interface {
	OnVPNStatus(statusJSON string)
}

func (i *Instance) vpnContext() (context.Context, error) {
	if !i.running() {
		return nil, errInstanceNotRunning
	}
	return i.handle.ctx, nil
}

func vpnStatusJSON(statuses []*vpnauth.Status) (string, error) {
	if statuses == nil {
		statuses = []*vpnauth.Status{}
	}
	data, err := json.Marshal(statuses)
	if err != nil {
		return "", err
	}
	return string(data), nil
}

// VPNStatus lists every openvpn/openconnect endpoint; timeoutMs > 0 waits that long for one still handshaking.
func (i *Instance) VPNStatus(timeoutMs int32) (string, error) {
	boxCtx, err := i.vpnContext()
	if err != nil {
		return "", err
	}
	timeout := time.Duration(timeoutMs) * time.Millisecond
	return vpnStatusJSON(vpnauth.Collect(boxCtx, boxCtx, vpnauth.Tags(boxCtx), timeout))
}

// SubmitVPNChallenge answers challenge id of endpoint tag with answerJSON, {"username", "password", "secret",
// "form_values": {submission_key: value}}: openvpn reads the first three, openconnect the form values; "{}" acknowledges.
func (i *Instance) SubmitVPNChallenge(tag string, id string, answerJSON string) error {
	boxCtx, err := i.vpnContext()
	if err != nil {
		return err
	}
	var answer vpnauth.Answer
	if answerJSON != "" {
		if err = json.Unmarshal([]byte(answerJSON), &answer); err != nil {
			return E.New("invalid challenge answer")
		}
	}
	return vpnauth.Submit(boxCtx, tag, id, answer)
}

func (i *Instance) CancelVPNChallenge(tag string, id string) error {
	boxCtx, err := i.vpnContext()
	if err != nil {
		return err
	}
	return vpnauth.Cancel(boxCtx, tag, id)
}

// WatchVPNStatus calls listener with the status list at once and after every change of an openvpn/openconnect
// endpoint, from one goroutine, until the instance closes. False, and no call, when the instance has none.
func (i *Instance) WatchVPNStatus(listener VPNStatusListener) (bool, error) {
	if listener == nil {
		return false, E.New("nil listener")
	}
	boxCtx, err := i.vpnContext()
	if err != nil {
		return false, err
	}
	var endpoints []*vpnauth.Endpoint
	for _, tag := range vpnauth.Tags(boxCtx) {
		if endpoint, resolveErr := vpnauth.Resolve(boxCtx, tag); resolveErr == nil {
			endpoints = append(endpoints, endpoint)
		}
	}
	if len(endpoints) == 0 {
		return false, nil
	}
	changed := make(chan struct{}, 1)
	for _, endpoint := range endpoints {
		go func() {
			for {
				// Subscribed before the signal, so the snapshot it triggers cannot miss a change.
				updated := endpoint.Updated()
				select {
				case changed <- struct{}{}:
				default:
				}
				select {
				case <-updated:
				case <-boxCtx.Done():
					return
				}
			}
		}()
	}
	go func() {
		for {
			select {
			case <-changed:
			case <-boxCtx.Done():
				return
			}
			statuses := make([]*vpnauth.Status, 0, len(endpoints))
			for _, endpoint := range endpoints {
				statuses = append(statuses, endpoint.Snapshot())
			}
			if statusJSON, jsonErr := vpnStatusJSON(statuses); jsonErr == nil {
				listener.OnVPNStatus(statusJSON)
			}
		}
	}()
	return true, nil
}
