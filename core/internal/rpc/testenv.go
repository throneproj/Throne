package rpc

import (
	"log"
	"time"

	"ThroneCore/internal/boxbox"
	"ThroneCore/internal/boxmain"
	"ThroneCore/internal/xray"

	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/option"
)

type testEnv struct {
	box   *boxbox.Box
	tags  []string
	close func()
}

// `current` measures the running instance instead of building one, and owns nothing.
func prepareTestEnv(current bool, needXray bool, xrayConfig string, xrayFullConfigs []string,
	coreConfig string, tags []string, useDefaultOutbound bool,
	xrayDNSStrategy string) (*testEnv, error) {

	// Owned here, not by the caller: this builds the probe box the Xray instances below resolve through.
	var boxCtx boxContextHolder
	prepareXray := xrayPreparer(xrayDNSStrategy, boxCtx.get)

	if current {
		box := currentBox()
		if box == nil {
			return nil, errInstanceNotRunning
		}
		outTags := tags
		if _, exists := box.Outbound().Outbound("proxy"); exists {
			outTags = []string{"proxy"}
		} else {
			useDefaultOutbound = true
		}
		if useDefaultOutbound {
			outTags = []string{box.Outbound().Default().Tag()}
		}
		return &testEnv{box: box, tags: outTags, close: func() {}}, nil
	}

	var cleanups []func()
	unwind := func() {
		for i := len(cleanups) - 1; i >= 0; i-- {
			cleanups[i]()
		}
	}

	if needXray {
		instance, err := xray.CreateXrayInstance(xrayConfig)
		if err != nil {
			unwind()
			return nil, err
		}
		if err = prepareXray(instance); err != nil {
			_ = instance.Close()
			unwind()
			return nil, err
		}
		if err = instance.Start(); err != nil {
			_ = instance.Close()
			unwind()
			return nil, err
		}
		cleanups = append(cleanups, func() { _ = instance.Close() })
	}

	fullXray, err := startXrayFullConfigs(xrayFullConfigs, prepareXray)
	if err != nil {
		unwind()
		return nil, err
	}
	cleanups = append(cleanups, func() { closeXrayInstances(fullXray) })

	box, cancel, err := boxmain.Create([]byte(coreConfig), boxCtx.publish, func(options *option.Options) {
		applyAutoRedirectMark(options, autoRedirectMark.Load())
	})
	if err != nil {
		unwind()
		return nil, err
	}
	cleanups = append(cleanups, func() {
		box.CloseWithTimeout(cancel, 2*time.Second, log.Println, false)
	})

	outTags := tags
	if useDefaultOutbound {
		outTags = []string{box.Outbound().Default().Tag()}
	}
	return &testEnv{box: box, tags: outTags, close: unwind}, nil
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
