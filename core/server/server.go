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

type server int

// To returns a pointer to the given value.
func To[T any](v T) *T {
	return &v
}

func (s *server) Start(in *gen.LoadConfigReq, out *gen.ErrorResp) (_ error) {
	var err error

	defer func() {
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

func (s *server) Stop(in *gen.EmptyReq, out *gen.ErrorResp) (_ error) {
	var err error

	defer func() {
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

func (s *server) CheckConfig(in *gen.LoadConfigReq, out *gen.ErrorResp) error {
	err := boxmain.Check([]byte(*in.CoreConfig))
	if err != nil {
		out.Error = To(err.Error())
		return nil
	}
	return nil
}

func (s *server) Test(in *gen.TestReq, out *gen.TestResp) error {
	var testInstance *boxbox.Box
	var cancel context.CancelFunc
	var err error
	var twice = true
	if *in.TestCurrent {
		if boxInstance == nil {
			out.Results = []*gen.URLTestResp{{
				OutboundTag: To("proxy"),
				LatencyMs:   To(int32(0)),
				Error:       To("Instance is not running"),
			}}
			return nil
		}
		testInstance = boxInstance
		twice = false
	} else {
		testInstance, cancel, err = boxmain.Create([]byte(*in.Config))
		if err != nil {
			return err
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

	out.Results = res
	return nil
}

func (s *server) StopTest(in *gen.EmptyReq, out *gen.EmptyResp) error {
	cancelTests()
	testCtx, cancelTests = context.WithCancel(context.Background())

	return nil
}

func (s *server) QueryURLTest(in *gen.EmptyReq, out *gen.QueryURLTestResponse) error {
	results := URLReporter.Results()
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
	return nil
}

func (s *server) QueryStats(in *gen.EmptyReq, out *gen.QueryStatsResp) error {
	out.Ups = make(map[string]int64)
	out.Downs = make(map[string]int64)
	if boxInstance != nil {
		clash := service.FromContext[adapter.ClashServer](boxInstance.Context())
		if clash != nil {
			cApi, ok := clash.(*clashapi.Server)
			if !ok {
				log.Println("Failed to assert clash server")
				return E.New("invalid clash server type")
			}
			outbounds := service.FromContext[adapter.OutboundManager](boxInstance.Context())
			if outbounds == nil {
				log.Println("Failed to get outbound manager")
				return E.New("nil outbound manager")
			}
			endpoints := service.FromContext[adapter.EndpointManager](boxInstance.Context())
			if endpoints == nil {
				log.Println("Failed to get endpoint manager")
				return E.New("nil endpoint manager")
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
	return nil
}

func (s *server) ListConnections(in *gen.EmptyReq, out *gen.ListConnectionsResp) error {
	if boxInstance == nil {
		return nil
	}
	if service.FromContext[adapter.ClashServer](boxInstance.Context()) == nil {
		return errors.New("no clash server found")
	}
	clash, ok := service.FromContext[adapter.ClashServer](boxInstance.Context()).(*clashapi.Server)
	if !ok {
		return errors.New("invalid state, should not be here")
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
	out.Connections = res
	return nil
}

func (s *server) GetGeoIPList(in *gen.GeoListRequest, out *gen.GetGeoIPListResponse) error {
	resp, err := boxmain.ListGeoip(*in.Path + string(os.PathSeparator) + "geoip.db")
	if err != nil {
		return err
	}

	res := make([]string, 0)
	for _, r := range resp {
		r += "_IP"
		res = append(res, r)
	}

	out.Items = res
	return nil
}

func (s *server) GetGeoSiteList(in *gen.GeoListRequest, out *gen.GetGeoSiteListResponse) error {
	resp, err := boxmain.GeositeList(*in.Path + string(os.PathSeparator) + "geosite.db")
	if err != nil {
		return err
	}

	res := make([]string, 0)
	for _, r := range resp {
		r += "_SITE"
		res = append(res, r)
	}

	out.Items = res
	return nil
}

func (s *server) CompileGeoIPToSrs(in *gen.CompileGeoIPToSrsRequest, out *gen.EmptyResp) error {
	category := strings.TrimSuffix(*in.Item, "_IP")
	err := boxmain.CompileRuleSet(*in.Path+string(os.PathSeparator)+"geoip.db", category, boxmain.IpRuleSet, "./rule_sets/"+*in.Item+".srs")
	if err != nil {
		return err
	}

	return nil
}

func (s *server) CompileGeoSiteToSrs(in *gen.CompileGeoSiteToSrsRequest, out *gen.EmptyResp) error {
	category := strings.TrimSuffix(*in.Item, "_SITE")
	err := boxmain.CompileRuleSet(*in.Path+string(os.PathSeparator)+"geosite.db", category, boxmain.SiteRuleSet, "./rule_sets/"+*in.Item+".srs")
	if err != nil {
		return err
	}

	return nil
}

func (s *server) IsPrivileged(in *gen.EmptyReq, out *gen.IsPrivilegedResponse) error {
	if runtime.GOOS == "windows" {
		out.HasPrivilege = To(false)
		return nil
	}

	out.HasPrivilege = To(os.Geteuid() == 0)
	return nil
}

func (s *server) SpeedTest(in *gen.SpeedTestRequest, out *gen.SpeedTestResponse) error {
	if !*in.TestDownload && !*in.TestUpload && !*in.SimpleDownload {
		return errors.New("cannot run empty test")
	}
	var testInstance *boxbox.Box
	var cancel context.CancelFunc
	outboundTags := in.OutboundTags
	var err error
	if *in.TestCurrent {
		if boxInstance == nil {
			out.Results = []*gen.SpeedTestResult{{
				OutboundTag: To("proxy"),
				Error:       To("Instance is not running"),
			}}
			return nil
		}
		testInstance = boxInstance
	} else {
		testInstance, cancel, err = boxmain.Create([]byte(*in.Config))
		if err != nil {
			return err
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

	out.Results = res
	return nil
}

func (s *server) QuerySpeedTest(in *gen.EmptyReq, out *gen.QuerySpeedTestResponse) error {
	res, isRunning := SpTQuerier.Result()
	errStr := ""
	if res.Error != nil {
		errStr = res.Error.Error()
	}
	out.Result = &gen.SpeedTestResult{
			DlSpeed:       To(res.DlSpeed),
			UlSpeed:       To(res.UlSpeed),
			Latency:       To(res.Latency),
			OutboundTag:   To(res.Tag),
			Error:         To(errStr),
			ServerName:    To(res.ServerName),
			ServerCountry: To(res.ServerCountry),
			Cancelled:     To(res.Cancelled),
		}
	out.IsRunning = To(isRunning)
	return nil
}
