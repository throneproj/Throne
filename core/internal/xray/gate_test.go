package xray

import (
	"encoding/json"
	"net"
	"strconv"
	"testing"
	"time"
)

func TestRemapInboundsMovesPortsBehindTheOriginals(t *testing.T) {
	const config = `{
		"inbounds": [
			{"tag": "a-inbound", "listen": "127.0.0.1", "port": 34567, "protocol": "socks"},
			{"tag": "b-inbound", "listen": "127.0.0.1", "port": 34568, "protocol": "socks"}
		],
		"outbounds": [{"tag": "a", "protocol": "freedom"}]
	}`

	rewritten, pairs, err := remapInbounds(config)
	if err != nil {
		t.Fatalf("remapInbounds: %v", err)
	}
	if len(pairs) != 2 {
		t.Fatalf("got %d pairs, want 2", len(pairs))
	}
	for i, want := range []string{"127.0.0.1:34567", "127.0.0.1:34568"} {
		if pairs[i].gate != want {
			t.Errorf("pair %d gates %q, want %q", i, pairs[i].gate, want)
		}
		if pairs[i].target == want {
			t.Errorf("pair %d left Xray on the gated port %q", i, want)
		}
	}

	var parsed struct {
		Inbounds []struct {
			Tag    string `json:"tag"`
			Listen string `json:"listen"`
			Port   int    `json:"port"`
		} `json:"inbounds"`
		Outbounds []struct {
			Tag string `json:"tag"`
		} `json:"outbounds"`
	}
	if err = json.Unmarshal([]byte(rewritten), &parsed); err != nil {
		t.Fatalf("rewritten config does not parse: %v", err)
	}
	for i, inbound := range parsed.Inbounds {
		want := net.JoinHostPort(inbound.Listen, strconv.Itoa(inbound.Port))
		if want != pairs[i].target {
			t.Errorf("inbound %d listens on %q, but the gate forwards to %q", i, want, pairs[i].target)
		}
	}
	if len(parsed.Inbounds) != 2 || parsed.Inbounds[0].Tag != "a-inbound" {
		t.Errorf("inbound identity lost: %+v", parsed.Inbounds)
	}
	if len(parsed.Outbounds) != 1 || parsed.Outbounds[0].Tag != "a" {
		t.Errorf("outbounds mangled: %+v", parsed.Outbounds)
	}
}

// Ports are probed by binding and releasing, so a one-at-a-time probe can hand the same port to two inbounds.
func TestRemapInboundsAssignsDistinctPorts(t *testing.T) {
	inbounds := make([]map[string]any, 0, 16)
	for i := 0; i < 16; i++ {
		inbounds = append(inbounds, map[string]any{
			"listen":   "127.0.0.1",
			"port":     40000 + i,
			"protocol": "socks",
		})
	}
	raw, err := json.Marshal(map[string]any{"inbounds": inbounds})
	if err != nil {
		t.Fatal(err)
	}

	_, pairs, err := remapInbounds(string(raw))
	if err != nil {
		t.Fatalf("remapInbounds: %v", err)
	}
	seen := make(map[string]bool, len(pairs))
	for _, pair := range pairs {
		if seen[pair.target] {
			t.Fatalf("port %q handed out twice", pair.target)
		}
		seen[pair.target] = true
	}
}

func TestRemapInboundsRejectsUngateableConfigs(t *testing.T) {
	for name, config := range map[string]string{
		"no inbounds":  `{"outbounds": []}`,
		"empty array":  `{"inbounds": []}`,
		"missing port": `{"inbounds": [{"listen": "127.0.0.1", "protocol": "socks"}]}`,
		"zero port":    `{"inbounds": [{"listen": "127.0.0.1", "port": 0}]}`,
		"not json":     `{`,
	} {
		t.Run(name, func(t *testing.T) {
			if _, _, err := remapInbounds(config); err == nil {
				t.Fatal("expected an error, got none")
			}
		})
	}
}

// A closed port reads to the auto-selector as a dead member rather than a cold one.
func TestGateListensBeforeAnyInstanceStarts(t *testing.T) {
	probe, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	gated := probe.Addr().String()
	port := probe.Addr().(*net.TCPAddr).Port
	if err = probe.Close(); err != nil {
		t.Fatal(err)
	}

	config := `{"inbounds":[{"tag":"t","listen":"127.0.0.1","port":` + strconv.Itoa(port) +
		`,"protocol":"socks","settings":{"auth":"noauth","udp":true}}],` +
		`"outbounds":[{"tag":"out","protocol":"freedom"}]}`

	gate, err := StartGate(config, 0, nil)
	if err != nil {
		t.Fatalf("StartGate: %v", err)
	}
	defer gate.Close()

	if instance := gate.Instance(); instance != nil {
		t.Error("gate started an instance before anything dialed it")
	}
	conn, err := net.Dial("tcp", gated)
	if err != nil {
		t.Fatalf("gated port refused a connection while cold: %v", err)
	}
	_ = conn.Close()
}

func TestSweepIntervalFor(t *testing.T) {
	cases := []struct {
		name string
		idle time.Duration
		want time.Duration
	}{
		{"resident keeps the coarse tick", 0, gateSweepInterval},
		{"long idle keeps the coarse tick", 10 * time.Minute, gateSweepInterval},
		{"pool idle halves it", 30 * time.Second, 15 * time.Second},
		{"short idle tightens it", 10 * time.Second, 5 * time.Second},
		{"tiny idle stops at the floor", 2 * time.Second, minGateSweepInterval},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			if got := sweepIntervalFor(tc.idle); got != tc.want {
				t.Fatalf("sweepIntervalFor(%v) = %v, want %v", tc.idle, got, tc.want)
			}
		})
	}
}
