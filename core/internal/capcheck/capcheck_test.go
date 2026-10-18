package capcheck

import (
	"bytes"
	"encoding/json"
	"os"
	"runtime"
	"strings"
	"testing"
)

func TestPrintIsOneJSONLine(t *testing.T) {
	var out bytes.Buffer
	if err := Print(&out); err != nil {
		t.Fatal(err)
	}
	line, ok := strings.CutSuffix(out.String(), "\n")
	if !ok || strings.Contains(line, "\n") {
		t.Fatalf("want one newline-terminated line, got %q", out.String())
	}
	var fields map[string]any
	if err := json.Unmarshal([]byte(line), &fields); err != nil {
		t.Fatal(err)
	}
	for _, key := range []string{"tun_device", "net_admin", "kill_switch_privilege", "detail", "euid", "ruid", "setuid", "kernel", "nftables", "nftables_error"} {
		if _, ok := fields[key]; !ok {
			t.Errorf("missing %q in %s", key, line)
		}
	}
	if fields["euid"] != float64(os.Geteuid()) || fields["ruid"] != float64(os.Getuid()) {
		t.Errorf("euid/ruid %v/%v, want %d/%d", fields["euid"], fields["ruid"], os.Geteuid(), os.Getuid())
	}
	if runtime.GOOS != "linux" && fields["nftables"] != nil {
		t.Errorf("nftables = %v on %s, want null", fields["nftables"], runtime.GOOS)
	}
}

func TestReportDetailNamesTheFirstFailure(t *testing.T) {
	report := Report()
	failed := false
	for _, value := range []*bool{report.TunDevice, report.NetAdmin, report.KillSwitchPrivilege} {
		if value != nil && !*value {
			failed = true
		}
	}
	if failed && report.Detail == "" {
		t.Errorf("a capability failed but detail is empty: %+v", report)
	}
	if !failed && report.TunDevice != nil && report.NetAdmin != nil && report.KillSwitchPrivilege != nil && report.Detail != "" {
		t.Errorf("nothing failed but detail is %q", report.Detail)
	}
}
