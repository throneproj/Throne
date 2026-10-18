package xray

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log"
	"maps"
	"net"
	"strconv"
	"sync"
	"sync/atomic"
	"time"

	"github.com/xtls/xray-core/core"
)

const (
	gateSweepInterval    = 15 * time.Second
	minGateSweepInterval = 2 * time.Second
	gateStartAttempts    = 3
	// Connections queued behind a failed start share its error instead of each rebuilding the instance.
	gateStartCooldown = time.Second
)

// FailureLog reports a gate dropping its connections; a host whose log sink grades lines points it at the error level.
var FailureLog = func(message string) { log.Println(message) }

func sweepIntervalFor(idle time.Duration) time.Duration {
	if idle <= 0 || idle/2 >= gateSweepInterval {
		return gateSweepInterval
	}
	if half := idle / 2; half > minGateSweepInterval {
		return half
	}
	return minGateSweepInterval
}

// The gate owns the inbound ports for the whole life of the config: a dial landing while the instance is down must not hit a closed port.
// No UDP gate: Xray serves SOCKS5 UDP ASSOCIATE from a per-session ephemeral hub, and the association's TCP control connection keeps the instance from being swept.
type Gate struct {
	layout  *gateLayout
	idle    time.Duration
	prepare func(*core.Instance) error

	bridges []net.Listener
	stopped chan struct{}

	mu       sync.Mutex
	instance *core.Instance
	targets  []string // per bridge, set together with instance
	closed   bool
	failure  error
	failedAt time.Time

	// conns is nil once the gate is closed, which is how a connection accepted during teardown learns to give up.
	connsMu sync.Mutex
	conns   map[net.Conn]struct{}

	active  atomic.Int64
	lastUse atomic.Int64 // unix nanos
}

// The gated inbounds; every instance start writes freshly drawn ports into a copy of them.
type gateLayout struct {
	root     map[string]json.RawMessage
	inbounds []map[string]any
	hosts    []string
	ports    []int
}

// prepare must apply to each instance everything an eagerly created one would get (egress interface, outbound DNS); idle 0 keeps it resident.
func StartGate(config string, idle time.Duration, prepare func(*core.Instance) error) (*Gate, error) {
	layout, err := parseGateLayout(config)
	if err != nil {
		return nil, err
	}
	checked, err := layout.render(layout.ports)
	if err != nil {
		return nil, err
	}
	if err = CheckXrayConfig(checked); err != nil {
		return nil, err
	}

	gate := &Gate{
		layout:  layout,
		idle:    idle,
		prepare: prepare,
		stopped: make(chan struct{}),
		conns:   make(map[net.Conn]struct{}),
	}
	gate.touch()

	// Bound before any internal port is drawn, so a draw can never hand out a port the gate is about to take.
	for i, host := range layout.hosts {
		address := net.JoinHostPort(host, strconv.Itoa(layout.ports[i]))
		listener, listenErr := net.Listen("tcp", address)
		if listenErr != nil {
			closeListeners(gate.bridges)
			return nil, fmt.Errorf("xray gate: cannot take over %s: %w", address, listenErr)
		}
		gate.bridges = append(gate.bridges, listener)
	}

	for i, bridge := range gate.bridges {
		go gate.serve(bridge, i)
	}
	go gate.sweep()
	return gate, nil
}

func parseGateLayout(config string) (*gateLayout, error) {
	var root map[string]json.RawMessage
	if err := json.Unmarshal([]byte(config), &root); err != nil {
		return nil, fmt.Errorf("xray gate: parse config: %w", err)
	}
	var inbounds []map[string]any
	if raw, ok := root["inbounds"]; ok {
		if err := json.Unmarshal(raw, &inbounds); err != nil {
			return nil, fmt.Errorf("xray gate: parse inbounds: %w", err)
		}
	}
	if len(inbounds) == 0 {
		return nil, errors.New("xray gate: config has no inbounds to gate")
	}

	layout := &gateLayout{root: root, inbounds: inbounds}
	for _, inbound := range inbounds {
		host, _ := inbound["listen"].(string)
		if host == "" {
			host = "127.0.0.1"
		}
		port, ok := inbound["port"].(float64)
		if !ok || port <= 0 {
			return nil, errors.New("xray gate: inbound without a fixed port cannot be gated")
		}
		layout.hosts = append(layout.hosts, host)
		layout.ports = append(layout.ports, int(port))
	}
	return layout, nil
}

// Only the inbounds array is re-encoded, so hand-written outbound JSON reaches Xray byte for byte.
func (l *gateLayout) render(ports []int) (string, error) {
	inbounds := make([]map[string]any, len(l.inbounds))
	for i, inbound := range l.inbounds {
		inbounds[i] = maps.Clone(inbound)
		inbounds[i]["port"] = ports[i]
	}
	patched, err := json.Marshal(inbounds)
	if err != nil {
		return "", err
	}
	root := maps.Clone(l.root)
	root["inbounds"] = patched
	out, err := json.Marshal(root)
	if err != nil {
		return "", err
	}
	return string(out), nil
}

// Every probe stays open until the whole set is drawn: a released ephemeral port gets dealt straight back out.
func reservePorts(hosts []string) ([]net.Listener, []int, error) {
	probes := make([]net.Listener, 0, len(hosts))
	ports := make([]int, 0, len(hosts))
	for _, host := range hosts {
		probe, err := probeListenConfig.Listen(context.Background(), "tcp", net.JoinHostPort(host, "0"))
		if err != nil {
			closeListeners(probes)
			return nil, nil, fmt.Errorf("xray gate: no free port on %s: %w", host, err)
		}
		probes = append(probes, probe)
		ports = append(ports, probe.Addr().(*net.TCPAddr).Port)
	}
	return probes, ports, nil
}

func closeListeners(listeners []net.Listener) {
	for _, listener := range listeners {
		_ = listener.Close()
	}
}

// nil while the gate is cold.
func (g *Gate) Instance() *core.Instance {
	g.mu.Lock()
	defer g.mu.Unlock()
	return g.instance
}

func (g *Gate) Close() {
	g.mu.Lock()
	if g.closed {
		g.mu.Unlock()
		return
	}
	g.closed = true
	instance := g.instance
	g.instance, g.targets = nil, nil
	g.mu.Unlock()

	close(g.stopped)
	closeListeners(g.bridges)

	g.connsMu.Lock()
	conns := g.conns
	g.conns = nil
	g.connsMu.Unlock()
	for conn := range conns {
		_ = conn.Close()
	}

	if instance != nil {
		_ = instance.Close()
	}
}

func (g *Gate) serve(bridge net.Listener, index int) {
	for {
		conn, err := bridge.Accept()
		if err != nil {
			return
		}
		go g.handle(conn, index)
	}
}

func (g *Gate) handle(conn net.Conn, index int) {
	if !g.track(conn) {
		_ = conn.Close()
		return
	}
	defer func() {
		g.forget(conn)
		_ = conn.Close()
	}()

	// Counted before the instance is up, or the sweeper could call the gate idle mid-setup.
	g.active.Add(1)
	g.touch()
	defer func() {
		g.active.Add(-1)
		g.touch()
	}()

	targets, err := g.ensure()
	if err != nil {
		return
	}
	upstream, err := net.Dial("tcp", targets[index])
	if err != nil {
		log.Println("Xray gate: failed to reach the instance:", err)
		return
	}
	if !g.track(upstream) {
		_ = upstream.Close()
		return
	}
	defer func() {
		g.forget(upstream)
		_ = upstream.Close()
	}()

	done := make(chan struct{}, 2)
	go relay(upstream, conn, done)
	go relay(conn, upstream, done)
	<-done
	<-done
}

// Half-closes dst on EOF; closing both ends on the first EOF would truncate a download whose request body ended early.
func relay(dst, src net.Conn, done chan<- struct{}) {
	defer func() { done <- struct{}{} }()
	_, _ = io.Copy(dst, src)
	if closer, ok := dst.(interface{ CloseWrite() error }); ok {
		_ = closer.CloseWrite()
	}
}

// Logs a failure once per start, not once per connection it drops.
func (g *Gate) ensure() ([]string, error) {
	g.mu.Lock()
	defer g.mu.Unlock()
	if g.closed {
		return nil, errors.New("xray gate: closed")
	}
	if g.instance != nil {
		return g.targets, nil
	}
	if g.failure != nil && time.Since(g.failedAt) < gateStartCooldown {
		return nil, g.failure
	}

	var err error
	for attempt := 1; attempt <= gateStartAttempts; attempt++ {
		var instance *core.Instance
		var targets []string
		// Retries release the probes first, in case this kernel refuses Xray a port the probe still holds.
		hold := probesHoldThroughStart && attempt == 1
		if instance, targets, err = g.startInstance(hold); err == nil {
			log.Println("Xray gate: started the instance on demand")
			g.instance, g.targets, g.failure = instance, targets, nil
			return targets, nil
		}
		if !isAddrInUse(err) {
			break
		}
		log.Printf("Xray gate: an internal port was taken during start (attempt %d of %d): %v", attempt, gateStartAttempts, err)
	}
	g.failure, g.failedAt = err, time.Now()
	FailureLog(fmt.Sprintf("Xray gate: cannot start the instance behind %s, dropping its connections: %v", g.describe(), err))
	return nil, err
}

// Draws new internal ports on every start: the ones a swept instance used were free for anyone to take while it was down.
func (g *Gate) startInstance(holdProbes bool) (*core.Instance, []string, error) {
	probes, ports, err := reservePorts(g.layout.hosts)
	if err != nil {
		return nil, nil, err
	}
	defer func() { closeListeners(probes) }()

	config, err := g.layout.render(ports)
	if err != nil {
		return nil, nil, err
	}
	instance, err := CreateXrayInstance(config)
	if err != nil {
		return nil, nil, err
	}
	if g.prepare != nil {
		if err = g.prepare(instance); err != nil {
			_ = instance.Close()
			return nil, nil, err
		}
	}
	if !holdProbes {
		closeListeners(probes)
		probes = nil
	}
	if err = instance.Start(); err != nil {
		_ = instance.Close()
		return nil, nil, err
	}

	targets := make([]string, len(ports))
	for i, port := range ports {
		targets[i] = net.JoinHostPort(g.layout.hosts[i], strconv.Itoa(port))
	}
	return instance, targets, nil
}

func (g *Gate) describe() string {
	first := g.bridges[0].Addr().String()
	if more := len(g.bridges) - 1; more > 0 {
		return fmt.Sprintf("%s and %d more ports", first, more)
	}
	return first
}

func (g *Gate) idleNow() bool {
	return g.idle > 0 && g.active.Load() == 0 && time.Since(time.Unix(0, g.lastUse.Load())) >= g.idle
}

func (g *Gate) sweep() {
	ticker := time.NewTicker(sweepIntervalFor(g.idle))
	defer ticker.Stop()
	for {
		select {
		case <-g.stopped:
			return
		case <-ticker.C:
			if !g.idleNow() {
				continue
			}
			// Rechecked under mu: a connection counted after the first check must not be handed an instance about to close.
			var instance *core.Instance
			g.mu.Lock()
			if g.idleNow() {
				instance = g.instance
				g.instance, g.targets = nil, nil
			}
			g.mu.Unlock()
			if instance != nil {
				log.Println("Xray gate: the instance went idle, shutting it down")
				_ = instance.Close()
			}
		}
	}
}

func (g *Gate) touch() {
	g.lastUse.Store(time.Now().UnixNano())
}

// Reports false once the gate is closed and nothing will come back for the connection.
func (g *Gate) track(conn net.Conn) bool {
	g.connsMu.Lock()
	defer g.connsMu.Unlock()
	if g.conns == nil {
		return false
	}
	g.conns[conn] = struct{}{}
	return true
}

func (g *Gate) forget(conn net.Conn) {
	g.connsMu.Lock()
	defer g.connsMu.Unlock()
	delete(g.conns, conn)
}
