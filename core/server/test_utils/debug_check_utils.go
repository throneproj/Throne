package test_utils

import (
	"ThroneCore/internal/boxbox"
	"context"
	"encoding/binary"
	"fmt"
	"io"
	"math/rand"
	"net"
	"net/http"
	"strings"
	"time"

	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing/common/metadata"
	"github.com/sagernet/sing/service"
)

const debugCheckDefaultTimeout = 30 * time.Second
const debugTCPTestURL = "http://cachefly.cachefly.net/1mb.test"
const debugIPifyURL = "https://api.ipify.org"

// DebugCheckResult holds all results for one profile check.
type DebugCheckResult struct {
	RealIP    string
	ProxyIP   string
	IPChanged bool
	TCPBytes  int64
	TCPOk     bool
	UDPOk     bool
	Error     error
	TCPError  error
	UDPError  error
}

// getDirectIP fetches the public IP address via a direct HTTP connection (no proxy outbound).
func getDirectIP(timeout time.Duration) (string, error) {
	client := &http.Client{Timeout: timeout}
	req, err := http.NewRequest("GET", debugIPifyURL, nil)
	if err != nil {
		return "", err
	}
	resp, err := client.Do(req)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()
	data, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}
	return strings.TrimSpace(string(data)), nil
}

// getProxyIP fetches the public IP address as seen through the given outbound.
func getProxyIP(ctx context.Context, outbound adapter.Outbound, timeout time.Duration) (string, error) {
	dialCtx, cancel := context.WithTimeout(ctx, timeout)
	defer cancel()
	client := &http.Client{
		Transport: &http.Transport{
			DialContext: func(c context.Context, network, addr string) (net.Conn, error) {
				return outbound.DialContext(dialCtx, "tcp", metadata.ParseSocksaddr(addr))
			},
		},
		Timeout: timeout,
	}
	req, err := http.NewRequestWithContext(dialCtx, "GET", debugIPifyURL, nil)
	if err != nil {
		return "", err
	}
	resp, err := client.Do(req)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()
	data, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}
	return strings.TrimSpace(string(data)), nil
}

// testTCPMegabyte downloads at least 1 MB through the given outbound.
// Returns the number of bytes downloaded and any error.
func testTCPMegabyte(ctx context.Context, outbound adapter.Outbound, timeout time.Duration) (int64, error) {
	dialCtx, cancel := context.WithTimeout(ctx, timeout)
	defer cancel()
	client := &http.Client{
		Transport: &http.Transport{
			DialContext: func(c context.Context, network, addr string) (net.Conn, error) {
				return outbound.DialContext(dialCtx, "tcp", metadata.ParseSocksaddr(addr))
			},
		},
		Timeout: timeout,
	}
	req, err := http.NewRequestWithContext(dialCtx, "GET", debugTCPTestURL, nil)
	if err != nil {
		return 0, err
	}
	resp, err := client.Do(req)
	if err != nil {
		return 0, err
	}
	defer resp.Body.Close()

	n, err := io.Copy(io.Discard, resp.Body)
	if err != nil {
		return n, err
	}
	const oneMB = int64(1024 * 1024)
	if n < oneMB {
		return n, fmt.Errorf("only downloaded %d bytes (need at least 1 MB)", n)
	}
	return n, nil
}

// buildDNSQuery builds a minimal DNS A-record query for the given domain.
func buildDNSQuery(domain string) []byte {
	txID := uint16(rand.Uint32())
	buf := make([]byte, 0, 32)

	// Header: ID, Flags (standard recursive query), QDCOUNT=1, rest 0
	buf = binary.BigEndian.AppendUint16(buf, txID)
	buf = binary.BigEndian.AppendUint16(buf, 0x0100) // QR=0 OPCODE=0 RD=1
	buf = binary.BigEndian.AppendUint16(buf, 1)      // QDCOUNT
	buf = binary.BigEndian.AppendUint16(buf, 0)      // ANCOUNT
	buf = binary.BigEndian.AppendUint16(buf, 0)      // NSCOUNT
	buf = binary.BigEndian.AppendUint16(buf, 0)      // ARCOUNT

	// Question: domain name labels
	for _, label := range strings.Split(domain, ".") {
		buf = append(buf, byte(len(label)))
		buf = append(buf, []byte(label)...)
	}
	buf = append(buf, 0)             // root label
	buf = append(buf, 0x00, 0x01)    // QTYPE A
	buf = append(buf, 0x00, 0x01)    // QCLASS IN

	return buf
}

// testUDPYoutube verifies UDP routing by sending a DNS query for youtube.com to 8.8.8.8:53
// through the proxy outbound. A valid response confirms the proxy forwards UDP.
func testUDPYoutube(ctx context.Context, outbound adapter.Outbound, timeout time.Duration) error {
	dialCtx, cancel := context.WithTimeout(ctx, timeout)
	defer cancel()

	conn, err := outbound.DialContext(dialCtx, "udp", metadata.ParseSocksaddr("8.8.8.8:53"))
	if err != nil {
		return fmt.Errorf("UDP dial failed: %w", err)
	}
	defer conn.Close()

	conn.SetDeadline(time.Now().Add(timeout))

	query := buildDNSQuery("youtube.com")
	if _, err = conn.Write(query); err != nil {
		return fmt.Errorf("UDP write failed: %w", err)
	}

	buf := make([]byte, 512)
	n, err := conn.Read(buf)
	if err != nil {
		return fmt.Errorf("UDP read failed: %w", err)
	}
	if n < 12 {
		return fmt.Errorf("DNS response too short (%d bytes)", n)
	}
	return nil
}

// RunDebugCheck performs all three checks (IP change, TCP 1MB, UDP YouTube) for one outbound.
func RunDebugCheck(ctx context.Context, instance *boxbox.Box, outboundTag string, useDefault bool, timeout time.Duration) *DebugCheckResult {
	result := &DebugCheckResult{}

	if timeout <= 0 {
		timeout = debugCheckDefaultTimeout
	}

	// Resolve outbound
	outbounds := service.FromContext[adapter.OutboundManager](instance.Context())
	var outbound adapter.Outbound
	if useDefault {
		outbound = instance.Outbound().Default()
	} else {
		var found bool
		outbound, found = outbounds.Outbound(outboundTag)
		if !found {
			result.Error = fmt.Errorf("outbound %q not found", outboundTag)
			return result
		}
	}

	// 1. Real IP (direct, no proxy)
	realIP, err := getDirectIP(timeout / 3)
	if err != nil {
		realIP = fmt.Sprintf("(error: %v)", err)
	}
	result.RealIP = realIP

	// 2. Proxy IP via ipify
	proxyIP, err := getProxyIP(ctx, outbound, timeout/3)
	if err != nil {
		result.Error = fmt.Errorf("IP check failed: %w", err)
		result.ProxyIP = ""
	} else {
		result.ProxyIP = proxyIP
		result.IPChanged = realIP != proxyIP && proxyIP != ""
	}

	// 3. TCP 1 MB download
	bytes, err := testTCPMegabyte(ctx, outbound, timeout)
	result.TCPBytes = bytes
	if err != nil {
		result.TCPError = err
		result.TCPOk = false
	} else {
		result.TCPOk = true
	}

	// 4. UDP: DNS query for youtube.com via 8.8.8.8:53
	err = testUDPYoutube(ctx, outbound, timeout/3)
	if err != nil {
		result.UDPError = err
		result.UDPOk = false
	} else {
		result.UDPOk = true
	}

	return result
}
