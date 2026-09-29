package mobile

import (
	"context"
	"errors"
	"io"
	stdlog "log"
	"sync"
	"time"

	"ThroneCore/internal/probe"

	box "github.com/sagernet/sing-box"
	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/common/trafficcontrol"
	"github.com/sagernet/sing-box/common/urltest"
	"github.com/sagernet/sing-box/experimental/clashmode"
	"github.com/sagernet/sing-box/log"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/service"
	"github.com/sagernet/sing/service/pause"
)

const boxCloseTimeout = 2 * time.Second

type StartOptions struct {
	CoreConfig              string
	NeedXray                bool
	XrayConfig              string
	XrayOutboundDNSStrategy string
	XrayLazyStart           bool
	XrayIdleSeconds         int32
	XrayFullIdleSeconds     int32
	// Unexported so gomobile skips the slice; filled through AddXrayFullConfig.
	xrayFullConfigs []string
}

func (o *StartOptions) AddXrayFullConfig(config string) {
	o.xrayFullConfigs = append(o.xrayFullConfigs, config)
}

var _ probe.Box = (*boxHandle)(nil)

// The ctx handed to box.New is the box's service context (box.Context registers into the caller's
// registry and New reuses it), so no Box.Context() accessor is needed.
type boxHandle struct {
	ctx context.Context
	*box.Box
}

func (h *boxHandle) Context() context.Context {
	return h.ctx
}

type Instance struct {
	platform PlatformInterface
	handle   *boxHandle
	cancel   context.CancelFunc
	xray     *xrayStack

	pauseManager   pause.Manager
	outbounds      adapter.OutboundManager
	connections    adapter.ConnectionManager
	traffic        *trafficcontrol.Manager
	clashMode      *clashmode.Manager
	cacheFile      adapter.CacheFile
	urlTestHistory *urltest.HistoryStorage
	logFactory     log.Factory

	stateAccess     sync.Mutex
	started         bool
	closed          bool
	localDNSFailure *LocalDNSFailure

	statusAccess      sync.Mutex
	statusSampled     bool
	lastUplinkTotal   int64
	lastDownlinkTotal int64

	outboundTrafficAccess sync.Mutex
	outboundTraffic       map[string]pendingTraffic
}

// Mirrors rpc.Start + boxmain.Create: Xray first, then the box, whose ctx is published to the Xray
// resolvers before Start.
func NewInstance(platform PlatformInterface, options *StartOptions) (*Instance, error) {
	if platform == nil {
		return nil, E.New("nil platform interface")
	}
	if options == nil {
		return nil, E.New("nil start options")
	}
	if sDebug {
		stdlog.Println("Start:", options.CoreConfig)
		if options.NeedXray {
			stdlog.Println("Start Xray:", options.XrayConfig)
		}
	}
	installProtector(platform)

	holder := new(boxContextHolder)
	xrayStack, err := startMainXray(options, xrayPreparer(options.XrayOutboundDNSStrategy, holder.get))
	if err != nil {
		return nil, err
	}

	ctx := newBoxContext(platform, newPlatformInterfaceWrapper(platform))
	boxOptions, err := parseConfig(ctx, options.CoreConfig)
	if err != nil {
		xrayStack.close()
		return nil, err
	}
	ctx, cancel := context.WithCancel(ctx)
	urlTestHistory := urltest.NewHistoryStorage()
	ctx = service.ContextWithPtr(ctx, urlTestHistory)
	boxInstance, err := box.New(box.Options{
		Context:           ctx,
		Options:           boxOptions,
		PlatformLogWriter: platformLogWriter{},
	})
	if err != nil {
		cancel()
		urlTestHistory.Close()
		xrayStack.close()
		return nil, E.Cause(err, "create service")
	}
	// Published before Start, not after: a remote rule-set fetched during Start may already dial
	// through an Xray outbound, whose resolver needs this box's DNS router (see xraydns).
	holder.publish(ctx)
	return &Instance{
		platform:       platform,
		handle:         &boxHandle{ctx: ctx, Box: boxInstance},
		cancel:         cancel,
		xray:           xrayStack,
		pauseManager:   service.FromContext[pause.Manager](ctx),
		outbounds:      service.FromContext[adapter.OutboundManager](ctx),
		connections:    service.FromContext[adapter.ConnectionManager](ctx),
		traffic:        service.PtrFromContext[trafficcontrol.Manager](ctx),
		clashMode:      service.PtrFromContext[clashmode.Manager](ctx),
		cacheFile:      service.FromContext[adapter.CacheFile](ctx),
		urlTestHistory: urlTestHistory,
		logFactory:     boxInstance.LogFactory(),
	}, nil
}

func (i *Instance) Start() error {
	i.stateAccess.Lock()
	if i.closed {
		i.stateAccess.Unlock()
		return E.New("instance closed")
	}
	if i.started {
		i.stateAccess.Unlock()
		return E.New("instance already started")
	}
	i.started = true
	i.stateAccess.Unlock()
	if err := i.handle.Start(); err != nil {
		var dnsErr *localDNSError
		if errors.As(err, &dnsErr) {
			i.stateAccess.Lock()
			i.localDNSFailure = &LocalDNSFailure{Servers: dnsErr.servers}
			i.stateAccess.Unlock()
		}
		_ = i.Close()
		return E.Cause(err, "start service")
	}
	return nil
}

// LocalDNSFailure describes a start that failed because the local DNS server got no answer.
type LocalDNSFailure struct {
	// Servers lists the default network's DNS servers that were asked, empty when Android's resolver was.
	Servers string
}

// LocalDNSFailure is non-nil once Start has failed on the local DNS server.
func (i *Instance) LocalDNSFailure() *LocalDNSFailure {
	i.stateAccess.Lock()
	defer i.stateAccess.Unlock()
	return i.localDNSFailure
}

func (i *Instance) Close() error {
	i.stateAccess.Lock()
	if i.closed {
		i.stateAccess.Unlock()
		return nil
	}
	i.closed = true
	i.stateAccess.Unlock()
	err := closeBoxWithTimeout(i.cancel, i.handle.Box, boxCloseTimeout, true)
	i.urlTestHistory.Close()
	i.xray.close()
	releaseProtector(i.platform)
	return err
}

func (i *Instance) running() bool {
	i.stateAccess.Lock()
	defer i.stateAccess.Unlock()
	return i.started && !i.closed
}

func (i *Instance) Pause() {
	if i.running() && i.pauseManager != nil {
		i.pauseManager.DevicePause()
	}
}

func (i *Instance) Wake() {
	if i.running() && i.pauseManager != nil {
		i.pauseManager.DeviceWake()
	}
}

func (i *Instance) NeedWIFIState() bool {
	if !i.running() {
		return false
	}
	return i.handle.Network().NeedWIFIState()
}

func (i *Instance) UpdateWIFIState() {
	if i.running() {
		i.handle.Network().UpdateWIFIState(i.handle.ctx)
	}
}

func (i *Instance) ResetNetwork() {
	if i.running() {
		i.handle.Network().ResetNetwork(i.handle.ctx)
	}
}

// boxbox.CloseWithTimeout's semantics: cancel and Close on a goroutine, warn once the timer fires,
// and only a blocking caller learns the close error.
func closeBoxWithTimeout(cancel context.CancelFunc, closer io.Closer, timeout time.Duration, block bool) error {
	start := time.Now()
	timer := time.NewTimer(timeout)
	defer timer.Stop()
	done := make(chan struct{})
	var closeErr error
	go func() {
		cancel()
		closeErr = closer.Close()
		close(done)
	}()
	select {
	case <-done:
	case <-timer.C:
		stdlog.Println("[Warning] sing-box close takes longer than expected")
		if !block {
			return nil
		}
		<-done
	}
	stdlog.Printf("[Info] sing-box closed in %d ms", time.Since(start).Milliseconds())
	return closeErr
}
