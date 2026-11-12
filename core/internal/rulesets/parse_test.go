package rulesets

import (
	"errors"
	"os"
	"path/filepath"
	"slices"
	"testing"
)

func readRulesetJSON(t *testing.T, fileName string) []byte {
	t.Helper()
	path := filepath.Join("testdata", fileName)
	bytes, err := os.ReadFile(path)
	if err != nil {
		t.Fatalf("read %s: %v", fileName, err)
	}
	return bytes
}

func TestParseIPCIDRsPlainJSON(t *testing.T) {
	cidrs, skipped, version, err := ParseIPCIDRs(readRulesetJSON(t, "plain_cidrs.json"))
	if err != nil {
		t.Fatal(err)
	}
	if version != 1 {
		t.Fatalf("version = %d, want 1", version)
	}
	if skipped != 0 {
		t.Fatalf("skipped = %d, want 0", skipped)
	}
	want := []string{"192.168.0.0/24", "10.0.0.1", "2001:db8::1"}
	if len(cidrs) != len(want) {
		t.Fatalf("cidrs = %v", cidrs)
	}
	for _, prefix := range want {
		if !slices.Contains(cidrs, prefix) {
			t.Fatalf("missing %q in %v", prefix, cidrs)
		}
	}
}

func TestParseIPCIDRsSkipsInvertedRule(t *testing.T) {
	_, skipped, _, err := ParseIPCIDRs(readRulesetJSON(t, "skip_inverted.json"))
	if err != nil {
		t.Fatal(err)
	}
	if skipped != 1 {
		t.Fatalf("skipped = %d, want 1", skipped)
	}
}

func TestParseIPCIDRsNoRanges(t *testing.T) {
	_, _, _, err := ParseIPCIDRs(readRulesetJSON(t, "no_ranges.json"))
	if !errors.Is(err, ErrNoIPRanges) {
		t.Fatalf("err = %v, want ErrNoIPRanges", err)
	}
}

func TestParseIPCIDRsInvalidJSON(t *testing.T) {
	_, _, _, err := ParseIPCIDRs([]byte(`{`))
	if err == nil {
		t.Fatal("expected error for broken JSON")
	}
}
