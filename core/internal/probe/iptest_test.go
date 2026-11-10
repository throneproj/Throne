package probe

import (
	"context"
	"errors"
	"io"
	"net/http"
	"strings"
	"testing"
	"time"
)

type stubTransport func(*http.Request) (*http.Response, error)

func (f stubTransport) RoundTrip(r *http.Request) (*http.Response, error) { return f(r) }

func answer(status int, body string) *http.Response {
	return &http.Response{StatusCode: status, Body: io.NopCloser(strings.NewReader(body)), Header: http.Header{}}
}

func TestIPLookupParsers(t *testing.T) {
	cases := []struct {
		provider string
		status   int
		body     string
		ip, cc   string
		fails    bool
	}{
		{"ip2location", 200, `{"ip":"203.0.113.7","country_code":"DE","country_name":"Germany"}`, "203.0.113.7", "DE", false},
		{"ip2location", 200, `{"ip":"203.0.113.7","country_code":"-"}`, "203.0.113.7", "", false},
		{"ip2location", 401, `{"error":{"error_code":10000,"error_message":"Invalid API key."}}`, "", "", true},
		{"ip-api", 200, `{"status":"success","query":"2001:db8::7","countryCode":"FR"}`, "2001:db8::7", "FR", false},
		{"ip-api", 200, `{"status":"fail","message":"reserved range"}`, "", "", true},
		{"ipwho", 200, `{"success":true,"ip":"203.0.113.7","country_code":"NL"}`, "203.0.113.7", "NL", false},
		{"ipwho", 200, `{"success":false,"message":"Invalid IP address"}`, "", "", true},
		{"ipinfo", 200, `{"ip":"203.0.113.7","country":"US","city":"Ashburn"}`, "203.0.113.7", "US", false},
		{"ipapi-co", 200, `{"error":true,"reason":"RateLimited"}`, "", "", true},
		{"ipapi-co", 200, `{"ip":"203.0.113.7","country_code":"GB"}`, "203.0.113.7", "GB", false},
		{"freeipapi", 200, `{"ipAddress":"203.0.113.7","countryCode":"JP"}`, "203.0.113.7", "JP", false},
		{"cloudflare", 200, "fl=1\nip=203.0.113.7\nloc=XX\ntls=TLSv1.3\n", "203.0.113.7", "", false},
		{"cloudflare", 200, "ip=203.0.113.7\r\nloc=SE\r\n", "203.0.113.7", "SE", false},
		{"ipinfo", 429, `Too Many Requests`, "", "", true},
		{"ipinfo", 200, `{"ip":"bogus"}`, "", "", true},
	}
	for _, tc := range cases {
		chain := ipLookupChain(tc.provider)
		if len(chain) != 1 || chain[0].id != tc.provider {
			t.Fatalf("%s: chain %v", tc.provider, chain)
		}
		client := &http.Client{Transport: stubTransport(func(r *http.Request) (*http.Response, error) {
			return answer(tc.status, tc.body), nil
		})}
		info, err := ipTest(context.Background(), client, chain, time.Second, time.Second)
		if (err != nil) != tc.fails || info.IP != tc.ip || info.CountryCode != tc.cc {
			t.Errorf("%s %q: got %+v, %v", tc.provider, tc.body, info, err)
		}
	}
}

func TestIPLookupAutoChain(t *testing.T) {
	var asked []string
	client := &http.Client{Transport: stubTransport(func(r *http.Request) (*http.Response, error) {
		asked = append(asked, r.URL.Host)
		switch r.URL.Host {
		case "api.ip2location.io":
			return answer(429, `{"error":{"error_message":"Quota exceeded"}}`), nil
		case "ip-api.com":
			return answer(200, `{"status":"success","query":"203.0.113.7","countryCode":"DE"}`), nil
		}
		return nil, errors.New("unexpected")
	})}
	info, err := ipTest(context.Background(), client, ipLookupChain("auto"), time.Second, time.Second)
	if err != nil || info.CountryCode != "DE" || strings.Join(asked, ",") != "api.ip2location.io,ip-api.com" {
		t.Fatalf("got %+v, %v after %v", info, err, asked)
	}

	asked = nil
	dead := &http.Client{Transport: stubTransport(func(r *http.Request) (*http.Response, error) {
		asked = append(asked, r.URL.Host)
		return nil, errors.New("connection refused")
	})}
	if _, err := ipTest(context.Background(), dead, ipLookupChain(""), time.Second, time.Second); err == nil || len(asked) != 1 {
		t.Fatalf("dead exit: %v after %v", err, asked)
	}
}
