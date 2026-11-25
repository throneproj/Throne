package scan

import (
	"context"
	"crypto/rand"
	"encoding/binary"
	"encoding/hex"
	"errors"
	"fmt"
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
	root := dnsmessage.Question{Name: dnsmessage.MustNewName("."), Type: dnsmessage.TypeNS, Class: dnsmessage.ClassINET}
	return checkResolvers(ctx, egress, resolvers, timeout, root, func(dnsmessage.Header, *dnsmessage.Parser) error { return nil })
}

// CheckInternetDNS asks each resolver for an A record of a fresh name under com.: NXDOMAIN, or NOERROR without a LAN address, means up, and so does no resolver.
func CheckInternetDNS(ctx context.Context, egress Egress, resolvers []netip.AddrPort, timeout time.Duration) (bool, error) {
	var label [8]byte
	_, _ = rand.Read(label[:])
	// No cache holds a fresh name, and com.'s NSEC3 opt-out keeps resolvers from synthesizing its NXDOMAIN (RFC 8198).
	fresh := dnsmessage.Question{Name: dnsmessage.MustNewName(hex.EncodeToString(label[:]) + ".com."), Type: dnsmessage.TypeA, Class: dnsmessage.ClassINET}
	return checkResolvers(ctx, egress, resolvers, timeout, fresh, verifyInternetReply)
}

// A fresh name pointing at a LAN address is an offline router's catch-all; CGNAT and fake-ip (198.18.0.0/15) answers stay up.
func verifyInternetReply(header dnsmessage.Header, parser *dnsmessage.Parser) error {
	switch header.RCode {
	case dnsmessage.RCodeNameError:
		return nil
	case dnsmessage.RCodeSuccess:
	default:
		return fmt.Errorf("rcode %d", header.RCode)
	}
	if err := parser.SkipAllQuestions(); err != nil {
		return err
	}
	for {
		answer, err := parser.AnswerHeader()
		if errors.Is(err, dnsmessage.ErrSectionDone) {
			return nil
		}
		if err != nil {
			return err
		}
		var addr netip.Addr
		switch answer.Type {
		case dnsmessage.TypeA:
			resource, err := parser.AResource()
			if err != nil {
				return err
			}
			addr = netip.AddrFrom4(resource.A)
		case dnsmessage.TypeAAAA:
			resource, err := parser.AAAAResource()
			if err != nil {
				return err
			}
			addr = netip.AddrFrom16(resource.AAAA).Unmap()
		default:
			if err := parser.SkipAnswer(); err != nil {
				return err
			}
			continue
		}
		if addr.IsPrivate() || addr.IsLoopback() || addr.IsLinkLocalUnicast() || addr.IsUnspecified() {
			return fmt.Errorf("answered with %s", addr)
		}
	}
}

func checkResolvers(ctx context.Context, egress Egress, resolvers []netip.AddrPort, timeout time.Duration, question dnsmessage.Question, verify func(dnsmessage.Header, *dnsmessage.Parser) error) (bool, error) {
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
			err := queryResolver(ctx, egress, resolver, question, verify)
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

func queryResolver(ctx context.Context, egress Egress, resolver netip.AddrPort, question dnsmessage.Question, verify func(dnsmessage.Header, *dnsmessage.Parser) error) error {
	resolver = netip.AddrPortFrom(resolver.Addr().Unmap(), resolver.Port())
	conn, err := listenUDP(ctx, egress, resolver.Addr())
	if err != nil {
		return err
	}
	defer conn.Close()
	stopClose := context.AfterFunc(ctx, func() { _ = conn.Close() })
	defer stopClose()
	var random [2]byte
	_, _ = rand.Read(random[:])
	id := binary.BigEndian.Uint16(random[:])
	query, err := (&dnsmessage.Message{
		Header:    dnsmessage.Header{ID: id, RecursionDesired: true},
		Questions: []dnsmessage.Question{question},
	}).Pack()
	if err != nil {
		return err
	}
	if _, err = conn.WriteTo(query, net.UDPAddrFromAddrPort(resolver)); err != nil {
		return contextOr(ctx, err)
	}
	buffer := make([]byte, 4096)
	var parser dnsmessage.Parser
	for {
		n, from, err := conn.ReadFrom(buffer)
		if err != nil {
			return contextOr(ctx, err)
		}
		udpAddr, isUDP := from.(*net.UDPAddr)
		if !isUDP || udpAddr.AddrPort().Addr().Unmap() != resolver.Addr() {
			continue
		}
		header, err := parser.Start(buffer[:n])
		if err != nil || !header.Response || header.ID != id {
			continue
		}
		return verify(header, &parser)
	}
}
