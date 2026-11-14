package mobile

import (
	"context"
	"encoding/json"

	"ThroneCore/internal/routetest"

	box "github.com/sagernet/sing-box"
	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/option"
	E "github.com/sagernet/sing/common/exceptions"
)

// The desktop's TestRouteRequest; zero values take its defaults (port 443, network "tcp", protocol "auto").
// PackageName is the app the connection comes from, which package_name rules match.
type RouteTestRequest struct {
	Domain      string
	IP          string
	Port        int32
	Network     string
	Protocol    string
	PackageName string
	// Only for the package-level TestRoute: the full config to start stripped.
	Config string
}

// Both TestRoutes return the desktop's TestRouteResponse as JSON keyed like libcore.proto, empty fields left out
// except a step's index, rule, action and action_detail:
// {"error", "steps": [{"index", "rule", "action", "action_detail", "matched", "protocol", "sniffed_domain",
// "addresses": [], "error", "note"}], "is_final", "action", "outbound", "outbound_chain": [], "skipped": [],
// "destination", "domain", "addresses": [], "inbound"}.
// A test that cannot run says why in "error"; the Go error is kept for a bad call or a core panic.

// TestRoute walks the running instance's route rules.
func (i *Instance) TestRoute(request *RouteTestRequest) (string, error) {
	if request == nil {
		return "", E.New("nil request")
	}
	return runRouteTest(request, func(context.Context) (*routetest.Env, error) {
		if !i.running() {
			return nil, errInstanceNotRunning
		}
		return routetest.Running(i.handle, i.routeRules), nil
	})
}

// TestRoute starts request.Config stripped down to route matching, the way the desktop tests a config: no
// inbounds, no DNS section, every outbound and endpoint a stub, and rules that need a live endpoint skipped.
// platform may be nil off-device.
func TestRoute(platform PlatformInterface, request *RouteTestRequest) (string, error) {
	if request == nil || request.Config == "" {
		return "", E.New("nil request or empty config")
	}
	return runRouteTest(request, func(ctx context.Context) (*routetest.Env, error) {
		keepCacheFile := func() bool { return runningInstances.Load() == 0 }
		return routetest.StartStripped(ctx, keepCacheFile, func(strip func(*option.Options)) (routetest.Box, func(), error) {
			return newRouteTestBox(platform, request.Config, strip)
		})
	})
}

func runRouteTest(request *RouteTestRequest, start func(ctx context.Context) (*routetest.Env, error)) (result string, err error) {
	// The desktop's dispatcher recovers a panicking handler; a panic inside a gomobile call takes the process down.
	defer func() {
		if r := recover(); r != nil {
			result, err = "", E.New("route test panic: ", r)
		}
	}()
	ctx, cancel := context.WithTimeout(context.Background(), routetest.Timeout)
	defer cancel()
	port := request.Port
	if port == 0 {
		port = 443
	}
	query, err := routetest.Parse(routetest.Request{
		PackageName: request.PackageName,
		Domain:      request.Domain,
		IP:          request.IP,
		Port:        port,
		Network:     request.Network,
		Protocol:    request.Protocol,
	})
	if err != nil {
		return routeTestJSON(&routetest.Result{Error: err.Error()})
	}
	env, err := start(ctx)
	if err != nil {
		return routeTestJSON(&routetest.Result{Error: err.Error()})
	}
	defer env.Close()
	return routeTestJSON(env.Test(ctx, query))
}

func routeTestJSON(result *routetest.Result) (string, error) {
	data, err := json.Marshal(result)
	if err != nil {
		return "", err
	}
	return string(data), nil
}

// prepareTestEnv's box without Xray or a log writer: the stripped config has neither an Xray outbound nor a log.
// The release blocks until the box is closed, so the next stripped box finds cache.db free.
func newRouteTestBox(platform PlatformInterface, config string, strip func(*option.Options)) (routetest.Box, func(), error) {
	var platformInterface adapter.PlatformInterface
	if platform != nil {
		platformInterface = newPlatformInterfaceWrapper(platform)
	}
	ctx := newBoxContext(platform, platformInterface)
	options, err := parseConfig(ctx, config)
	if err != nil {
		return nil, nil, err
	}
	strip(&options)
	ctx, cancel := context.WithCancel(ctx)
	boxInstance, err := box.New(box.Options{
		Context: ctx,
		Options: options,
	})
	if err != nil {
		cancel()
		return nil, nil, E.Cause(err, "create service")
	}
	// A failed Start has closed the box already.
	if err = boxInstance.Start(); err != nil {
		cancel()
		return nil, nil, E.Cause(err, "start service")
	}
	release := func() { _ = closeBoxWithTimeout(cancel, boxInstance, boxCloseTimeout, true) }
	return &boxHandle{ctx: ctx, Box: boxInstance}, release, nil
}
