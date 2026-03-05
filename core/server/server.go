package main

import (
	"ThroneCore/gen"
	"ThroneCore/internal/boxbox"
	"ThroneCore/internal/boxmain"
	"ThroneCore/internal/process"
	"ThroneCore/internal/sys"
	"ThroneCore/internal/wg"
	"ThroneCore/internal/xray"
	"ThroneCore/test_utils"
	"context"
	"errors"
	"fmt"
	"log"
	"net/netip"
	"os"
	"runtime"
	"strings"
	"time"

	"github.com/google/shlex"
	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/experimental/clashapi"
	"github.com/sagernet/sing/common"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/service"
	"github.com/xtls/xray-core/core"
)

var boxInstance *boxbox.Box
var extraProcess *process.Process
var needUnsetDNS bool
var instanceCancel context.CancelFunc
var debug bool

// Xray core
var xrayInstance *core.Instance

type server struct {
	gen.UnimplementedLibcoreServiceServer
}

// To returns a pointer to the given value.
func To[T any](v T) *T {
	return &v
}

func (s *server) Start(ctx context.Context, in *gen.LoadConfigReq) (out *gen.ErrorResp, _ error) {
	var err error

	defer func() {
		out = &gen.ErrorResp{}
		if err != nil {
			out.Error = To(err.Error())
			boxInstance = nil
		}
	}()

	if debug {
		log.Println("Start:", *in.CoreConfig)
	}

	if boxInstance != nil {
		err = errors.New("instance already started")
		return
	}

	if *in.NeedExtraProcess {
		args, e := shlex.Split(in.GetExtraProcessArgs())
		if e != nil {
			err = E.Cause(e, "Failed to parse args")
			return
		}
		if in.ExtraProcessConf != nil {
			extraConfPath := *in.ExtraProcessConfDir + string(os.PathSeparator) + "extra.conf"
			f, e := os.OpenFile(extraConfPath, os.O_CREATE|os.O_TRUNC|os.O_RDWR, 700)
			if e != nil {
				err = E.Cause(e, "Failed to open extra.conf")
				return
			}
			_, e = f.WriteString(*in.ExtraProcessConf)
			if e != nil {
				err = E.Cause(e, "Failed to write extra.conf")
				return
			}
			_ = f.Close()
			for idx, arg := range args {
				if strings.Contains(arg, "%s") {
					args[idx] = fmt.Sprintf(arg, extraConfPath)
					break
				}
			}
		}

		extraProcess = process.NewProcess(*in.ExtraProcessPath, args, *in.ExtraNoOut)
		err = extraProcess.Start()
		if err != nil {
			return
		}
	}

	if *in.NeedXray {
		xrayInstance, err = xray.CreateXrayInstance(*in.XrayConfig)
		if err != nil {
			return
		}
		err = xrayInstance.Start()
		if err != nil {
			xrayInstance = nil
			return
		}
	}

	boxInstance, instanceCancel, err = boxmain.Create([]byte(*in.CoreConfig))
	if err != nil {
		if extraProcess != nil {
			extraProcess.Stop()
			extraProcess = nil
		}
		if xrayInstance != nil {
			xrayInstance.Close()
			xrayInstance = nil
		}
		return
	}

	if runtime.GOOS == "darwin" && strings.Contains(*in.CoreConfig, "tun-in") {
		stopAllCores := func() {
			boxInstance.CloseWithTimeout(instanceCancel, time.Second*2, log.Println, true)
			boxInstance = nil
			if extraProcess != nil {
				extraProcess.Stop()
				extraProcess = nil
			}
			if xrayInstance != nil {
				xrayInstance.Close()
				xrayInstance = nil
			}
		}

		tunCIDR := in.GetTunIpv4Cidr()
		if tunCIDR == "" {
			err = errors.New("tun_ipv4_cidr is required for tun-in on macOS")
			stopAllCores()
			return
		}

		tunPrefix, parseErr := netip.ParsePrefix(tunCIDR)
		if parseErr != nil || !tunPrefix.Addr().Is4() {
			err = fmt.Errorf("invalid tun_ipv4_cidr %q", tunCIDR)
			stopAllCores()
			return
		}

		tunDNS := tunPrefix.Addr().Next()
		if !tunDNS.IsValid() || !tunDNS.Is4() {
			err = fmt.Errorf("got invalid DNS IP from tun_ipv4_cidr: %s", tunDNS)
			stopAllCores()
			return
		}

		if err := sys.SetSystemDNS(tunDNS.String(), boxInstance.Network().InterfaceMonitor()); err != nil {
			log.Println("Failed to set system DNS:", err)
		}

		needUnsetDNS = true
	}

	return
}

func (s *server) Stop(ctx context.Context, in *gen.EmptyReq) (out *gen.ErrorResp, _ error) {
	var err error

	defer func() {
		out = &gen.ErrorResp{}
		if err != nil {
			out.Error = To(err.Error())
		}
	}()

	if boxInstance == nil {
		return
	}

	if needUnsetDNS {
		needUnsetDNS = false
		err := sys.SetSystemDNS("Empty", boxInstance.Network().InterfaceMonitor())
		if err != nil {
			log.Println("Failed to unset system DNS:", err)
		}
	}
	boxInstance.CloseWithTimeout(instanceCancel, time.Second*2, log.Println, true)

	boxInstance = nil

	if extraProcess != nil {
		extraProcess.Stop()
		extraProcess = nil
	}

	if xrayInstance != nil {
		xrayInstance.Close()
		xrayInstance = nil
	}

	return
}

func (s *server) CheckConfig(ctx context.Context, in *gen.LoadConfigReq) (out *gen.ErrorResp, _ error) {
	err := boxmain.Check([]byte(*in.CoreConfig))
	out = &gen.ErrorResp{}
	if err != nil {
		out.Error = To(err.Error())
	}
	return
}

func (s *server) Test(ctx context.Context, in *gen.TestReq) (*gen.TestResp, error) {
	var testInstance *boxbox.Box
	var xrayTestIntance *core.Instance
	var cancel context.CancelFunc
	var err error
	var twice = true
	if *in.TestCurrent {
		if boxInstance == nil {
			return &gen.TestResp{Results: []*gen.URLTestResp{{
				OutboundTag: To("proxy"),
				LatencyMs:   To(int32(0)),
				Error:       To("Instance is not running"),
			}}}, nil
		}
		testInstance = boxInstance
		twice = false
	} else {
		if *in.NeedXray {
			xrayTestIntance, err = xray.CreateXrayInstance(*in.XrayConfig)
			if err != nil {
				return nil, err
			}
			err = xrayTestIntance.Start()
			if err != nil {
				return nil, err
			}
			defer func() {
				common.Must(xrayTestIntance.Close())
			}() // crash in case it does not close properly
		}
		testInstance, cancel, err = boxmain.Create([]byte(*in.Config))
		if err != nil {
			return nil, err
		}
		defer testInstance.CloseWithTimeout(cancel, 2*time.Second, log.Println, false)
	}

	outboundTags := in.OutboundTags
	if *in.UseDefaultOutbound || *in.TestCurrent {
		outbound := testInstance.Outbound().Default()
		outboundTags = []string{outbound.Tag()}
	}

	var maxConcurrency = *in.MaxConcurrency
	if maxConcurrency >= 500 || maxConcurrency == 0 {
		maxConcurrency = test_utils.MaxConcurrentTests
	}
	results := test_utils.BatchURLTest(test_utils.TestCtx, testInstance, outboundTags, *in.Url, int(maxConcurrency), twice, time.Duration(*in.TestTimeoutMs)*time.Millisecond)

	res := make([]*gen.URLTestResp, 0)
	for idx, data := range results {
		errStr := ""
		if data.Error != nil {
			errStr = data.Error.Error()
		}
		res = append(res, &gen.URLTestResp{
			OutboundTag: To(outboundTags[idx]),
			LatencyMs:   To(int32(data.Duration.Milliseconds())),
			Error:       To(errStr),
		})
	}

	return &gen.TestResp{Results: res}, nil
}

func (s *server) StopTest(ctx context.Context, in *gen.EmptyReq) (*gen.EmptyResp, error) {
	test_utils.CancelTests()
	test_utils.TestCtx, test_utils.CancelTests = context.WithCancel(context.Background())

	return &gen.EmptyResp{}, nil
}

func (s *server) QueryURLTest(ctx context.Context, in *gen.EmptyReq) (out *gen.QueryURLTestResponse, _ error) {
	results := test_utils.URLReporter.Results()
	out = &gen.QueryURLTestResponse{}
	for _, r := range results {
		errStr := ""
		if r.Error != nil {
			errStr = r.Error.Error()
		}
		out.Results = append(out.Results, &gen.URLTestResp{
			OutboundTag: To(r.Tag),
			LatencyMs:   To(int32(r.Duration.Milliseconds())),
			Error:       To(errStr),
		})
	}
	return
}

func (s *server) IPTest(ctx context.Context, in *gen.IPTestRequest) (*gen.IPTestResp, error) {
	var testInstance *boxbox.Box
	var xrayTestInstance *core.Instance
	var cancel context.CancelFunc
	var err error
	if *in.NeedXray {
		xrayTestInstance, err = xray.CreateXrayInstance(*in.XrayConfig)
		if err != nil {
			return nil, err
		}
		err = xrayTestInstance.Start()
		if err != nil {
			return nil, err
		}
		defer func() {
			common.Must(xrayTestInstance.Close())
		}()
	}
	testInstance, cancel, err = boxmain.Create([]byte(*in.Config))
	if err != nil {
		return nil, err
	}
	defer testInstance.CloseWithTimeout(cancel, 2*time.Second, log.Println, false)

	outboundTags := in.OutboundTags
	if *in.UseDefaultOutbound {
		outbound := testInstance.Outbound().Default()
		outboundTags = []string{outbound.Tag()}
	}

	maxConcurrency := *in.MaxConcurrency
	if maxConcurrency >= 500 || maxConcurrency == 0 {
		maxConcurrency = test_utils.MaxConcurrentTests
	}
	timeout := time.Duration(*in.TestTimeoutMs) * time.Millisecond
	results := test_utils.BatchIPTest(test_utils.TestCtx, testInstance, outboundTags, int(maxConcurrency), timeout)

	res := make([]*gen.IPTestRes, 0, len(results))
	for idx, data := range results {
		errStr := ""
		if data.Error != nil {
			errStr = data.Error.Error()
		}
		tag := outboundTags[idx]
		res = append(res, &gen.IPTestRes{
			OutboundTag: To(tag),
			Ip:          To(data.Result.IP),
			CountryCode: To(data.Result.CountryCode),
			Error:       To(errStr),
		})
	}
	return &gen.IPTestResp{Results: res}, nil
}

func (s *server) QueryIPTest(ctx context.Context, in *gen.EmptyReq) (out *gen.QueryIPTestResponse, _ error) {
	results := test_utils.IPReporter.Results()
	out = &gen.QueryIPTestResponse{}
	for _, r := range results {
		errStr := ""
		if r.Error != nil {
			errStr = r.Error.Error()
		}
		out.Results = append(out.Results, &gen.IPTestRes{
			OutboundTag: To(r.Tag),
			Ip:          To(r.Result.IP),
			CountryCode: To(r.Result.CountryCode),
			Error:       To(errStr),
		})
	}
	return
}

func (s *server) QueryStats(ctx context.Context, in *gen.EmptyReq) (out *gen.QueryStatsResp, err error) {
	out = &gen.QueryStatsResp{}
	out.Ups = make(map[string]int64)
	out.Downs = make(map[string]int64)
	if boxInstance != nil {
		clash := service.FromContext[adapter.ClashServer](boxInstance.Context())
		if clash != nil {
			cApi, ok := clash.(*clashapi.Server)
			if !ok {
				log.Println("Failed to assert clash server")
				err = E.New("invalid clash server type")
				return
			}
			outbounds := service.FromContext[adapter.OutboundManager](boxInstance.Context())
			if outbounds == nil {
				log.Println("Failed to get outbound manager")
				err = E.New("nil outbound manager")
				return
			}
			endpoints := service.FromContext[adapter.EndpointManager](boxInstance.Context())
			if endpoints == nil {
				log.Println("Failed to get endpoint manager")
				err = E.New("nil endpoint manager")
				return
			}
			for _, ob := range outbounds.Outbounds() {
				u, d := cApi.TrafficManager().TotalOutbound(ob.Tag())
				out.Ups[ob.Tag()] = u
				out.Downs[ob.Tag()] = d
			}
			for _, ep := range endpoints.Endpoints() {
				u, d := cApi.TrafficManager().TotalOutbound(ep.Tag())
				out.Ups[ep.Tag()] = u
				out.Downs[ep.Tag()] = d
			}
		}
	}
	return
}

func (s *server) ListConnections(ctx context.Context, in *gen.EmptyReq) (*gen.ListConnectionsResp, error) {
	if boxInstance == nil {
		return &gen.ListConnectionsResp{}, nil
	}
	if service.FromContext[adapter.ClashServer](boxInstance.Context()) == nil {
		return &gen.ListConnectionsResp{}, errors.New("no clash server found")
	}
	clash, ok := service.FromContext[adapter.ClashServer](boxInstance.Context()).(*clashapi.Server)
	if !ok {
		return &gen.ListConnectionsResp{}, errors.New("invalid state, should not be here")
	}
	connections := clash.TrafficManager().Connections()

	res := make([]*gen.ConnectionMetaData, 0)
	for _, c := range connections {
		process := ""
		if c.Metadata.ProcessInfo != nil {
			spl := strings.Split(c.Metadata.ProcessInfo.ProcessPath, string(os.PathSeparator))
			process = spl[len(spl)-1]
		}
		r := &gen.ConnectionMetaData{
			Id:        To(c.ID.String()),
			CreatedAt: To(c.CreatedAt.UnixMilli()),
			Upload:    To(c.Upload.Load()),
			Download:  To(c.Download.Load()),
			Outbound:  To(c.Outbound),
			Network:   To(c.Metadata.Network),
			Dest:      To(c.Metadata.Destination.String()),
			Protocol:  To(c.Metadata.Protocol),
			Domain:    To(c.Metadata.Domain),
			Process:   To(process),
		}
		res = append(res, r)
	}
	return &gen.ListConnectionsResp{Connections: res}, nil
}

func (s *server) IsPrivileged(ctx context.Context, _ *gen.EmptyReq) (*gen.IsPrivilegedResponse, error) {
	if runtime.GOOS == "windows" {
		return &gen.IsPrivilegedResponse{
			HasPrivilege: To(false),
		}, nil
	}

	return &gen.IsPrivilegedResponse{HasPrivilege: To(os.Geteuid() == 0)}, nil
}

func (s *server) SpeedTest(ctx context.Context, in *gen.SpeedTestRequest) (*gen.SpeedTestResponse, error) {
	if !*in.TestDownload && !*in.TestUpload && !*in.SimpleDownload && !*in.OnlyCountry {
		return nil, errors.New("cannot run empty test")
	}
	var testInstance *boxbox.Box
	var xrayTestIntance *core.Instance
	var cancel context.CancelFunc
	outboundTags := in.OutboundTags
	var err error
	if *in.TestCurrent {
		if boxInstance == nil {
			return &gen.SpeedTestResponse{Results: []*gen.SpeedTestResult{{
				OutboundTag: To("proxy"),
				Error:       To("Instance is not running"),
			}}}, nil
		}
		testInstance = boxInstance
	} else {
		if *in.NeedXray {
			xrayTestIntance, err = xray.CreateXrayInstance(*in.XrayConfig)
			if err != nil {
				return nil, err
			}
			err = xrayTestIntance.Start()
			if err != nil {
				return nil, err
			}
			defer xrayTestIntance.Close()
		}
		testInstance, cancel, err = boxmain.Create([]byte(*in.Config))
		if err != nil {
			return nil, err
		}
		defer cancel()
		defer testInstance.Close()
	}

	if *in.UseDefaultOutbound || *in.TestCurrent {
		outbound := testInstance.Outbound().Default()
		outboundTags = []string{outbound.Tag()}
	}

	results := test_utils.BatchSpeedTest(test_utils.TestCtx, testInstance, outboundTags, *in.TestDownload, *in.TestUpload, *in.SimpleDownload, *in.SimpleDownloadAddr, time.Duration(*in.TimeoutMs)*time.Millisecond, *in.OnlyCountry, *in.CountryConcurrency)

	res := make([]*gen.SpeedTestResult, 0)
	for _, data := range results {
		errStr := ""
		if data.Error != nil {
			errStr = data.Error.Error()
		}
		res = append(res, &gen.SpeedTestResult{
			DlSpeed:       To(data.DlSpeed),
			UlSpeed:       To(data.UlSpeed),
			Latency:       To(data.Latency),
			OutboundTag:   To(data.Tag),
			Error:         To(errStr),
			ServerName:    To(data.ServerName),
			ServerCountry: To(data.ServerCountry),
			Cancelled:     To(data.Cancelled),
		})
	}

	return &gen.SpeedTestResponse{Results: res}, nil
}

func (s *server) QuerySpeedTest(context.Context, *gen.EmptyReq) (*gen.QuerySpeedTestResponse, error) {
	res, isRunning := test_utils.SpTQuerier.Result()
	errStr := ""
	if res.Error != nil {
		errStr = res.Error.Error()
	}
	return &gen.QuerySpeedTestResponse{
		Result: &gen.SpeedTestResult{
			DlSpeed:       To(res.DlSpeed),
			UlSpeed:       To(res.UlSpeed),
			Latency:       To(res.Latency),
			OutboundTag:   To(res.Tag),
			Error:         To(errStr),
			ServerName:    To(res.ServerName),
			ServerCountry: To(res.ServerCountry),
			Cancelled:     To(res.Cancelled),
		},
		IsRunning: To(isRunning),
	}, nil
}

func (s *server) QueryCountryTest(ctx context.Context, _ *gen.EmptyReq) (out *gen.QueryCountryTestResponse, _ error) {
	results := test_utils.CountryResults.Results()
	out = &gen.QueryCountryTestResponse{}
	for _, res := range results {
		var errStr string
		if res.Error != nil {
			errStr = res.Error.Error()
		}
		out.Results = append(out.Results, &gen.SpeedTestResult{
			DlSpeed:       To(res.DlSpeed),
			UlSpeed:       To(res.UlSpeed),
			Latency:       To(res.Latency),
			OutboundTag:   To(res.Tag),
			Error:         To(errStr),
			ServerName:    To(res.ServerName),
			ServerCountry: To(res.ServerCountry),
			Cancelled:     To(res.Cancelled),
		})
	}
	return
}

func (s *server) GenWgKeyPair(ctx context.Context, _ *gen.EmptyReq) (out *gen.GenWgKeyPairResponse, _ error) {
	var res gen.GenWgKeyPairResponse
	privateKey, err := wg.GeneratePrivateKey()
	if err != nil {
		res.Error = To(err.Error())
		return &res, nil
	}
	res.PrivateKey = To(privateKey.String())
	res.PublicKey = To(privateKey.PublicKey().String())
	return &res, nil
}
