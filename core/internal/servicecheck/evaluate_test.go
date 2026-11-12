package servicecheck

import (
	"net/http"
	"testing"
)

func TestEvaluateMatchesStatusRule(t *testing.T) {
	check := parseCheckFromFile(t, "check_available.json")
	responses := []*Response{{
		Status: 200,
		URL:    "https://example.com/",
		Header: make(http.Header),
	}}
	outcome := check.Evaluate(responses)
	if outcome.Status != StatusAvailable {
		t.Fatalf("status = %q, want %q", outcome.Status, StatusAvailable)
	}
}

func TestEvaluateDenyRegionOverridesAvailable(t *testing.T) {
	check := parseCheckFromFile(t, "check_deny_region.json")
	outcome := check.Evaluate([]*Response{{
		Status: 200,
		URL:    "https://example.com/cdn-cgi/trace",
		Header: make(http.Header),
		Body:   []byte("loc=CN\n"),
	}})
	if outcome.Region != "CN" {
		t.Fatalf("region = %q, want CN", outcome.Region)
	}
	if outcome.Status != StatusRestricted {
		t.Fatalf("status = %q, want %q", outcome.Status, StatusRestricted)
	}
}

func TestEvaluateRequestError(t *testing.T) {
	check := parseCheckFromFile(t, "check_request_error.json")
	outcome := check.Evaluate([]*Response{{Err: http.ErrServerClosed}})
	if outcome.Status != StatusFailed {
		t.Fatalf("status = %q, want %q", outcome.Status, StatusFailed)
	}
}
