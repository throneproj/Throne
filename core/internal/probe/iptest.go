package probe

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"net/netip"
	"strings"
	"time"

	"github.com/sagernet/sing-box/adapter"

	"ThroneCore/internal/servicecheck"
)

type IPInfo struct {
	IP          string
	CountryCode string
}

var IPReporter resultBuffer[IPTestResult]

const IPTestTimeout = 3 * time.Second

type IPTestResult struct {
	Result IPInfo
	Tag    string
	Error  error
}

// provider is an exit_lookup_provider id as the GUI stores it; "" or "auto" tries several.
func BatchIPTest(ctx context.Context, i Box, outboundTags []string, maxConcurrency int, cold bool, timeout time.Duration, provider string) []*IPTestResult {
	if timeout <= 0 {
		timeout = IPTestTimeout
	}
	chain := ipLookupChain(provider)

	results := runBatch(ctx, i, outboundTags, maxConcurrency, batchProbe[IPTestResult]{
		run: func(ctx context.Context, tag string, outbound adapter.Outbound) *IPTestResult {
			if err := awaitTunnels(ctx, i, tag); err != nil {
				return &IPTestResult{Tag: tag, Error: err}
			}
			client, closeClient := outboundHTTPClient(ctx, i, tag, outbound)
			defer closeClient()
			info, err := ipTest(ctx, client, chain, firstRequestTimeout(i, tag, cold, timeout), timeout)
			return &IPTestResult{Result: info, Tag: tag, Error: err}
		},
		fail: func(tag string, err error) *IPTestResult {
			return &IPTestResult{Tag: tag, Error: err}
		},
		publish: IPReporter.AddResult,
	})
	IPReporter.Reclaim(results)
	return results
}

// The next provider is only worth asking when this one answered: no answer at all means the exit
// itself is down, and every other provider would time out the same way.
func ipTest(ctx context.Context, client *http.Client, chain []ipLookupProvider, firstTimeout, timeout time.Duration) (IPInfo, error) {
	var lastErr error
	for idx, provider := range chain {
		attemptTimeout := timeout
		if idx == 0 {
			attemptTimeout = firstTimeout
		}
		info, answered, err := provider.lookup(ctx, client, attemptTimeout)
		if err == nil {
			return info, nil
		}
		lastErr = err
		if len(chain) > 1 {
			lastErr = fmt.Errorf("%s: %w", provider.id, err)
		}
		if !answered || ctx.Err() != nil {
			break
		}
	}
	return IPInfo{}, lastErr
}

type ipLookupProvider struct {
	id  string
	url string
	// Returns the IP and country code, or the failure the service reports.
	parse func(body []byte) (string, string, error)
}

var ipLookupProviders = []ipLookupProvider{
	{"ip2location", "https://api.ip2location.io/", parseIP2Location},
	{"ip-api", "http://ip-api.com/json/", parseIPAPI},
	{"ipwho", "https://ipwho.is/", parseIPWho},
	{"ipinfo", "https://ipinfo.io/json", jsonFields("ip", "country")},
	{"ipapi-co", "https://ipapi.co/json/", parseIPAPICo},
	{"freeipapi", "https://free.freeipapi.com/api/v1/json", jsonFields("ipAddress", "countryCode")},
	{"cloudflare", "https://www.cloudflare.com/cdn-cgi/trace", parseCloudflareTrace},
}

// Same order as the GUI's automatic exit lookup.
var ipLookupAuto = []string{"ip2location", "ip-api", "ipwho", "ipinfo", "cloudflare"}

func ipLookupChain(provider string) []ipLookupProvider {
	find := func(id string) (ipLookupProvider, bool) {
		for _, p := range ipLookupProviders {
			if p.id == id {
				return p, true
			}
		}
		return ipLookupProvider{}, false
	}
	if p, ok := find(strings.TrimSpace(provider)); ok {
		return []ipLookupProvider{p}
	}
	chain := make([]ipLookupProvider, 0, len(ipLookupAuto))
	for _, id := range ipLookupAuto {
		p, _ := find(id)
		chain = append(chain, p)
	}
	return chain
}

func (p ipLookupProvider) lookup(ctx context.Context, client *http.Client, timeout time.Duration) (IPInfo, bool, error) {
	ctx, cancel := context.WithTimeout(ctx, timeout)
	defer cancel()
	req, err := http.NewRequestWithContext(ctx, http.MethodGet, p.url, nil)
	if err != nil {
		return IPInfo{}, false, err
	}
	req.Header.Set("Accept", "application/json, text/plain, */*")
	resp, err := client.Do(req)
	if err != nil {
		return IPInfo{}, false, err
	}
	defer resp.Body.Close()
	body, err := io.ReadAll(io.LimitReader(resp.Body, 64<<10))
	if err != nil {
		return IPInfo{}, true, err
	}
	ip, country, parseErr := p.parse(body)
	addr, addrErr := netip.ParseAddr(strings.TrimSpace(ip))
	switch {
	case parseErr != nil && resp.StatusCode != http.StatusOK:
		return IPInfo{}, true, fmt.Errorf("HTTP %d: %w", resp.StatusCode, parseErr)
	case parseErr != nil:
		return IPInfo{}, true, parseErr
	case addrErr != nil:
		if resp.StatusCode != http.StatusOK {
			return IPInfo{}, true, fmt.Errorf("HTTP %d", resp.StatusCode)
		}
		return IPInfo{}, true, errors.New("no IP in the answer")
	}
	return IPInfo{IP: addr.Unmap().String(), CountryCode: servicecheck.NormalizeRegion(country)}, true, nil
}

func decodeObject(body []byte) (map[string]any, error) {
	var obj map[string]any
	if err := json.Unmarshal(body, &obj); err != nil || obj == nil {
		return nil, errors.New("unparsable answer")
	}
	return obj, nil
}

func stringField(obj map[string]any, key string) string {
	value, _ := obj[key].(string)
	value = strings.TrimSpace(value)
	// IP2Location answers "-" for whatever it does not know.
	if value == "-" {
		return ""
	}
	return value
}

func failure(reason string) error {
	if reason == "" {
		return errors.New("the service reported a failure")
	}
	return errors.New(reason)
}

func jsonFields(ipKey, countryKey string) func([]byte) (string, string, error) {
	return func(body []byte) (string, string, error) {
		obj, err := decodeObject(body)
		if err != nil {
			return "", "", err
		}
		return stringField(obj, ipKey), stringField(obj, countryKey), nil
	}
}

func parseIP2Location(body []byte) (string, string, error) {
	obj, err := decodeObject(body)
	if err != nil {
		return "", "", err
	}
	if e, ok := obj["error"].(map[string]any); ok {
		return "", "", failure(stringField(e, "error_message"))
	}
	return stringField(obj, "ip"), stringField(obj, "country_code"), nil
}

func parseIPAPI(body []byte) (string, string, error) {
	obj, err := decodeObject(body)
	if err != nil {
		return "", "", err
	}
	if stringField(obj, "status") != "success" {
		return "", "", failure(stringField(obj, "message"))
	}
	return stringField(obj, "query"), stringField(obj, "countryCode"), nil
}

func parseIPWho(body []byte) (string, string, error) {
	obj, err := decodeObject(body)
	if err != nil {
		return "", "", err
	}
	if success, ok := obj["success"].(bool); ok && !success {
		return "", "", failure(stringField(obj, "message"))
	}
	return stringField(obj, "ip"), stringField(obj, "country_code"), nil
}

func parseIPAPICo(body []byte) (string, string, error) {
	obj, err := decodeObject(body)
	if err != nil {
		return "", "", err
	}
	if failed, _ := obj["error"].(bool); failed {
		return "", "", failure(stringField(obj, "reason"))
	}
	return stringField(obj, "ip"), stringField(obj, "country_code"), nil
}

func parseCloudflareTrace(body []byte) (string, string, error) {
	var ip, country string
	for _, line := range strings.Split(string(body), "\n") {
		key, value, found := strings.Cut(line, "=")
		if !found {
			continue
		}
		switch strings.TrimSpace(key) {
		case "ip":
			ip = strings.TrimSpace(value)
		case "loc":
			country = strings.TrimSpace(value)
		}
	}
	return ip, country, nil
}
