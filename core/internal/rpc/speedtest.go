package rpc

import (
	"context"
	"errors"
	"time"

	"ThroneCore/gen"
	"ThroneCore/internal/probe"
)

func speedTestResultToProto(res probe.SpeedTestResult) *gen.SpeedTestResult {
	errStr := ""
	if res.Error != nil {
		errStr = res.Error.Error()
	}
	return &gen.SpeedTestResult{
		DlSpeed:       To(res.DlSpeed),
		UlSpeed:       To(res.UlSpeed),
		Latency:       To(res.Latency),
		OutboundTag:   To(res.Tag),
		Error:         To(errStr),
		ServerName:    To(res.ServerName),
		ServerCountry: To(res.ServerCountry),
		Cancelled:     To(res.Cancelled),
		DlBytes:       To(res.DlBytes),
		UlBytes:       To(res.UlBytes),
	}
}

func (s *server) SpeedTest(ctx context.Context, in *gen.SpeedTestRequest) (*gen.SpeedTestResponse, error) {
	if !*in.TestDownload && !*in.TestUpload && !*in.SimpleDownload && !*in.OnlyCountry {
		return nil, errors.New("cannot run empty test")
	}

	env, err := prepareTestEnv(in.GetTestCurrent(), in.GetNeedXray(), in.GetXrayConfig(),
		in.XrayFullConfigs, in.GetConfig(), in.OutboundTags, in.GetUseDefaultOutbound(),
		in.GetXrayOutboundDnsStrategy())
	if err != nil {
		if errors.Is(err, errInstanceNotRunning) {
			return &gen.SpeedTestResponse{Results: []*gen.SpeedTestResult{{
				OutboundTag: To("proxy"),
				Error:       To(err.Error()),
			}}}, nil
		}
		return nil, err
	}
	defer env.close()

	results := probe.BatchSpeedTest(probe.TestContext(), env.box, env.tags,
		*in.TestDownload, *in.TestUpload, *in.SimpleDownload, *in.SimpleDownloadAddr,
		time.Duration(*in.TimeoutMs)*time.Millisecond, *in.OnlyCountry, *in.CountryConcurrency)

	res := make([]*gen.SpeedTestResult, 0, len(results))
	for _, data := range results {
		res = append(res, speedTestResultToProto(*data))
	}

	return &gen.SpeedTestResponse{Results: res}, nil
}

func (s *server) QuerySpeedTest(context.Context, *gen.EmptyReq) (*gen.QuerySpeedTestResponse, error) {
	res, isRunning := probe.SpTQuerier.Result()
	return &gen.QuerySpeedTestResponse{
		Result:    speedTestResultToProto(res),
		IsRunning: To(isRunning),
	}, nil
}

func (s *server) QueryCountryTest(ctx context.Context, _ *gen.EmptyReq) (out *gen.QueryCountryTestResponse, _ error) {
	results := probe.CountryResults.Results()
	out = &gen.QueryCountryTestResponse{}
	for _, res := range results {
		out.Results = append(out.Results, speedTestResultToProto(*res))
	}
	return
}
