package probe

import (
	"context"
	"errors"
	"fmt"
	"net"
	"net/http"
	"sync"
	"time"

	"github.com/Mahdi-zarei/speedtest-go/speedtest"
	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing/common/metadata"
	"github.com/sagernet/sing/service"
)

// What a probe needs from an instance; *boxbox.Box and core/mobile's box handle both satisfy it.
type Box interface {
	Context() context.Context
	Outbound() adapter.OutboundManager
}

const FetchServersTimeout = 8 * time.Second
const MaxConcurrentTests = 100

const TunnelStartupTimeout = 5 * time.Second

// Bounds the wait for a tunnel that refuses dials until its handshake completes.
const TunnelHandshakeTimeout = 10 * time.Second

// The GUI matches on this text, so the wording is part of the contract.
var ErrTestAborted = errors.New("test aborted")

type testSession struct {
	mu     sync.Mutex
	ctx    context.Context
	cancel context.CancelFunc
}

var session testSession

func (s *testSession) current() context.Context {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.ctx == nil {
		s.ctx, s.cancel = context.WithCancel(context.Background())
	}
	return s.ctx
}

func (s *testSession) cancelAndRearm() {
	s.mu.Lock()
	defer s.mu.Unlock()
	if s.cancel != nil {
		s.cancel()
	}
	s.ctx, s.cancel = context.WithCancel(context.Background())
}

func TestContext() context.Context { return session.current() }

func CancelTests() { session.cancelAndRearm() }

// Drains on read: each result is handed to the GUI exactly once.
type resultBuffer[T any] struct {
	results []*T
	mu      sync.Mutex
}

func (b *resultBuffer[T]) AddResult(result *T) {
	b.mu.Lock()
	defer b.mu.Unlock()
	b.results = append(b.results, result)
}

func (b *resultBuffer[T]) Results() []*T {
	b.mu.Lock()
	defer b.mu.Unlock()
	res := b.results
	b.results = nil
	return res
}

// Anything left buffered is drained by the next test, whose tags repeat ours.
func (b *resultBuffer[T]) Reclaim(owned []*T) {
	if len(owned) == 0 {
		return
	}
	b.mu.Lock()
	defer b.mu.Unlock()
	if len(b.results) == 0 {
		return
	}
	drop := make(map[*T]struct{}, len(owned))
	for _, r := range owned {
		drop[r] = struct{}{}
	}
	kept := b.results[:0]
	for _, r := range b.results {
		if _, ours := drop[r]; !ours {
			kept = append(kept, r)
		}
	}
	b.results = kept
}

type batchProbe[T any] struct {
	run     func(ctx context.Context, tag string, outbound adapter.Outbound) *T
	fail    func(tag string, err error) *T
	publish func(*T)
}

func normalizeConcurrency(maxConcurrency int) int {
	if maxConcurrency <= 0 || maxConcurrency >= 500 {
		return MaxConcurrentTests
	}
	return maxConcurrency
}

// One result per tag, in the order given. Cancelling ctx aborts tags not yet started.
func runBatch[T any](ctx context.Context, i Box, outboundTags []string, maxConcurrency int, probe batchProbe[T]) []*T {
	outbounds := service.FromContext[adapter.OutboundManager](i.Context())
	resMap := make(map[string]*T, len(outboundTags))
	var resAccess sync.Mutex
	limiter := make(chan struct{}, normalizeConcurrency(maxConcurrency))

	store := func(tag string, res *T) {
		resAccess.Lock()
		resMap[tag] = res
		resAccess.Unlock()
	}

	wg := &sync.WaitGroup{}
	wg.Add(len(outboundTags))
	for _, tag := range outboundTags {
		select {
		case <-ctx.Done():
			// Not published: an aborted tag is not a measurement.
			store(tag, probe.fail(tag, ErrTestAborted))
			wg.Done()
			continue
		default:
		}

		time.Sleep(2 * time.Millisecond) // don't spawn goroutines too quickly
		limiter <- struct{}{}
		go func(t string) {
			defer wg.Done()
			defer func() { <-limiter }()

			outbound, found := outbounds.Outbound(t)
			if !found {
				// A tag can vanish between building the box and probing it; a panic here is out of reach of the dispatcher's recover.
				res := probe.fail(t, fmt.Errorf("no outbound with tag %s found", t))
				store(t, res)
				probe.publish(res)
				return
			}
			res := probe.run(ctx, t, outbound)
			store(t, res)
			probe.publish(res)
		}(tag)
	}

	wg.Wait()

	res := make([]*T, 0, len(outboundTags))
	for _, tag := range outboundTags {
		r, ok := resMap[tag]
		if !ok || r == nil {
			r = probe.fail(tag, errors.New("no result"))
		}
		res = append(res, r)
	}
	return res
}

// Remembers every conn a probe dials, so the closer can tear them down rather than leave them to a
// transport that outlives the box they were dialed through.
type probeDialer struct {
	dial   func(ctx context.Context, network, address string) (net.Conn, error)
	access sync.Mutex
	conns  []net.Conn
	closed bool
}

func (d *probeDialer) DialContext(ctx context.Context, network, address string) (net.Conn, error) {
	d.access.Lock()
	closed := d.closed
	d.access.Unlock()
	if closed {
		return nil, net.ErrClosed
	}
	conn, err := d.dial(ctx, network, address)
	if err != nil {
		return nil, err
	}
	// The closer can land mid-dial; that conn is ours to close, not to hand out.
	d.access.Lock()
	if d.closed {
		d.access.Unlock()
		_ = conn.Close()
		return nil, net.ErrClosed
	}
	d.conns = append(d.conns, conn)
	d.access.Unlock()
	return conn, nil
}

// A proxied conn's Close can block on its own teardown handshake, so it never runs on the caller.
func (d *probeDialer) Close() {
	d.access.Lock()
	conns := d.conns
	d.conns = nil
	d.closed = true
	d.access.Unlock()
	for _, conn := range conns {
		go func(conn net.Conn) { _ = conn.Close() }(conn)
	}
}

func dialerHTTPClient(dial func(ctx context.Context, network, address string) (net.Conn, error), timeout time.Duration) (*http.Client, func()) {
	probe := &probeDialer{dial: dial}
	transport := &http.Transport{DialContext: probe.DialContext}
	return &http.Client{Transport: transport, Timeout: timeout}, func() {
		probe.Close()
		transport.CloseIdleConnections()
	}
}

// Dials carry a child of the batch context, not the per-request one, so cancelling the batch tears
// them down -- and so does the closer, leaving none inside the outbound once the probe returns.
func outboundHTTPClient(ctx context.Context, outbound adapter.Outbound) (*http.Client, func()) {
	dialCtx, cancelDials := context.WithCancel(ctx)
	client, closeClient := dialerHTTPClient(func(_ context.Context, network, addr string) (net.Conn, error) {
		return outbound.DialContext(dialCtx, "tcp", metadata.ParseSocksaddr(addr))
	}, 0)
	return client, func() {
		cancelDials()
		closeClient()
	}
}

// Endpoint membership, not a type assertion: plain outbounds such as direct also satisfy adapter.Endpoint.
func tunnelEndpoints(i Box, tag string) []adapter.Endpoint {
	outbounds := i.Outbound()
	endpoints := service.FromContext[adapter.EndpointManager](i.Context())
	visited := make(map[string]bool)
	pending := []string{tag}
	var tunnels []adapter.Endpoint
	for len(pending) > 0 {
		tag = pending[len(pending)-1]
		pending = pending[:len(pending)-1]
		if visited[tag] {
			continue
		}
		visited[tag] = true
		outbound, found := outbounds.Outbound(tag)
		if !found {
			continue
		}
		if endpoint, isEndpoint := endpoints.Get(tag); isEndpoint {
			tunnels = append(tunnels, endpoint)
		}
		pending = append(pending, outbound.Dependencies()...)
	}
	return tunnels
}

type tunnelHandshake struct {
	updated  func() <-chan struct{}
	snapshot func() (state string, failure string)
}

// OpenVPN and OpenConnect refuse dials until their handshake completes; other tunnels hold them instead.
func handshakeOf(endpoint adapter.Endpoint) *tunnelHandshake {
	switch typed := endpoint.(type) {
	case adapter.OpenVPNEndpoint:
		return &tunnelHandshake{
			updated: typed.StatusUpdated,
			snapshot: func() (string, string) {
				status := typed.OpenVPNStatus()
				return status.State, status.Error
			},
		}
	case adapter.OpenConnectEndpoint:
		return &tunnelHandshake{
			updated: typed.StatusUpdated,
			snapshot: func() (string, string) {
				status := typed.OpenConnectStatus()
				return status.State, status.Error
			},
		}
	}
	return nil
}

// Both protocols spell their states the same way.
func (h *tunnelHandshake) await(ctx context.Context) error {
	for {
		// Subscribed before the snapshot, so a change in between still wakes us.
		updated := h.updated()
		state, failure := h.snapshot()
		switch state {
		case adapter.OpenVPNStateConnected:
			return nil
		case adapter.OpenVPNStateError:
			return errors.New(failure)
		case adapter.OpenVPNStateAuthPending:
			return errors.New("waiting for authentication")
		}
		select {
		case <-updated:
		case <-ctx.Done():
			return fmt.Errorf("handshake: %w", ctx.Err())
		}
	}
}

func awaitTunnels(ctx context.Context, i Box, tag string) error {
	ctx, cancel := context.WithTimeout(ctx, TunnelHandshakeTimeout)
	defer cancel()
	for _, endpoint := range tunnelEndpoints(i, tag) {
		handshake := handshakeOf(endpoint)
		if handshake == nil {
			continue
		}
		if err := handshake.await(ctx); err != nil {
			return fmt.Errorf("%s: %w", endpoint.Type(), err)
		}
	}
	return nil
}

func firstRequestTimeout(i Box, tag string, cold bool, timeout time.Duration) time.Duration {
	if !cold {
		return timeout
	}
	for _, endpoint := range tunnelEndpoints(i, tag) {
		// An awaited tunnel is already up; only the others can still be holding the first dial.
		if handshakeOf(endpoint) == nil {
			return timeout + TunnelStartupTimeout
		}
	}
	return timeout
}

func getNetDialer(dialer func(ctx context.Context, network string, destination metadata.Socksaddr) (net.Conn, error)) func(ctx context.Context, network string, address string) (net.Conn, error) {
	return func(ctx context.Context, network string, address string) (net.Conn, error) {
		return dialer(ctx, network, metadata.ParseSocksaddr(address))
	}
}

func getSpeedtestServer(ctx context.Context, dialer func(ctx context.Context, network string, address string) (net.Conn, error)) (*speedtest.Server, func(), error) {
	probe := &probeDialer{dial: dialer}
	// speedtest.New builds its own transport and writes it back here; the servers it returns keep using it.
	userConfig := &speedtest.UserConfig{
		DialContextFunc: probe.DialContext,
		PingMode:        speedtest.HTTP,
		MaxConnections:  8,
	}
	clt := speedtest.New(speedtest.WithUserConfig(userConfig))
	closeClient := func() {
		probe.Close()
		if userConfig.T != nil {
			userConfig.T.CloseIdleConnections()
		}
	}
	fetchCtx, cancel := context.WithTimeout(ctx, FetchServersTimeout)
	defer cancel()
	srv, err := clt.FetchServerListContext(fetchCtx)
	if err != nil {
		closeClient()
		return nil, nil, err
	}
	srv, err = srv.FindServer(nil)
	if err != nil {
		closeClient()
		return nil, nil, err
	}

	if srv.Len() == 0 {
		closeClient()
		return nil, nil, errors.New("no server found for speedTest")
	}

	return srv[0], closeClient, nil
}
