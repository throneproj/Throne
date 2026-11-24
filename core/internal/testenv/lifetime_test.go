package testenv

import (
	"context"
	"fmt"
	"io"
	"net"
	"net/netip"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	_ "ThroneCore/internal/distro/all"

	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing/service"
	"golang.org/x/net/proxy"
)

type closeTestBox struct {
	adapter.DNSRouter
	ctx    context.Context
	closed atomic.Bool
	late   chan string
}

func (b *closeTestBox) Context() context.Context          { return b.ctx }
func (b *closeTestBox) Outbound() adapter.OutboundManager { return nil }
func (b *closeTestBox) Lookup(_ context.Context, domain string, _ adapter.DNSQueryOptions) ([]netip.Addr, error) {
	if b.closed.Load() {
		select {
		case b.late <- domain:
		default:
		}
	}
	return []netip.Addr{netip.MustParseAddr("127.0.0.1")}, nil
}

func TestProbeRetiresPendingVLESS(t *testing.T) {
	for _, tc := range []struct {
		transport string
		mux       bool
		testpre   int
	}{{"tcp", false, 0}, {"tcp", true, 0}, {"xhttp", true, 0}, {"tcp", false, 1}} {
		t.Run(fmt.Sprintf("%s/mux_%t/pre_%d", tc.transport, tc.mux, tc.testpre), func(t *testing.T) {
			target, err := net.Listen("tcp4", "127.0.0.1:0")
			if err != nil {
				t.Fatal(err)
			}
			defer target.Close()
			bridge, err := net.Listen("tcp4", "127.0.0.1:0")
			if err != nil {
				t.Fatal(err)
			}
			addr := bridge.Addr().String()
			port := bridge.Addr().(*net.TCPAddr).Port
			bridge.Close()
			config := fmt.Sprintf(`{
				"log":{"loglevel":"none"},
				"inbounds":[{"protocol":"socks","listen":"127.0.0.1","port":%d}],
				"outbounds":[{"protocol":"vless","tag":"proxy",
					"settings":{"address":"probe.invalid","port":%d,"testpre":%d,"id":"82767718-4ddc-413a-8511-b87bbcf1b09a","encryption":"none"},
					"mux":{"enabled":%t},"streamSettings":{"network":"%s","security":"tls","tlsSettings":{"serverName":"probe.invalid"}}}]
			}`, port, target.Addr().(*net.TCPAddr).Port, tc.testpre, tc.mux, tc.transport)
			b := &closeTestBox{late: make(chan string, 1)}
			b.ctx = service.ContextWith[adapter.DNSRouter](context.Background(), b)
			env, err := Prepare(context.Background(), Request{XrayFullConfigs: []string{config}, XrayDNSStrategy: "ForceIPv4"}, nil, func(pub func(context.Context)) (*closeTestBox, func(), error) {
				pub(b.ctx)
				return b, func() { b.closed.Store(true) }, nil
			})
			if err != nil {
				t.Fatal(err)
			}
			closeEnv := sync.OnceFunc(env.Close)
			defer closeEnv()
			client, err := proxy.SOCKS5("tcp", addr, nil, &net.Dialer{Timeout: time.Second})
			if err != nil {
				t.Fatal(err)
			}
			ctx, cancel := context.WithTimeout(context.Background(), 4*time.Second)
			defer cancel()
			conn, err := client.(proxy.ContextDialer).DialContext(ctx, "tcp", "127.0.0.1:80")
			if err != nil {
				t.Fatal(err)
			}
			defer conn.Close()
			if _, err = io.WriteString(conn, "GET / HTTP/1.0\r\n\r\n"); err != nil {
				t.Fatal(err)
			}
			target.(*net.TCPListener).SetDeadline(time.Now().Add(4 * time.Second))
			peer, err := target.Accept()
			if err != nil {
				t.Fatal(err)
			}
			defer peer.Close()
			// Wait for the actual handshake to reach the peer before tearing down the batch.
			peer.SetReadDeadline(time.Now().Add(3 * time.Second))
			var first [1]byte
			if _, err := io.ReadFull(peer, first[:]); err != nil {
				t.Fatal(err)
			}
			closeEnv()
			peer.SetReadDeadline(time.Now().Add(time.Second))
			if _, err := io.Copy(io.Discard, peer); err != nil {
				if e, ok := err.(net.Error); ok && e.Timeout() {
					t.Fatal("stalled handshake still has an open socket after Env.Close")
				}
			}
			select {
			case domain := <-b.late:
				t.Fatalf("outbound looked up %s after Env.Close returned", domain)
			case <-time.After(700 * time.Millisecond):
			}
			target.(*net.TCPListener).SetDeadline(time.Now().Add(25 * time.Millisecond))
			if late, err := target.Accept(); err == nil {
				late.Close()
				t.Fatal("outbound redialed after Env.Close returned")
			}
		})
	}
}

func TestStopDuringPrepare(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	box := &closeTestBox{ctx: context.Background()}
	env, err := Prepare(ctx, Request{}, nil, func(publish func(context.Context)) (*closeTestBox, func(), error) {
		publish(box.ctx)
		cancel()
		return box, func() { box.closed.Store(true) }, nil
	})
	if env != nil || err != context.Canceled || !box.closed.Load() {
		t.Fatalf("env=%v, err=%v, closed=%v", env, err, box.closed.Load())
	}
}
