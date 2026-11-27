package testenv

import (
	"context"
	"errors"
	"fmt"
	"net"
	"testing"

	_ "ThroneCore/internal/distro/all"
	"ThroneCore/internal/probe"
)

func TestProbeTeardown(t *testing.T) {
	for _, stage := range []string{"after result", "during setup"} {
		t.Run(stage, func(t *testing.T) {
			ctx, cancel := context.WithCancel(t.Context())
			defer cancel()
			listener, err := net.Listen("tcp4", "127.0.0.1:0")
			if err != nil {
				t.Fatal(err)
			}
			addr := listener.Addr().(*net.TCPAddr)
			listener.Close()
			config := fmt.Sprintf(`{"inbounds":[{"listen":"127.0.0.1","port":%d,"protocol":"socks"}],"outbounds":[{"protocol":"freedom"}]}`, addr.Port)
			closed := false
			env, err := Prepare(ctx, Request{XrayFullConfigs: []string{config}}, nil, func(publish func(context.Context)) (probe.Box, func(), error) {
				publish(t.Context())
				conn, err := net.DialTCP("tcp4", nil, addr)
				if err != nil {
					return nil, nil, fmt.Errorf("sidecar did not start: %w", err)
				}
				conn.Close()
				if stage == "during setup" {
					cancel()
				}
				return nil, func() {
					closed = true
					if conn, err := net.DialTCP("tcp4", nil, addr); err == nil {
						conn.Close()
						t.Error("Xray still accepting connections when its resolver is closed")
					}
				}, nil
			})
			if stage == "during setup" {
				if !errors.Is(err, context.Canceled) || env != nil {
					t.Fatalf("got env %v, err %v", env, err)
				}
			} else {
				if err != nil {
					t.Fatal(err)
				}
				env.Close()
			}
			if !closed {
				t.Fatal("probe box was not closed")
			}
		})
	}
}
