package rpc

import (
	"context"
	"log"
	"time"

	"ThroneCore/internal/boxmain"
	"ThroneCore/internal/probe"
	"ThroneCore/internal/testenv"

	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/option"
)

// `current` measures the running instance instead of building one, and owns nothing.
func prepareTestEnv(current bool, needXray bool, xrayConfig string, xrayFullConfigs []string,
	coreConfig string, tags []string, useDefaultOutbound bool,
	xrayDNSStrategy string) (*testenv.Env[*boxmain.Box], error) {

	if current {
		box := currentBox()
		if box == nil {
			return nil, errInstanceNotRunning
		}
		return testenv.Current(box, tags, useDefaultOutbound), nil
	}

	request := testenv.Request{
		NeedXray:           needXray,
		XrayConfig:         xrayConfig,
		XrayFullConfigs:    xrayFullConfigs,
		XrayDNSStrategy:    xrayDNSStrategy,
		Tags:               tags,
		UseDefaultOutbound: useDefaultOutbound,
	}
	return testenv.Prepare(request, currentEgress, func(publish func(context.Context)) (*boxmain.Box, func(), error) {
		var xrayTunnelHops map[string]int
		box, cancel, err := boxmain.Create([]byte(coreConfig), publish, func(options *option.Options) {
			applyAutoRedirectMark(options, autoRedirectMark.Load())
			probe.LimitWireGuardWorkers(options)
			if needXray {
				xrayTunnelHops = probe.XrayDNSTunnelHops(options, xrayConfig)
			}
		})
		if err != nil {
			return nil, nil, err
		}
		untrack := probe.TrackDNSTunnels(box, xrayTunnelHops)
		return box, func() {
			untrack()
			box.CloseWithTimeout(cancel, 2*time.Second, log.Println, false)
		}, nil
	})
}

// A probe box has no tun to mark its sockets, so the running Tun's auto_redirect nftables rules would capture them.
func applyAutoRedirectMark(options *option.Options, mark uint32) {
	if mark == 0 {
		return
	}
	for _, inbound := range options.Inbounds {
		// sing-box refuses route.default_mark next to its own tun auto_redirect.
		if inbound.Type == C.TypeTun {
			return
		}
	}
	if options.Route == nil {
		options.Route = &option.RouteOptions{}
	}
	if options.Route.DefaultMark == 0 {
		options.Route.DefaultMark = option.FwMark(mark)
	}
}
