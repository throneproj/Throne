package probe

import (
	"context"
	"errors"
	"net/http"
	"testing"
	"time"
)

func TestParseIPRiskInfo(t *testing.T) {
	for _, test := range []struct {
		name string
		body string
		want IPInfo
	}{
		{"zero", `{"ip":"192.0.2.1","countryCode":"US","fraudScore":0}`, IPInfo{IP: "192.0.2.1", CountryCode: "US", FraudScore: 0}},
		{"ipv6", `{"ip":"2001:db8::1","countryCode":"JP","fraudScore":75,"asn":17506}`, IPInfo{IP: "2001:db8::1", CountryCode: "JP", FraudScore: 75}},
	} {
		t.Run(test.name, func(t *testing.T) {
			got, err := parseIPRiskInfo([]byte(test.body))
			if err != nil {
				t.Fatal(err)
			}
			if got != test.want {
				t.Fatalf("got %+v, want %+v", got, test.want)
			}
		})
	}
}

func TestParseIPRiskInfoRejectsInvalidResults(t *testing.T) {
	for _, test := range []struct {
		name string
		body string
	}{
		{"missing score", `{"ip":"192.0.2.1"}`},
		{"null score", `{"ip":"192.0.2.1","fraudScore":null}`},
		{"string score", `{"ip":"192.0.2.1","fraudScore":"0"}`},
		{"fractional score", `{"ip":"192.0.2.1","fraudScore":1.5}`},
		{"negative score", `{"ip":"192.0.2.1","fraudScore":-1}`},
		{"missing IP", `{"fraudScore":0}`},
		{"invalid IP", `{"ip":"not-an-ip","fraudScore":0}`},
		{"scoped IP", `{"ip":"fe80::1%eth0","fraudScore":0}`},
		{"API error", `{"error":"rate limited"}`},
		{"invalid JSON", `<html>Bad gateway</html>`},
	} {
		t.Run(test.name, func(t *testing.T) {
			got, err := parseIPRiskInfo([]byte(test.body))
			if err == nil || got.FraudScore != -1 {
				t.Fatalf("got %+v, error %v; invalid results must not become a valid score", got, err)
			}
		})
	}
}

func TestIPRiskTestCancelled(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	cancel()
	got, err := ipRiskTest(ctx, &http.Client{}, time.Second)
	if !errors.Is(err, context.Canceled) || got.FraudScore != -1 {
		t.Fatalf("got %+v, error %v; want cancellation without a score", got, err)
	}
}
