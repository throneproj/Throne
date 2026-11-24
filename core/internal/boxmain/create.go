package boxmain

import (
	"context"
	"os"
	"os/signal"
	"syscall"
	"time"

	box "github.com/sagernet/sing-box"
	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/log"
	"github.com/sagernet/sing-box/option"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/json"
)

const (
	tailscaleStateDirectory    = "tailscale"
	tailscaleTaildropDirectory = "Taildrop"
)

func parseConfig(ctx context.Context, configContent []byte) (*option.Options, error) {
	options, err := json.UnmarshalExtendedContext[option.Options](ctx, configContent)
	if err != nil {
		return nil, E.Cause(err, "decode config at ", string(configContent))
	}
	return &options, nil
}

// Fixed so that a config cannot point Tailscale's state or received files at an arbitrary folder.
func pinTailscaleDirectories(options *option.Options) {
	for _, endpoint := range options.Endpoints {
		if tailscaleOptions, isTailscale := endpoint.Options.(*option.TailscaleEndpointOptions); isTailscale {
			tailscaleOptions.StateDirectory = tailscaleStateDirectory
			tailscaleOptions.TaildropDirectory = tailscaleTaildropDirectory
		}
	}
}

// Otherwise sing-box serves pprof on debug.listen and applies the config's GC and memory limits to this whole process.
func dropDebugOptions(options *option.Options) {
	if options.Experimental != nil {
		options.Experimental.Debug = nil
	}
}

// onCreated gets the box's context between New and Start: the Xray sidecars start first and resolve through this box.
func Create(configContent []byte, onCreated func(context.Context), adjust ...func(*option.Options)) (*Box, context.CancelFunc, error) {
	// Fresh context per call: concurrent boxes sharing one service.Registry clobber each other's OutboundManager.
	ctx := newBoxContext()
	options, err := parseConfig(ctx, configContent)
	if err != nil {
		return nil, nil, err
	}
	pinTailscaleDirectories(options)
	dropDebugOptions(options)
	if disableColor {
		if options.Log == nil {
			options.Log = &option.LogOptions{}
		}
		options.Log.DisableColor = true
	}
	for _, fn := range adjust {
		fn(options)
	}
	ctx, cancel := context.WithCancel(ctx)
	instance, err := box.New(box.Options{
		Context: ctx,
		Options: *options,
	})
	if err != nil {
		cancel()
		return nil, nil, E.Cause(err, "create service")
	}
	if onCreated != nil {
		onCreated(ctx)
	}

	osSignals := make(chan os.Signal, 1)
	signal.Notify(osSignals, os.Interrupt, syscall.SIGTERM, syscall.SIGHUP)
	defer func() {
		signal.Stop(osSignals)
		close(osSignals)
	}()
	startCtx, finishStart := context.WithCancel(context.Background())
	go func() {
		_, loaded := <-osSignals
		if loaded {
			cancel()
			closeMonitor(startCtx)
		}
	}()
	err = instance.Start()
	finishStart()
	if err != nil {
		cancel()
		return nil, nil, E.Cause(err, "start service")
	}
	return &Box{Box: instance, ctx: ctx}, cancel, nil
}

func closeMonitor(ctx context.Context) {
	time.Sleep(C.FatalStopTimeout)
	select {
	case <-ctx.Done():
		return
	default:
	}
	log.Fatal("sing-box did not close!")
}
