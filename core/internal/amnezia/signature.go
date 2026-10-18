package amnezia

import (
	"bytes"
	"context"
	"crypto/tls"
	"encoding/hex"
	"net"
	"net/url"
	"strings"
	"sync"
	"time"

	"github.com/sagernet/quic-go"
	E "github.com/sagernet/sing/common/exceptions"
	"golang.org/x/net/idna"
)

// AmneziaWG signature slots I1-I5.
const maxSignaturePackets = 5

// The whole first flight leaves in one burst; the first retransmission waits a PTO of about 300 ms.
const flightSettleTime = 100 * time.Millisecond

// What the RPC and mobile callers allow a capture; it finishes offline in well under a second.
const QUICSignatureTimeout = 5 * time.Second

// QUICSignature captures, without sending anything, Chrome's first HTTP/3 flight to serverName as AmneziaWG <b> packets.
func QUICSignature(ctx context.Context, serverName string) ([]string, error) {
	serverName, err := normalizeServerName(serverName)
	if err != nil {
		return nil, err
	}

	conn := newFlightCapture()
	dialCtx, cancel := context.WithCancel(ctx)
	dialDone := make(chan struct{})
	go func() {
		defer close(dialDone)
		quicConn, _ := quic.Dial(dialCtx, conn, &net.UDPAddr{IP: net.IPv4(192, 0, 2, 1), Port: 443}, &tls.Config{
			ServerName: serverName,
			NextProtos: []string{"h3"},
		}, &quic.Config{ChromeParrot: true})
		if quicConn != nil {
			_ = quicConn.CloseWithError(0, "")
		}
	}()
	defer func() {
		cancel()
		conn.Close()
		<-dialDone
	}()

	var flight [][]byte
	var settle <-chan time.Time
	for len(flight) < maxSignaturePackets {
		select {
		case packet := <-conn.packets:
			flight = append(flight, packet)
			settle = time.After(flightSettleTime)
		case <-settle:
			return encodeFlight(flight), nil
		case <-dialDone:
			if len(flight) > 0 {
				return encodeFlight(flight), nil
			}
			return nil, E.New("QUIC handshake produced no packets")
		case <-ctx.Done():
			return nil, ctx.Err()
		}
	}
	return encodeFlight(flight), nil
}

func normalizeServerName(input string) (string, error) {
	host := strings.TrimSpace(input)
	if parsed, err := url.Parse(host); err == nil && parsed.Host != "" {
		host = parsed.Hostname()
	} else if name, _, err := net.SplitHostPort(host); err == nil {
		host = name
	}
	host = strings.TrimSuffix(host, ".")
	if host == "" {
		return "", E.New("missing domain")
	}
	if net.ParseIP(strings.Trim(host, "[]")) != nil {
		return "", E.New("an IP address cannot be used: browsers send no SNI for it")
	}
	ascii, err := idna.Lookup.ToASCII(host)
	if err != nil {
		return "", E.Cause(err, "invalid domain")
	}
	return ascii, nil
}

func encodeFlight(flight [][]byte) []string {
	packets := make([]string, 0, len(flight))
	for _, packet := range flight {
		packets = append(packets, "<b 0x"+hex.EncodeToString(packet)+">")
	}
	return packets
}

type flightCapture struct {
	packets   chan []byte
	closed    chan struct{}
	closeOnce sync.Once
}

func newFlightCapture() *flightCapture {
	return &flightCapture{
		packets: make(chan []byte, maxSignaturePackets),
		closed:  make(chan struct{}),
	}
}

func (c *flightCapture) WriteTo(p []byte, _ net.Addr) (int, error) {
	select {
	case c.packets <- bytes.Clone(p):
	default:
	}
	return len(p), nil
}

func (c *flightCapture) ReadFrom([]byte) (int, net.Addr, error) {
	<-c.closed
	return 0, nil, net.ErrClosed
}

func (c *flightCapture) Close() error {
	c.closeOnce.Do(func() { close(c.closed) })
	return nil
}

func (c *flightCapture) LocalAddr() net.Addr {
	return &net.UDPAddr{IP: net.IPv4zero}
}

func (c *flightCapture) SetDeadline(t time.Time) error {
	return c.SetReadDeadline(t)
}

// quic.Transport.Close unblocks its reader with a read deadline; the capture is single use, so any deadline ends it.
func (c *flightCapture) SetReadDeadline(t time.Time) error {
	if !t.IsZero() {
		c.Close()
	}
	return nil
}

func (c *flightCapture) SetWriteDeadline(time.Time) error { return nil }
