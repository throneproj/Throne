package mobile

import (
	"context"
	"strconv"
	"strings"
	"time"

	"ThroneCore/internal/warp"

	E "github.com/sagernet/sing/common/exceptions"
)

const (
	warpRegisterTimeout     = 10 * time.Second
	warpRegisterHostTimeout = 10 * time.Second
)

type WarpRegisterRequest struct {
	TunnelType string
	Proxy      string

	apiHosts []string
}

func (r *WarpRegisterRequest) AddAPIHost(host string) {
	r.apiHosts = append(r.apiHosts, host)
}

// Reserved is the WireGuard client_id as decimal bytes joined by commas ("a,b,c"), empty for MASQUE.
type WarpIdentity struct {
	DeviceID      string
	Token         string
	License       string
	IPv4          string
	IPv6          string
	PrivateKey    string
	PeerPublicKey string
	Endpoint      string
	Reserved      string
}

type WireGuardKeyPair struct {
	PrivateKey string
	PublicKey  string
}

// Blocks for up to 10 s plus 10 s per API host, like rpc.WarpRegister.
func WarpRegister(request *WarpRegisterRequest) (*WarpIdentity, error) {
	if request == nil {
		return nil, E.New("nil request")
	}
	hosts := len(request.apiHosts)
	if hosts == 0 {
		hosts = 1
	}
	ctx, cancel := context.WithTimeout(context.Background(), warpRegisterTimeout+time.Duration(hosts)*warpRegisterHostTimeout)
	defer cancel()
	identity, err := warp.Register(ctx, request.TunnelType, request.Proxy, request.apiHosts)
	if err != nil {
		return nil, err
	}
	reserved := make([]string, 0, len(identity.Reserved))
	for _, b := range identity.Reserved {
		reserved = append(reserved, strconv.Itoa(int(b)))
	}
	return &WarpIdentity{
		DeviceID:      identity.DeviceID,
		Token:         identity.Token,
		License:       identity.License,
		IPv4:          identity.IPv4,
		IPv6:          identity.IPv6,
		PrivateKey:    identity.PrivateKey,
		PeerPublicKey: identity.PeerPublicKey,
		Endpoint:      identity.Endpoint,
		Reserved:      strings.Join(reserved, ","),
	}, nil
}

func GenerateWireGuardKeyPair() (*WireGuardKeyPair, error) {
	privateKey, err := warp.GeneratePrivateKey()
	if err != nil {
		return nil, err
	}
	return &WireGuardKeyPair{
		PrivateKey: privateKey.String(),
		PublicKey:  privateKey.PublicKey().String(),
	}, nil
}
