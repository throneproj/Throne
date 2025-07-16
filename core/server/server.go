package main

import (
	"Core/gen"
	"Core/internal/boxbox"
	"Core/internal/boxmain"
	"Core/internal/process"
	"Core/internal/sys"
	"context"
	"errors"
	"fmt"
	"github.com/google/shlex"
	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/common/settings"
	"github.com/sagernet/sing-box/experimental/clashapi"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/metadata"
	"github.com/sagernet/sing/service"
	"log"
	"os"
	"runtime"
	"strings"
	"time"
)

var boxInstance *boxbox.Box
var extraProcess *process.Process
var needUnsetDNS bool
var systemProxyController settings.SystemProxy
var systemProxyAddr metadata.Socksaddr
var instanceCancel context.CancelFunc
var debug bool

type server struct {
	gen.UnimplementedLibcoreServiceServer
}

// To returns a pointer to the given value.
func To[T any](v T) *T {
	return &v
}

func (s *server) Exit(ctx context.Context, in *gen.EmptyReq) (out *gen.EmptyResp, _ error) {
	out = &gen.EmptyResp{}

	if needUnsetDNS {
		needUnsetDNS = false
		err := sys.SetSystemDNS("Empty", boxInstance.Network().InterfaceMonitor())
		if err != nil {
			log.Println("Failed to unset system DNS:", err)
		}
	}

	// Connection closed
	defer os.Exit(0)
	return
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
		log.Println("Start:", in.CoreConfig)
	}

	if boxInstance != nil {
		err = errors.New("instance already started")
		return
	}

	if *in.NeedExtraProcess {
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
		args, e := shlex.Split(*in.ExtraProcessArgs)
		if e != nil {
			err = E.Cause(e, "Failed to parse args")
			return
		}
		for idx, arg := range args {
			if strings.Contains(arg, "%s") {
				args[idx] = fmt.Sprintf(arg, extraConfPath)
				break
			}
		}

		extraProcess = process.NewProcess(*in.ExtraProcessPath, args, *in.ExtraNoOut)
		err = extraProcess.Start()
		if err != nil {
			return
		}
	}

	boxInstance, instanceCancel, err = boxmain.Create([]byte(*in.CoreConfig))
	if err != nil {
		return
	}
	if runtime.GOOS == "darwin" && strings.Contains(*in.CoreConfig, "utun") {
		err := sys.SetSystemDNS("172.19.0.2", boxInstance.Network().InterfaceMonitor())
		if err != nil {
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
	boxInstance.CloseWithTimeout(instanceCancel, time.Second*2, log.Println)

	boxInstance = nil

	if extraProcess != nil {
		extraProcess.Stop()
		extraProcess = nil
	}

	return
}

func (s *server) CheckConfig(ctx context.Context, in *gen.LoadConfigReq) (*gen.ErrorResp, error) {
	err := boxmain.Check([]byte(*in.CoreConfig))
	if err != nil {
		return &gen.ErrorResp{
			Error: To(err.Error()),
		}, nil
	}
	return &gen.ErrorResp{}, nil
}

func (s *server) Test(ctx context.Context, in *gen.TestReq) (*gen.TestResp, error) {
	var testInstance *boxbox.Box
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
		testInstance, cancel, err = boxmain.Create([]byte(*in.Config))
		if err != nil {
			return nil, err
		}
		defer cancel()
		defer testInstance.Close()
	}

	outboundTags := in.OutboundTags
	if *in.UseDefaultOutbound || *in.TestCurrent {
		outbound := testInstance.Outbound().Default()
		outboundTags = []string{outbound.Tag()}
	}

	var maxConcurrency = *in.MaxConcurrency
	if maxConcurrency >= 500 || maxConcurrency == 0 {
		maxConcurrency = MaxConcurrentTests
	}
	results := BatchURLTest(testCtx, testInstance, outboundTags, *in.Url, int(maxConcurrency), twice)

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
	cancelTests()
	testCtx, cancelTests = context.WithCancel(context.Background())

	return &gen.EmptyResp{}, nil
}

func (s *server) QueryURLTest(ctx context.Context, in *gen.EmptyReq) (*gen.QueryURLTestResponse, error) {
	results := URLReporter.Results()
	resp := &gen.QueryURLTestResponse{}
	for _, r := range results {
		errStr := ""
		if r.Error != nil {
			errStr = r.Error.Error()
		}
		resp.Results = append(resp.Results, &gen.URLTestResp{
			OutboundTag: To(r.Tag),
			LatencyMs:   To(int32(r.Duration.Milliseconds())),
			Error:       To(errStr),
		})
	}
	return resp, nil
}

func (s *server) QueryStats(ctx context.Context, _ *gen.EmptyReq) (*gen.QueryStatsResp, error) {
	resp := &gen.QueryStatsResp{
		Ups:   make(map[string]int64),
		Downs: make(map[string]int64),
	}
	if boxInstance != nil {
		clash := service.FromContext[adapter.ClashServer](boxInstance.Context())
		if clash != nil {
			cApi, ok := clash.(*clashapi.Server)
			if !ok {
				log.Println("Failed to assert clash server")
				return nil, E.New("invalid clash server type")
			}
			outbounds := service.FromContext[adapter.OutboundManager](boxInstance.Context())
			if outbounds == nil {
				log.Println("Failed to get outbound manager")
				return nil, E.New("nil outbound manager")
			}
			endpoints := service.FromContext[adapter.EndpointManager](boxInstance.Context())
			if endpoints == nil {
				log.Println("Failed to get endpoint manager")
				return nil, E.New("nil endpoint manager")
			}
			for _, out := range outbounds.Outbounds() {
				u, d := cApi.TrafficManager().TotalOutbound(out.Tag())
				resp.Ups[out.Tag()] = u
				resp.Downs[out.Tag()] = d
			}
			for _, ep := range endpoints.Endpoints() {
				u, d := cApi.TrafficManager().TotalOutbound(ep.Tag())
				resp.Ups[ep.Tag()] = u
				resp.Downs[ep.Tag()] = d
			}
		}
	}

	return resp, nil
}

func (s *server) ListConnections(ctx context.Context, in *gen.EmptyReq) (*gen.ListConnectionsResp, error) {
	if boxInstance == nil {
		return &gen.ListConnectionsResp{}, nil
	}
	if service.FromContext[adapter.ClashServer](boxInstance.Context()) == nil {
		return nil, errors.New("no clash server found")
	}
	clash, ok := service.FromContext[adapter.ClashServer](boxInstance.Context()).(*clashapi.Server)
	if !ok {
		return nil, errors.New("invalid state, should not be here")
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
	out := &gen.ListConnectionsResp{
		Connections: res,
	}
	return out, nil
}

func (s *server) GetGeoIPList(ctx context.Context, in *gen.GeoListRequest) (*gen.GetGeoIPListResponse, error) {
	resp, err := boxmain.ListGeoip(*in.Path + string(os.PathSeparator) + "geoip.db")
	if err != nil {
		return nil, err
	}

	res := make([]string, 0)
	for _, r := range resp {
		r += "_IP"
		res = append(res, r)
	}

	return &gen.GetGeoIPListResponse{Items: res}, nil
}

func (s *server) GetGeoSiteList(ctx context.Context, in *gen.GeoListRequest) (*gen.GetGeoSiteListResponse, error) {
	resp, err := boxmain.GeositeList(*in.Path + string(os.PathSeparator) + "geosite.db")
	if err != nil {
		return nil, err
	}

	res := make([]string, 0)
	for _, r := range resp {
		r += "_SITE"
		res = append(res, r)
	}

	return &gen.GetGeoSiteListResponse{Items: res}, nil
}

func (s *server) CompileGeoIPToSrs(ctx context.Context, in *gen.CompileGeoIPToSrsRequest) (*gen.EmptyResp, error) {
	category := strings.TrimSuffix(*in.Item, "_IP")
	err := boxmain.CompileRuleSet(*in.Path+string(os.PathSeparator)+"geoip.db", category, boxmain.IpRuleSet, "./rule_sets/"+*in.Item+".srs")
	if err != nil {
		return nil, err
	}

	return &gen.EmptyResp{}, nil
}

func (s *server) CompileGeoSiteToSrs(ctx context.Context, in *gen.CompileGeoSiteToSrsRequest) (*gen.EmptyResp, error) {
	category := strings.TrimSuffix(*in.Item, "_SITE")
	err := boxmain.CompileRuleSet(*in.Path+string(os.PathSeparator)+"geosite.db", category, boxmain.SiteRuleSet, "./rule_sets/"+*in.Item+".srs")
	if err != nil {
		return nil, err
	}

	return &gen.EmptyResp{}, nil
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
	if !*in.TestDownload && !*in.TestUpload && !*in.SimpleDownload {
		return nil, errors.New("cannot run empty test")
	}
	var testInstance *boxbox.Box
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

	results := BatchSpeedTest(testCtx, testInstance, outboundTags, *in.TestDownload, *in.TestUpload, *in.SimpleDownload, *in.SimpleDownloadAddr)

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
	res, isRunning := SpTQuerier.Result()
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
