package rpc

import (
	"context"
	"fmt"
	"time"

	"ThroneCore/gen"
	"ThroneCore/internal/customtest"
	"ThroneCore/internal/probe"
)

func (s *server) CustomTest(ctx context.Context, in *gen.CustomTestRequest) (*gen.CustomTestResponse, error) {
	if err := customtest.Check(in.GetScript()); err != nil {
		return nil, fmt.Errorf("custom test: %w", err)
	}

	const current = false
	env, err := prepareTestEnv(current, in.GetNeedXray(), in.GetXrayConfig(),
		in.XrayFullConfigs, in.GetConfig(), in.OutboundTags, in.GetUseDefaultOutbound(),
		in.GetXrayOutboundDnsStrategy())
	if err != nil {
		return nil, err
	}
	defer env.Close()

	timeout := time.Duration(in.GetTestTimeoutMs()) * time.Millisecond
	results, err := probe.BatchCustomTest(probe.TestContext(), env.Box, env.Tags,
		int(in.GetMaxConcurrency()), !current, in.GetScript(), timeout)
	if err != nil {
		return nil, fmt.Errorf("custom test: %w", err)
	}

	out := &gen.CustomTestResponse{Results: make([]*gen.CustomTestResult, 0, len(results))}
	for idx, r := range results {
		out.Results = append(out.Results, customResultToGen(env.Tags[idx], r))
	}
	return out, nil
}

func (s *server) QueryCustomTest(ctx context.Context, in *gen.EmptyReq) (*gen.QueryCustomTestResponse, error) {
	out := &gen.QueryCustomTestResponse{}
	for _, r := range probe.CustomReporter.Results() {
		out.Results = append(out.Results, customResultToGen(r.Tag, r))
	}
	return out, nil
}

func (s *server) CheckCustomTest(ctx context.Context, in *gen.CheckCustomTestRequest) (*gen.ErrorResp, error) {
	if err := customtest.Check(in.GetScript()); err != nil {
		return &gen.ErrorResp{Error: To(err.Error())}, nil
	}
	return &gen.ErrorResp{}, nil
}

func customResultToGen(tag string, r *probe.CustomTestResult) *gen.CustomTestResult {
	out := &gen.CustomTestResult{
		OutboundTag: To(tag),
	}
	if r.Error != nil {
		out.Error = To(r.Error.Error())
		return out
	}
	out.Text = To(r.Result.Text)
	out.Color = To(r.Result.Color)
	out.Tooltip = To(r.Result.Tooltip)
	out.SortKind = To(int32(r.Result.SortKind))
	out.SortNumber = To(r.Result.SortNumber)
	out.SortString = To(r.Result.SortString)
	return out
}
