package rpc

import (
	"context"
	"fmt"
	"log"
	"runtime"
	"strings"
	"time"

	"ThroneCore/gen"
	"ThroneCore/internal/boxmain"
	"ThroneCore/internal/probe"
	"ThroneCore/internal/process"
	"ThroneCore/internal/sysdns"
	"ThroneCore/internal/xray"

	"github.com/google/shlex"
	"github.com/sagernet/sing-box/option"
	E "github.com/sagernet/sing/common/exceptions"
)

func (s *server) Start(ctx context.Context, in *gen.LoadConfigReq) (out *gen.ErrorResp, _ error) {
	lifecycleMu.Lock()
	defer lifecycleMu.Unlock()

	// Ahead of the cleanup below, which would orphan the running box and the system DNS it owns.
	if currentBox() != nil {
		return &gen.ErrorResp{Error: To("instance already started")}, nil
	}

	var err error

	defer func() {
		out = &gen.ErrorResp{}
		if err != nil {
			out.Error = To(err.Error())
			setBoxInstance(nil, nil, nil)
			autoRedirectMark.Store(0)
		}
	}()

	if debugLog {
		log.Println("Start:", *in.CoreConfig)
		if in.XrayConfig != nil {
			log.Println("Start Xray:", *in.XrayConfig)
		}
	}

	if *in.NeedExtraProcess {
		args, e := shlex.Split(in.GetExtraProcessArgs())
		if e != nil {
			err = E.Cause(e, "Failed to parse args")
			return
		}
		var extraConfPath, extraCleanupPath string
		if in.ExtraProcessConf != nil {
			extraConfPath, extraCleanupPath, e = process.CreateExtraConfig(*in.ExtraProcessConf)
			if e != nil {
				err = E.Cause(e, "Failed to create extra.conf")
				return
			}
			for idx, arg := range args {
				if strings.Contains(arg, "%s") {
					args[idx] = fmt.Sprintf(arg, extraConfPath)
					break
				}
			}
		}

		extraProcess = process.NewProcess(*in.ExtraProcessPath, args, *in.ExtraNoOut)
		extraProcess.SetCleanupPath(extraCleanupPath)
		err = extraProcess.Start()
		if err != nil {
			return
		}
	}

	autoRedirectMark.Store(autoRedirectMarkFor([]byte(in.GetCoreConfig())))

	stopSidecars := func() {
		sysdns.Release()
		if extraProcess != nil {
			extraProcess.Stop()
			extraProcess = nil
		}
		closeXray()
	}

	stack := new(xray.Stack)
	setXrayStack(stack)
	if e := stack.Start(xray.StackOptions{
		NeedXray:    *in.NeedXray,
		Config:      in.GetXrayConfig(),
		Lazy:        in.GetXrayLazyStart(),
		Idle:        time.Duration(in.GetXrayIdleSeconds()) * time.Second,
		FullConfigs: in.GetXrayFullConfigs(),
		FullIdle:    time.Duration(in.GetXrayFullIdleSeconds()) * time.Second,
		DNSStrategy: in.GetXrayOutboundDnsStrategy(),
		Egress:      currentEgress,
	}); e != nil {
		stopSidecars()
		err = e
		return
	}

	// A macOS Tun whose system resolver cannot be pointed at it would start with every lookup scoped past it.
	systemDNS := runtime.GOOS == "darwin" && in.GetTunIpv4Cidr() != ""
	if systemDNS {
		if e := sysdns.Prepare(); e != nil {
			stopSidecars()
			err = E.Cause(e, "set up the system DNS")
			return
		}
	}

	var routeRules []option.Rule
	var xrayTunnelHops map[string]int
	box, cancel, err := boxmain.Create([]byte(*in.CoreConfig), stack.PublishBoxContext, func(options *option.Options) {
		if options.Route != nil {
			routeRules = options.Route.Rules
		}
		if systemDNS {
			sysdns.AdjustInbound(options)
		}
		if in.GetNeedXray() {
			xrayTunnelHops = probe.XrayDNSTunnelHops(options, in.GetXrayConfig())
		}
	})
	if err != nil {
		stopSidecars()
		return
	}
	setBoxInstance(box, cancel, routeRules)

	if systemDNS {
		if e := sysdns.Apply(box.Network().InterfaceMonitor()); e != nil {
			sysdns.Release()
			box.CloseWithTimeout(cancel, time.Second*2, log.Println, true)
			stopSidecars()
			err = E.Cause(e, "set the system DNS")
			return
		}
	}

	untrackDNSTunnels = probe.TrackDNSTunnels(box, xrayTunnelHops)

	if in.GetTunIpv4Cidr() != "" {
		stopEgressForwardingWatch = watchEgressForwarding(box.Network())
	}

	return
}

func (s *server) Stop(ctx context.Context, in *gen.EmptyReq) (out *gen.ErrorResp, _ error) {
	lifecycleMu.Lock()
	defer lifecycleMu.Unlock()

	var err error

	defer func() {
		out = &gen.ErrorResp{}
		if err != nil {
			out.Error = To(err.Error())
		}
	}()

	// Before the box closes, so the system resolver never points at a listener that is already gone; also without a box,
	// so no lost instance can keep the system DNS.
	sysdns.Release()

	box, cancel := currentInstance()
	if box == nil {
		return
	}

	if stopEgressForwardingWatch != nil {
		stopEgressForwardingWatch()
		stopEgressForwardingWatch = nil
	}

	if untrackDNSTunnels != nil {
		untrackDNSTunnels()
		untrackDNSTunnels = nil
	}

	// Unpublished first, so a poll mid-teardown sees no instance rather than a dying one.
	setBoxInstance(nil, nil, nil)
	box.CloseWithTimeout(cancel, time.Second*2, log.Println, true)

	if extraProcess != nil {
		extraProcess.Stop()
		extraProcess = nil
	}

	closeXray()
	// The Tun and its nftables rules went down with the box, so later test instances must not carry the exemption mark.
	autoRedirectMark.Store(0)

	return
}

func (s *server) CheckConfig(ctx context.Context, in *gen.LoadConfigReq) (out *gen.ErrorResp, _ error) {
	out = &gen.ErrorResp{}
	// boxmain.Check can panic on malformed configs; unrecovered it reaches main()'s os.Exit(0) and kills the core.
	defer func() {
		if r := recover(); r != nil {
			buf := make([]byte, 4096)
			n := runtime.Stack(buf, false)
			log.Printf("CheckConfig panic: %v\n%s", r, buf[:n])
			out.Error = To(fmt.Sprintf("CheckConfig panic: %v", r))
		}
	}()
	if in.GetNeedXray() {
		if err := xray.CheckXrayConfig(in.GetXrayConfig()); err != nil {
			out.Error = To(err.Error())
		}
		return
	}
	err := boxmain.Check([]byte(*in.CoreConfig))
	if err != nil {
		out.Error = To(err.Error())
	}
	return
}
