package probe

import (
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/netip"
	"time"

	"github.com/sagernet/sing-box/adapter"
)

type IPInfo struct {
	IP          string `json:"ip"`
	CountryCode string `json:"country_code"`
	FraudScore  int32  `json:"-"`
}

var IPReporter resultBuffer[IPTestResult]

const IPTestTimeout = 3 * time.Second
const ipInfoAPI = "https://api.ip2location.io/"
const ipRiskAPI = "https://my.ippure.com/v1/info"

type IPTestResult struct {
	Result IPInfo
	Tag    string
	Error  error
}

func BatchIPTest(ctx context.Context, i Box, outboundTags []string, maxConcurrency int, cold bool, timeout time.Duration, riskTest bool) []*IPTestResult {
	if timeout <= 0 {
		timeout = IPTestTimeout
	}

	results := runBatch(ctx, i, outboundTags, maxConcurrency, batchProbe[IPTestResult]{
		run: func(ctx context.Context, tag string, outbound adapter.Outbound) *IPTestResult {
			if err := awaitTunnels(ctx, i, tag); err != nil {
				return &IPTestResult{Result: IPInfo{FraudScore: -1}, Tag: tag, Error: err}
			}
			client, closeClient := outboundHTTPClient(ctx, i, tag, outbound)
			defer closeClient()
			test := ipTest
			if riskTest {
				test = ipRiskTest
			}
			info, err := test(ctx, client, firstRequestTimeout(i, tag, cold, timeout))
			return &IPTestResult{Result: info, Tag: tag, Error: err}
		},
		fail: func(tag string, err error) *IPTestResult {
			return &IPTestResult{Result: IPInfo{FraudScore: -1}, Tag: tag, Error: err}
		},
		publish: IPReporter.AddResult,
	})
	IPReporter.Reclaim(results)
	return results
}

func ipTest(ctx context.Context, client *http.Client, timeout time.Duration) (IPInfo, error) {
	res := IPInfo{FraudScore: -1}
	ctx, cancel := context.WithTimeout(ctx, timeout)
	defer cancel()
	req, err := http.NewRequestWithContext(ctx, "GET", ipInfoAPI, nil)
	if err != nil {
		return res, err
	}
	resp, err := client.Do(req)
	if err != nil {
		return res, err
	}
	defer resp.Body.Close()
	data, err := io.ReadAll(resp.Body)
	if err != nil {
		return res, err
	}
	err = json.Unmarshal(data, &res)
	if err != nil {
		return res, err
	}
	return res, nil
}

func ipRiskTest(ctx context.Context, client *http.Client, timeout time.Duration) (IPInfo, error) {
	res := IPInfo{FraudScore: -1}
	ctx, cancel := context.WithTimeout(ctx, timeout)
	defer cancel()
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, ipRiskAPI, nil)
	if err != nil {
		return res, err
	}
	resp, err := client.Do(req)
	if err != nil {
		return res, err
	}
	defer resp.Body.Close()
	if resp.StatusCode < http.StatusOK || resp.StatusCode >= http.StatusMultipleChoices {
		return res, fmt.Errorf("IP risk lookup: HTTP %d", resp.StatusCode)
	}
	data, err := io.ReadAll(resp.Body)
	if err != nil {
		return res, err
	}
	return parseIPRiskInfo(data)
}

func parseIPRiskInfo(data []byte) (IPInfo, error) {
	res := IPInfo{FraudScore: -1}
	var payload struct {
		IP          string `json:"ip"`
		CountryCode string `json:"countryCode"`
		FraudScore  *int32 `json:"fraudScore"`
	}
	if err := json.Unmarshal(data, &payload); err != nil {
		return res, fmt.Errorf("IP risk lookup: %w", err)
	}
	ip, err := netip.ParseAddr(payload.IP)
	if err != nil || ip.Zone() != "" {
		return res, fmt.Errorf("IP risk lookup: missing or invalid IP")
	}
	if payload.FraudScore == nil || *payload.FraudScore < 0 {
		return res, fmt.Errorf("IP risk lookup: missing or invalid fraudScore")
	}
	return IPInfo{IP: payload.IP, CountryCode: payload.CountryCode, FraudScore: *payload.FraudScore}, nil
}
