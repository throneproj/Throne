package mobile

import (
	"context"
	"errors"
	"net"
	"net/netip"
	"slices"
	"strconv"
	"strings"
	"time"

	"ThroneCore/internal/resolve"
	"ThroneCore/internal/warp"

	E "github.com/sagernet/sing/common/exceptions"
)

const (
	warpRegisterTimeout     = 10 * time.Second
	warpRegisterHostTimeout = 10 * time.Second
	warpDialTimeout         = 10 * time.Second
)

// TLSFragmentFallbackDelay is a Go duration, empty = 500ms; the fragment fields are ignored with a Proxy.
type WarpRegisterRequest struct {
	TunnelType               string
	Proxy                    string
	TLSFragment              bool
	TLSRecordFragment        bool
	TLSFragmentFallbackDelay string

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
	hosts := warp.HostCount(request.apiHosts)
	ctx, cancel := context.WithTimeout(context.Background(), warpRegisterTimeout+time.Duration(hosts)*warpRegisterHostTimeout)
	defer cancel()
	var dial warp.DialFunc
	if request.Proxy == "" {
		dial = directWarpDial()
	}
	identity, err := warp.Register(ctx, request.TunnelType, request.Proxy, request.apiHosts, warp.TLSFragmentOptions{
		Fragment:       request.TLSFragment,
		RecordFragment: request.TLSRecordFragment,
		FallbackDelay:  request.TLSFragmentFallbackDelay,
	}, dial)
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

// While a box runs, the API calls leave outside the VPN: through the Tun they are routed like any app's traffic, into
// the very WARP being registered when it is the exit, and looked up through fakeip. nil when no box runs.
func directWarpDial() warp.DialFunc {
	instance := activeInstance.Load()
	if instance == nil || !instance.running() {
		return nil
	}
	running := instance.handle.ctx
	dialer := &net.Dialer{Timeout: warpDialTimeout, KeepAlive: 30 * time.Second, Control: protectDial}
	return func(ctx context.Context, network, address string) (net.Conn, error) {
		host, port, err := net.SplitHostPort(address)
		if err != nil {
			return nil, err
		}
		if _, err = netip.ParseAddr(host); err == nil {
			return dialer.DialContext(ctx, network, address)
		}
		addresses, err := resolve.Direct(ctx, running, host)
		if errors.Is(err, resolve.ErrNoDirectServer) {
			return dialer.DialContext(ctx, network, address)
		}
		if err != nil {
			return nil, err
		}
		// IPv4 first: an IPv6 address the network cannot reach would hold each dial for its whole timeout.
		slices.SortStableFunc(addresses, func(a, b netip.Addr) int {
			return boolOrder(!a.Unmap().Is4()) - boolOrder(!b.Unmap().Is4())
		})
		dialErr := E.New("no addresses for ", host)
		for _, address := range addresses {
			conn, err := dialer.DialContext(ctx, network, net.JoinHostPort(address.Unmap().String(), port))
			if err == nil {
				return conn, nil
			}
			dialErr = err
			if ctx.Err() != nil {
				break
			}
		}
		return nil, dialErr
	}
}

func boolOrder(value bool) int {
	if value {
		return 1
	}
	return 0
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
