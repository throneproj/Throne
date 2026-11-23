package boxmain

import (
	"context"
	stdlog "log"
	"maps"
	"os"
	"os/signal"
	"path/filepath"
	"regexp"
	"slices"
	"strings"
	"syscall"
	"time"

	"ThroneCore/internal/torproc"

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
	psiphonFolder              = "psiphon"
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

// The rest could start a program (ClientTransportPlugin exec), add bridges or proxies, or move tor's files.
var allowedTorrcOptions = []string{
	"ClientOnly", "ExitNodes", "EntryNodes", "ExcludeNodes", "ExcludeExitNodes", "StrictNodes", "GeoIPExcludeUnknown",
	"NumEntryGuards", "NumDirectoryGuards", "UseEntryGuards", "CircuitBuildTimeout", "LearnCircuitBuildTimeout",
	"MaxCircuitDirtiness", "NewCircuitPeriod", "ConnectionPadding", "ReducedConnectionPadding", "CircuitPadding",
	"ReducedCircuitPadding", "ClientUseIPv4", "ClientUseIPv6", "ClientPreferIPv6ORPort", "PathsNeededToBuildCircuits",
	"AvoidDiskWrites",
}

// Fixed so that a config cannot run another program as tor or pick its folders; returns the torrc options it dropped.
func pinTor(options *option.Options) []string {
	var dropped []string
	for _, outbound := range options.Outbounds {
		torOptions, isTor := outbound.Options.(*option.TorOutboundOptions)
		if !isTor {
			continue
		}
		torOptions.ExecutablePath = torproc.ExecutablePath()
		torOptions.DataDirectory = ""
		torOptions.GeoIPFile = torproc.GeoIPPath()
		torOptions.GeoIP6File = torproc.GeoIPv6Path()
		torOptions.ExtraArgs = nil
		for _, key := range slices.Sorted(maps.Keys(torOptions.Options)) {
			value := torOptions.Options[key]
			delete(torOptions.Options, key)
			allowed := slices.IndexFunc(allowedTorrcOptions, func(name string) bool { return strings.EqualFold(name, key) })
			if allowed < 0 {
				dropped = append(dropped, "tor outbound "+outbound.Tag+": dropped torrc option "+key)
				continue
			}
			// The canonical spelling: keys reach tor's SETCONF unescaped, and EqualFold also matches non-ASCII lookalikes.
			torOptions.Options[allowedTorrcOptions[allowed]] = value
		}
	}
	return dropped
}

var (
	psiphonIDPattern     = regexp.MustCompile(`^[0-9A-Fa-f]{16}$`)
	psiphonRegionPattern = regexp.MustCompile(`^[A-Za-z]{2}$`)
)

// Absolute, like tor's paths, so it does not depend on the directory it is resolved from.
func psiphonDirectory() string {
	if abs, err := filepath.Abs(psiphonFolder); err == nil {
		return abs
	}
	return psiphonFolder
}

// Pins Psiphon's datastore folder and checks its IDs; reports whether the config has a Psiphon outbound.
func pinPsiphon(options *option.Options) (bool, error) {
	found := false
	for _, outbound := range options.Outbounds {
		psiphonOptions, isPsiphon := outbound.Options.(*option.PsiphonOutboundOptions)
		if !isPsiphon {
			continue
		}
		found = true
		psiphonOptions.DataDirectory = psiphonDirectory()
		if id := psiphonOptions.PropagationChannelID; id != "" && !psiphonIDPattern.MatchString(id) {
			return false, E.New("psiphon outbound ", outbound.Tag, ": propagation_channel_id must be 16 hex digits")
		}
		if id := psiphonOptions.SponsorID; id != "" && !psiphonIDPattern.MatchString(id) {
			return false, E.New("psiphon outbound ", outbound.Tag, ": sponsor_id must be 16 hex digits")
		}
		if region := psiphonOptions.EgressRegion; region != "" && !psiphonRegionPattern.MatchString(region) {
			return false, E.New("psiphon outbound ", outbound.Tag, ": egress_region must be a two-letter country code")
		}
	}
	return found, nil
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
	for _, dropped := range pinTor(options) {
		stdlog.Println("[Warning]", dropped)
	}
	hasPsiphon, err := pinPsiphon(options)
	if err != nil {
		return nil, nil, err
	}
	if hasPsiphon {
		chownPsiphonDirectory()
	}
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
		if hasPsiphon {
			chownPsiphonDirectory()
		}
		return nil, nil, E.Cause(err, "start service")
	}
	return &Box{Box: instance, ctx: ctx, psiphon: hasPsiphon}, cancel, nil
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
