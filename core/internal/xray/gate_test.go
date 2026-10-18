package xray

import (
	"encoding/json"
	"io"
	"net"
	"strconv"
	"testing"
	"time"

	_ "github.com/xtls/xray-core/app/dispatcher"
	_ "github.com/xtls/xray-core/app/proxyman/inbound"
	_ "github.com/xtls/xray-core/app/proxyman/outbound"
	_ "github.com/xtls/xray-core/proxy/freedom"
	_ "github.com/xtls/xray-core/proxy/socks"
	_ "github.com/xtls/xray-core/transport/internet/tagged/taggedimpl"
	_ "github.com/xtls/xray-core/transport/internet/tcp"
)

func TestGateLayoutRendersPortsInPlace(t *testing.T) {
	const config = `{
		"inbounds": [
			{"tag": "a-inbound", "listen": "127.0.0.1", "port": 34567, "protocol": "socks"},
			{"tag": "b-inbound", "listen": "127.0.0.1", "port": 34568, "protocol": "socks"}
		],
		"outbounds": [{"tag": "a", "protocol": "freedom"}]
	}`

	layout, err := parseGateLayout(config)
	if err != nil {
		t.Fatalf("parseGateLayout: %v", err)
	}
	if len(layout.ports) != 2 || layout.ports[0] != 34567 || layout.ports[1] != 34568 {
		t.Fatalf("gated ports %v, want [34567 34568]", layout.ports)
	}

	for _, ports := range [][]int{{40001, 40002}, {40003, 40004}} {
		rendered, err := layout.render(ports)
		if err != nil {
			t.Fatalf("render: %v", err)
		}
		var parsed struct {
			Inbounds []struct {
				Tag  string `json:"tag"`
				Port int    `json:"port"`
			} `json:"inbounds"`
			Outbounds []struct {
				Tag string `json:"tag"`
			} `json:"outbounds"`
		}
		if err = json.Unmarshal([]byte(rendered), &parsed); err != nil {
			t.Fatalf("rendered config does not parse: %v", err)
		}
		if len(parsed.Inbounds) != 2 || parsed.Inbounds[0].Tag != "a-inbound" || parsed.Inbounds[1].Tag != "b-inbound" {
			t.Fatalf("inbound identity lost: %+v", parsed.Inbounds)
		}
		for i, inbound := range parsed.Inbounds {
			if inbound.Port != ports[i] {
				t.Errorf("inbound %d listens on %d, want %d", i, inbound.Port, ports[i])
			}
		}
		if len(parsed.Outbounds) != 1 || parsed.Outbounds[0].Tag != "a" {
			t.Errorf("outbounds mangled: %+v", parsed.Outbounds)
		}
	}
	if layout.ports[0] != 34567 || layout.inbounds[0]["port"].(float64) != 34567 {
		t.Error("render wrote through to the layout")
	}
}

// Ports are probed by binding and releasing, so a one-at-a-time probe can hand the same port to two inbounds.
func TestReservePortsAssignsDistinctPorts(t *testing.T) {
	hosts := make([]string, 16)
	for i := range hosts {
		hosts[i] = "127.0.0.1"
	}
	probes, ports, err := reservePorts(hosts)
	if err != nil {
		t.Fatalf("reservePorts: %v", err)
	}
	defer closeListeners(probes)
	seen := make(map[int]bool, len(ports))
	for _, port := range ports {
		if seen[port] {
			t.Fatalf("port %d handed out twice", port)
		}
		seen[port] = true
	}
}

func TestParseGateLayoutRejectsUngateableConfigs(t *testing.T) {
	for name, config := range map[string]string{
		"no inbounds":  `{"outbounds": []}`,
		"empty array":  `{"inbounds": []}`,
		"missing port": `{"inbounds": [{"listen": "127.0.0.1", "protocol": "socks"}]}`,
		"zero port":    `{"inbounds": [{"listen": "127.0.0.1", "port": 0}]}`,
		"not json":     `{`,
	} {
		t.Run(name, func(t *testing.T) {
			if _, err := parseGateLayout(config); err == nil {
				t.Fatal("expected an error, got none")
			}
		})
	}
}

func freePort(t *testing.T) int {
	t.Helper()
	probe, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatal(err)
	}
	port := probe.Addr().(*net.TCPAddr).Port
	if err = probe.Close(); err != nil {
		t.Fatal(err)
	}
	return port
}

func socksConfig(port int) string {
	return `{"inbounds":[{"tag":"t","listen":"127.0.0.1","port":` + strconv.Itoa(port) +
		`,"protocol":"socks","settings":{"auth":"noauth","udp":true}}],` +
		`"outbounds":[{"tag":"out","protocol":"freedom"}]}`
}

// A closed port reads to the auto-selector as a dead member rather than a cold one.
func TestGateListensBeforeAnyInstanceStarts(t *testing.T) {
	port := freePort(t)
	gate, err := StartGate(socksConfig(port), 0, nil)
	if err != nil {
		t.Fatalf("StartGate: %v", err)
	}
	defer gate.Close()

	if instance := gate.Instance(); instance != nil {
		t.Error("gate started an instance before anything dialed it")
	}
	conn, err := net.Dial("tcp", net.JoinHostPort("127.0.0.1", strconv.Itoa(port)))
	if err != nil {
		t.Fatalf("gated port refused a connection while cold: %v", err)
	}
	_ = conn.Close()
}

func socksGreeting(t *testing.T, address string) {
	t.Helper()
	conn, err := net.Dial("tcp", address)
	if err != nil {
		t.Fatalf("dial %s: %v", address, err)
	}
	defer conn.Close()
	_ = conn.SetDeadline(time.Now().Add(10 * time.Second))
	if _, err = conn.Write([]byte{5, 1, 0}); err != nil {
		t.Fatalf("write greeting: %v", err)
	}
	reply := make([]byte, 2)
	if _, err = io.ReadFull(conn, reply); err != nil {
		t.Fatalf("read greeting reply: %v", err)
	}
	if reply[0] != 5 || reply[1] != 0 {
		t.Fatalf("greeting reply %v, want [5 0]", reply)
	}
}

func waitIdle(t *testing.T, gate *Gate) {
	t.Helper()
	deadline := time.Now().Add(5 * time.Second)
	for gate.active.Load() > 0 {
		if time.Now().After(deadline) {
			t.Fatal("gate connections never drained")
		}
		time.Sleep(10 * time.Millisecond)
	}
}

// A swept instance's internal port is free for anyone while the gate is cold; the next start must not need it back.
func TestGateRestartsOnFreshPortsAfterShutdown(t *testing.T) {
	port := freePort(t)
	gated := net.JoinHostPort("127.0.0.1", strconv.Itoa(port))
	gate, err := StartGate(socksConfig(port), 0, nil)
	if err != nil {
		t.Fatalf("StartGate: %v", err)
	}
	defer gate.Close()

	socksGreeting(t, gated)
	waitIdle(t, gate)

	gate.mu.Lock()
	instance, first := gate.instance, gate.targets
	gate.instance, gate.targets = nil, nil
	gate.mu.Unlock()
	if instance == nil || len(first) != 1 {
		t.Fatal("no instance came up for the first connection")
	}
	if first[0] == gated {
		t.Fatalf("instance listens on the gated port %s", gated)
	}
	_ = instance.Close()

	squatter, err := net.Listen("tcp", first[0])
	if err != nil {
		t.Skipf("cannot occupy %s for the test: %v", first[0], err)
	}
	defer squatter.Close()

	socksGreeting(t, gated)
	gate.mu.Lock()
	second := gate.targets
	gate.mu.Unlock()
	if len(second) != 1 || second[0] == first[0] {
		t.Fatalf("restart reused %v, want a fresh port", second)
	}
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
