package rpc

import (
	"context"
	"errors"
	"fmt"
	"log"
	"net"
	"net/netip"
	"time"

	"ThroneCore/gen"
	"ThroneCore/internal/boxmain"
	"ThroneCore/internal/resolve"
	"ThroneCore/internal/torbridges"

	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
)

const (
	// Stays well under the GUI's 90 s RPC timeout.
	moatTimeout = 60 * time.Second
	// With a box running, the rest of moatTimeout is left for asking through its proxy.
	moatDirectBudget = 25 * time.Second
	moatDialTimeout  = 10 * time.Second
)

func (s *server) ParseTorBridges(ctx context.Context, in *gen.ParseTorBridgesRequest) (*gen.ParseTorBridgesResponse, error) {
	doc, err := torbridges.ParseOrEmbedded(in.GetDocument())
	if err != nil {
		return &gen.ParseTorBridgesResponse{Error: To(err.Error())}, nil
	}
	return &gen.ParseTorBridgesResponse{
		Version:  To(int32(doc.Version)),
		Revision: To(doc.Revision),
		Builtin:  bridgeSettingsToGen(doc.Builtin),
	}, nil
}

func (s *server) TorMoatSettings(ctx context.Context, in *gen.TorMoatSettingsRequest) (*gen.TorMoatSettingsResponse, error) {
	country, err := torbridges.NormalizeCountry(in.GetCountry())
	if err != nil {
		return &gen.TorMoatSettingsResponse{Error: To(err.Error())}, nil
	}
	transports, err := torbridges.NormalizeTransports(in.GetTransports())
	if err != nil {
		return &gen.TorMoatSettingsResponse{Error: To(err.Error())}, nil
	}
	doc := torbridges.Effective(in.GetBridgesDocument())
	request := torbridges.MoatRequest{Country: country, Transports: transports}

	ctx, cancel := context.WithTimeout(ctx, moatTimeout)
	defer cancel()
	box := currentBox()
	budget := moatTimeout
	if box != nil {
		budget = moatDirectBudget
	}
	answer, err := askMoat(ctx, budget, doc, request, moatDirectDial)
	viaProxy := false
	var refusal *torbridges.ServiceError
	if err != nil && !errors.As(err, &refusal) && box != nil {
		if dial := moatProxyDial(box); dial != nil {
			proxyAnswer, proxyErr := askMoat(ctx, 0, doc, request, dial)
			if proxyErr == nil || errors.As(proxyErr, &refusal) {
				answer, err, viaProxy = proxyAnswer, proxyErr, true
			} else {
				err = fmt.Errorf("directly: %w; through the proxy: %w", err, proxyErr)
			}
		}
	}
	switch {
	case errors.As(err, &refusal):
		return &gen.TorMoatSettingsResponse{Error: To(refusal.Error()), ViaProxy: To(viaProxy)}, nil
	case err != nil:
		log.Println("[Warning] Tor bridge service unreachable, answering from the built-in list:", err)
		return &gen.TorMoatSettingsResponse{
			Country:  To(country),
			Settings: bridgeSettingsToGen(doc.Offline(country, transports)),
			Offline:  To(true),
		}, nil
	}
	return &gen.TorMoatSettingsResponse{
		Country:  To(answer.Country),
		Settings: bridgeSettingsToGen(answer.Settings),
		ViaProxy: To(viaProxy),
	}, nil
}

func askMoat(ctx context.Context, budget time.Duration, doc *torbridges.Document, request torbridges.MoatRequest, dial torbridges.DialFunc) (*torbridges.MoatAnswer, error) {
	if budget > 0 {
		var cancel context.CancelFunc
		ctx, cancel = context.WithTimeout(ctx, budget)
		defer cancel()
	}
	return doc.AskMoat(ctx, request, dial)
}

// Past a running Tun like the box's own direct outbound, so the service locates the user's own network.
func moatDirectDial(ctx context.Context, network, address string) (net.Conn, error) {
	controlFunc, err := scanEgress{}.Control()
	if err != nil {
		return nil, err
	}
	dialer := net.Dialer{Timeout: moatDialTimeout, Control: controlFunc}
	host, port, err := net.SplitHostPort(address)
	if err != nil {
		return nil, err
	}
	box := currentBox()
	if _, err = netip.ParseAddr(host); err == nil || box == nil {
		return dialer.DialContext(ctx, network, address)
	}
	// The OS resolver would get the box's answer, a fake IP under fakeip, or nothing under the kill switch.
	addresses, err := resolve.Direct(ctx, box.Context(), host)
	if errors.Is(err, resolve.ErrNoDirectServer) {
		return dialer.DialContext(ctx, network, address)
	}
	if err != nil {
		return nil, err
	}
	var errs []error
	for _, addr := range addresses {
		conn, err := dialer.DialContext(ctx, network, net.JoinHostPort(addr.Unmap().String(), port))
		if err == nil {
			return conn, nil
		}
		errs = append(errs, err)
	}
	if len(errs) == 0 {
		return nil, fmt.Errorf("lookup %s: no addresses", host)
	}
	return nil, errors.Join(errs...)
}

// nil when the running box has no proxy outbound to ask through.
func moatProxyDial(box *boxmain.Box) torbridges.DialFunc {
	outbound, found := box.Outbound().Outbound("proxy")
	if !found {
		return nil
	}
	return func(ctx context.Context, network, address string) (net.Conn, error) {
		return outbound.DialContext(ctx, N.NetworkTCP, M.ParseSocksaddr(address))
	}
}

func bridgeSettingsToGen(settings []torbridges.Setting) []*gen.TorBridgeSetting {
	out := make([]*gen.TorBridgeSetting, 0, len(settings))
	for _, setting := range settings {
		out = append(out, &gen.TorBridgeSetting{
			Type:    To(setting.Type),
			Source:  To(setting.Source),
			Bridges: setting.Bridges,
		})
	}
	return out
}
