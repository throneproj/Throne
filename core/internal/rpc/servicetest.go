package rpc

import (
	"context"
	"errors"
	"fmt"
	"time"

	"ThroneCore/gen"
	"ThroneCore/internal/probe"
	"ThroneCore/internal/servicecheck"
)

func (s *server) ServiceTest(ctx context.Context, in *gen.ServiceTestRequest) (*gen.ServiceTestResponse, error) {
	defs, err := servicecheck.ParseOrDefault(in.GetChecks())
	if err != nil {
		return nil, fmt.Errorf("service checks: %w", err)
	}
	options := servicecheck.Options{SkipIPInfo: in.GetSkipIpInfo()}
	if len(in.Only) > 0 {
		options.Only = make(map[string]bool, len(in.Only))
		known := false
		for _, id := range in.Only {
			options.Only[id] = true
		}
		for _, check := range defs.Checks {
			known = known || options.Only[check.ID]
		}
		if !known {
			return nil, errors.New("service checks: none of the requested checks exist")
		}
	}

	current := in.GetTestCurrent()
	env, err := prepareTestEnv(current, in.GetNeedXray(), in.GetXrayConfig(),
		in.XrayFullConfigs, in.GetConfig(), in.OutboundTags, in.GetUseDefaultOutbound(),
		in.GetXrayOutboundDnsStrategy())
	if err != nil {
		return nil, err
	}
	defer env.close()

	testCtx := probe.TestContext()
	if current {
		testCtx = probe.LiveInstance(testCtx)
	}
	timeout := time.Duration(in.GetTestTimeoutMs()) * time.Millisecond
	results := probe.BatchServiceTest(testCtx, env.box, env.tags,
		int(in.GetMaxConcurrency()), !current, defs, timeout, options)

	out := &gen.ServiceTestResponse{Results: make([]*gen.ServiceTestResult, 0, len(results))}
	for idx, result := range results {
		out.Results = append(out.Results, serviceResultToGen(env.tags[idx], result))
	}
	return out, nil
}

func (s *server) QueryServiceTest(ctx context.Context, in *gen.EmptyReq) (*gen.QueryServiceTestResponse, error) {
	out := &gen.QueryServiceTestResponse{}
	for _, result := range probe.ServiceReporter.Results() {
		out.Results = append(out.Results, serviceResultToGen(result.Tag, result))
	}
	return out, nil
}

func (s *server) ParseServiceChecks(ctx context.Context, in *gen.ParseServiceChecksRequest) (*gen.ParseServiceChecksResponse, error) {
	defs, err := servicecheck.ParseOrDefault(in.GetChecks())
	if err != nil {
		return &gen.ParseServiceChecksResponse{Error: To(err.Error())}, nil
	}
	out := &gen.ParseServiceChecksResponse{
		Version:  To(int32(defs.Version)),
		Revision: To(defs.Revision),
	}
	for _, category := range defs.Categories {
		out.Categories = append(out.Categories, &gen.ServiceCategory{
			Id:   To(category.ID),
			Name: To(category.Name),
			Icon: To(category.Icon),
		})
	}
	for _, check := range defs.Checks {
		out.Checks = append(out.Checks, &gen.ServiceCheckInfo{
			Id:       To(check.ID),
			Name:     To(check.Name),
			Category: To(check.Category),
		})
	}
	return out, nil
}

func serviceResultToGen(tag string, result *probe.ServiceTestResult) *gen.ServiceTestResult {
	out := &gen.ServiceTestResult{OutboundTag: To(tag)}
	if result.Error != nil {
		out.Error = To(result.Error.Error())
		return out
	}
	for _, check := range result.Result.Checks {
		out.Checks = append(out.Checks, &gen.ServiceCheckOutcome{
			Id:       To(check.ID),
			Name:     To(check.Name),
			Category: To(check.Category),
			Status:   To(check.Status),
			Region:   To(check.Region),
			Note:     To(check.Note),
		})
	}
	for _, info := range result.Result.IPInfo {
		out.IpInfo = append(out.IpInfo, &gen.ServiceIPInfo{
			Provider:    To(info.Provider),
			Ip:          To(info.IP),
			CountryCode: To(info.CountryCode),
			Asn:         To(info.ASN),
			Org:         To(info.Org),
			Type:        To(info.Type),
			RiskScore:   To(int32(info.RiskScore)),
			Flags:       info.Flags,
			Error:       To(info.Error),
		})
	}
	return out
}
