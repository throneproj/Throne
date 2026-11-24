package scan

import (
	"context"
	"crypto/rand"
	"encoding/binary"
	"errors"
	"net"
	"net/netip"
	"time"

	"golang.org/x/net/dns/dnsmessage"
)

func CheckNetwork(ctx context.Context, egress Egress, targets []string, timeout time.Duration) (bool, error) {
	if len(targets) == 0 {
		return false, errors.New("no network check targets")
	}
	controlFunc, err := egress.Control()
	if err != nil {
		return false, err
	}
	ctx, cancel := context.WithTimeout(ctx, clampTimeout(timeout, 3*time.Second))
	defer cancel()
	failures := make(chan error, len(targets))
	for _, target := range targets {
		go func() {
			dialer := net.Dialer{Control: controlFunc}
			conn, dialErr := dialer.DialContext(ctx, "tcp", target)
			if dialErr == nil {
				_ = conn.Close()
				cancel()
			}
			failures <- dialErr
		}()
	}
	var firstErr error
	for range targets {
		if dialErr := <-failures; dialErr == nil {
			return true, nil
		} else if firstErr == nil {
			firstErr = dialErr
		}
	}
	return false, errors.New("no network check target answered: " + errorText(firstErr))
}

// CheckNetworkDNS asks each resolver for the root's NS records: any reply, whatever its rcode, means up, and so does no resolver.
func CheckNetworkDNS(ctx context.Context, egress Egress, resolvers []netip.AddrPort, timeout time.Duration) (bool, error) {
	if len(resolvers) == 0 {
		return true, nil
	}
	if _, err := egress.Control(); err != nil {
		return false, err
	}
	ctx, cancel := context.WithTimeout(ctx, clampTimeout(timeout, 3*time.Second))
	defer cancel()
	failures := make(chan error, len(resolvers))
	for _, resolver := range resolvers {
		go func() {
			err := queryRoot(ctx, egress, resolver)
			if err == nil {
				cancel()
			}
			failures <- err
		}()
	}
	var firstErr error
	for range resolvers {
		if err := <-failures; err == nil {
			return true, nil
		} else if firstErr == nil {
			firstErr = err
		}
	}
	return false, errors.New("no system resolver answered: " + errorText(firstErr))
}

func queryRoot(ctx context.Context, egress Egress, resolver netip.AddrPort) error {
	resolver = netip.AddrPortFrom(resolver.Addr().Unmap(), resolver.Port())
	conn, err := listenUDP(ctx, egress, resolver.Addr())
	if err != nil {
		return err
	}
	defer conn.Close()
	stopClose := context.AfterFunc(ctx, func() { _ = conn.Close() })
	defer stopClose()
	var id [2]byte
	_, _ = rand.Read(id[:])
	query, err := (&dnsmessage.Message{
		Header:    dnsmessage.Header{ID: binary.BigEndian.Uint16(id[:]), RecursionDesired: true},
		Questions: []dnsmessage.Question{{Name: dnsmessage.MustNewName("."), Type: dnsmessage.TypeNS, Class: dnsmessage.ClassINET}},
	}).Pack()
	if err != nil {
		return err
	}
	if _, err = conn.WriteTo(query, net.UDPAddrFromAddrPort(resolver)); err != nil {
		return contextOr(ctx, err)
	}
	buffer := make([]byte, 4096)
	for {
		n, from, err := conn.ReadFrom(buffer)
		if err != nil {
			return contextOr(ctx, err)
		}
		udpAddr, isUDP := from.(*net.UDPAddr)
		if isUDP && udpAddr.AddrPort().Addr().Unmap() == resolver.Addr() && n >= 12 && buffer[2]&0x80 != 0 && [2]byte(buffer[:2]) == id {
			return nil
		}
	}
}
