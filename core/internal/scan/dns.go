package scan

import (
	"cmp"
	"context"
	"crypto/rand"
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"net"
	"net/netip"
	"os"
	"slices"
	"strings"
	"sync"
	"time"

	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/include"
	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing-box/protocol/dnstt"
	"github.com/sagernet/sing-box/protocol/masterdns"
	singjson "github.com/sagernet/sing/common/json"
	"github.com/sagernet/sing/common/json/badjson"
	M "github.com/sagernet/sing/common/metadata"
	"github.com/xtls/xray-core/transport/internet/finalmask/xdns"
	"golang.org/x/net/dns/dnsmessage"
)

const (
	DNSProtocolDNSTT     = "dnstt"
	DNSProtocolMasterDNS = "masterdns"
	DNSProtocolXDNS      = "xdns"
)

const (
	dnsStagePoll  = "poll"
	dnsStageQName = "qname"

	dnsReasonTimeout = "timeout"
	dnsReasonRcode   = "rcode"
	dnsReasonAnswer  = "answer"
	dnsReasonQName   = "qname"
)

const (
	dnsDefaultPort     = 53
	dnsTargetTimeout   = 10 * time.Second
	dnsExchangeTimeout = 3 * time.Second
	dnsBurstSize       = 8
	dnsBurstInterval   = 20 * time.Millisecond
	dnsShortQNameLen   = 60

	xdnsMaxLenLimit       = 255
	xdnsDefaultLabelLimit = 63
	xdnsMaxEDNS0          = 4096
	xdnsPollFrame         = 17
	xdnsCommandFrame      = 9
	xdnsPollCommand       = 8
	xdnsUnknownCommand    = 0
)

type DNSOptions struct {
	Enabled    bool
	Protocol   string
	ConfigJSON string
}

type dnsProbe func(ctx context.Context, egress Egress, server netip.AddrPort, result *Result) error

func newDNSProbe(options DNSOptions) (dnsProbe, error) {
	switch options.Protocol {
	case DNSProtocolDNSTT:
		return newDNSTTProbe(options.ConfigJSON)
	case DNSProtocolMasterDNS:
		return newMasterDNSProbe(options.ConfigJSON)
	case DNSProtocolXDNS:
		return newXDNSProbe(options.ConfigJSON)
	}
	return nil, fmt.Errorf("unsupported DNS tunnel protocol %q", options.Protocol)
}

func (r *probeRun) dnsTarget(ctx context.Context, target Target) Result {
	result := Result{Addr: target.Addr, Port: target.Port}
	port := target.Port
	if port == 0 {
		port = dnsDefaultPort
	}
	result.ProbePort = port
	display := DisplayTarget(target.Addr, port)
	r.session.emit(EventStart, PhaseDNS, display, 0, "")
	probeCtx, cancel := context.WithTimeout(ctx, dnsTargetTimeout)
	defer cancel()
	if err := r.plan.dns(probeCtx, r.egress, netip.AddrPortFrom(target.Addr.Unmap(), port), &result); err != nil {
		return r.fail(ctx, &result, PhaseDNS, display, err)
	}
	r.session.emit(EventOK, PhaseDNS, display, time.Duration(result.DNSMs)*time.Millisecond, "")
	result.Passed = true
	return result
}

type dnsStageError struct {
	stage string
	err   error
}

func (e *dnsStageError) Error() string {
	return e.stage + ": " + e.err.Error()
}

func (e *dnsStageError) Unwrap() error {
	return e.err
}

var errNoReply = errors.New("timeout")

func dnsReasonError(reason string, rcode int, shortNamesOnly bool) error {
	switch reason {
	case dnsReasonTimeout:
		return errNoReply
	case dnsReasonRcode:
		if rcode > 0 {
			return fmt.Errorf("rcode %d", rcode)
		}
		return errors.New("error reply")
	case dnsReasonAnswer:
		return errors.New("wrong answer")
	case dnsReasonQName:
		if shortNamesOnly {
			return errors.New("name too long, short names pass")
		}
		return errors.New("name too long")
	}
	return errors.New("failed")
}

// DNSMs 0 means the resolver never replied, so a reply faster than the clock resolution still counts.
func answeredMs(latency time.Duration) int32 {
	return int32(min(max(latency.Milliseconds(), 1), int64(dnsTargetTimeout/time.Millisecond)))
}

func listenUDP(ctx context.Context, egress Egress, destination netip.Addr) (net.PacketConn, error) {
	controlFunc, err := egress.Control()
	if err != nil {
		return nil, err
	}
	network := "udp4"
	if destination.Unmap().Is6() {
		network = "udp6"
	}
	return (&net.ListenConfig{Control: controlFunc}).ListenPacket(ctx, network, "")
}

type egressDialer struct {
	egress Egress
}

func (d egressDialer) DialContext(ctx context.Context, network string, destination M.Socksaddr) (net.Conn, error) {
	if !destination.IsIP() {
		return nil, fmt.Errorf("unexpected destination %s", destination)
	}
	controlFunc, err := d.egress.Control()
	if err != nil {
		return nil, err
	}
	return (&net.Dialer{Control: controlFunc}).DialContext(ctx, network, destination.Unwrap().String())
}

func (d egressDialer) ListenPacket(ctx context.Context, destination M.Socksaddr) (net.PacketConn, error) {
	return listenUDP(ctx, d.egress, destination.Addr)
}

type recordingDialer struct {
	egressDialer
	access sync.Mutex
	err    error
}

func (d *recordingDialer) DialContext(ctx context.Context, network string, destination M.Socksaddr) (net.Conn, error) {
	conn, err := d.egressDialer.DialContext(ctx, network, destination)
	d.record(ctx, err)
	if err != nil {
		return nil, err
	}
	return &recordingConn{Conn: conn, ctx: ctx, dialer: d}, nil
}

func (d *recordingDialer) ListenPacket(ctx context.Context, destination M.Socksaddr) (net.PacketConn, error) {
	conn, err := d.egressDialer.ListenPacket(ctx, destination)
	d.record(ctx, err)
	if err != nil {
		return nil, err
	}
	return &recordingPacketConn{PacketConn: conn, ctx: ctx, dialer: d}, nil
}

// A connected socket can still meet a missing route when it sends, and the probes call that a timeout too.
type recordingConn struct {
	net.Conn
	ctx    context.Context
	dialer *recordingDialer
}

func (c *recordingConn) Write(p []byte) (int, error) {
	n, err := c.Conn.Write(p)
	c.dialer.record(c.ctx, err)
	return n, err
}

// An unconnected socket meets a missing route only when it sends.
type recordingPacketConn struct {
	net.PacketConn
	ctx    context.Context
	dialer *recordingDialer
}

func (c *recordingPacketConn) WriteTo(p []byte, addr net.Addr) (int, error) {
	n, err := c.PacketConn.WriteTo(p, addr)
	c.dialer.record(c.ctx, err)
	return n, err
}

func (d *recordingDialer) record(ctx context.Context, err error) {
	if err == nil || ctx.Err() != nil {
		return
	}
	d.access.Lock()
	defer d.access.Unlock()
	if d.err == nil {
		d.err = err
	}
}

// The probes report a failed dial or send as a timeout; the recorded error tells a lost egress or a local failure apart.
func (d *recordingDialer) failure(reason string) error {
	d.access.Lock()
	defer d.access.Unlock()
	if d.err != nil && (reason == dnsReasonTimeout || errors.Is(d.err, ErrNoEgress)) {
		return d.err
	}
	return nil
}

var errDryRun = errors.New("dry run")

type dryRunDialer struct{}

func (dryRunDialer) DialContext(context.Context, string, M.Socksaddr) (net.Conn, error) {
	return nil, errDryRun
}

func (dryRunDialer) ListenPacket(context.Context, M.Socksaddr) (net.PacketConn, error) {
	return nil, errDryRun
}

var boxContext = sync.OnceValue(func() context.Context {
	return include.Context(context.Background())
})

func newDNSTTProbe(config string) (dnsProbe, error) {
	outbound, err := singjson.UnmarshalExtendedContext[option.Outbound](boxContext(), []byte(config))
	if err != nil {
		return nil, fmt.Errorf("decode dnstt outbound: %w", err)
	}
	options, isDNSTT := outbound.Options.(*option.DNSTTOutboundOptions)
	if outbound.Type != C.TypeDNSTT || !isDNSTT {
		return nil, fmt.Errorf("expected a dnstt outbound, got %q", outbound.Type)
	}
	tcpFallback := false
	for _, resolver := range options.Resolvers {
		tcpFallback = tcpFallback || resolver.TCPFallback
	}
	resolverAt := func(server netip.AddrPort) option.DNSTTResolverOptions {
		return option.DNSTTResolverOptions{
			Type:          C.DNSTypeUDP,
			ServerOptions: option.ServerOptions{Server: server.Addr().String(), ServerPort: server.Port()},
			TCPFallback:   tcpFallback,
		}
	}
	// Only option errors are returned, and they surface before any exchange.
	if _, err = dnstt.Probe(context.Background(), dryRunDialer{}, *options, resolverAt(netip.AddrPortFrom(netip.IPv4Unspecified(), dnsDefaultPort))); err != nil {
		return nil, fmt.Errorf("dnstt outbound: %w", err)
	}
	return func(ctx context.Context, egress Egress, server netip.AddrPort, result *Result) error {
		dialer := &recordingDialer{egressDialer: egressDialer{egress}}
		probed, err := dnstt.Probe(ctx, dialer, *options, resolverAt(server))
		if err != nil {
			return err
		}
		if probed.FirstReply > 0 {
			result.DNSMs = answeredMs(probed.FirstReply)
		}
		if probed.Passed {
			result.Sent, result.Answered = int32(probed.BurstSent), int32(probed.BurstAnswered)
			return nil
		}
		result.DNSStage, result.DNSReason, result.ShortNamesOnly = cmp.Or(probed.Stage, dnsStagePoll), probed.Reason, probed.ShortNamesOnly
		if err = dialer.failure(probed.Reason); err == nil {
			err = dnsReasonError(probed.Reason, probed.Rcode, probed.ShortNamesOnly)
		}
		return &dnsStageError{stage: result.DNSStage, err: err}
	}, nil
}

func newMasterDNSProbe(config string) (dnsProbe, error) {
	var outbound struct {
		Type string `json:"type"`
		Tag  string `json:"tag,omitempty"`
	}
	if err := json.Unmarshal([]byte(config), &outbound); err != nil {
		return nil, fmt.Errorf("decode masterdns outbound: %w", err)
	}
	if outbound.Type != C.TypeMasterDNS {
		return nil, fmt.Errorf("expected a masterdns outbound, got %q", outbound.Type)
	}
	var options option.MasterDNSOutboundOptions
	if err := badjson.UnmarshallExcludedContext(boxContext(), []byte(config), &outbound, &options); err != nil {
		return nil, fmt.Errorf("decode masterdns outbound: %w", err)
	}
	if _, err := masterdns.ProbeResolver(context.Background(), dryRunDialer{}, options, netip.AddrPortFrom(netip.AddrFrom4([4]byte{127, 0, 0, 1}), dnsDefaultPort)); err != nil {
		return nil, fmt.Errorf("masterdns outbound: %w", err)
	}
	return func(ctx context.Context, egress Egress, server netip.AddrPort, result *Result) error {
		dialer := &recordingDialer{egressDialer: egressDialer{egress}}
		probed, err := masterdns.ProbeResolver(ctx, dialer, options, server)
		if err != nil {
			return err
		}
		if probed.FirstReply > 0 {
			result.DNSMs = answeredMs(probed.FirstReply)
		}
		if probed.Passed {
			result.Sent, result.Answered = int32(probed.BurstSent), int32(probed.BurstAnswered)
			return nil
		}
		// Both minimum-size checks only show whether the tunnel answers at all, which is the poll stage of the others.
		result.DNSStage, result.DNSReason = dnsStagePoll, probed.Reason
		if err = dialer.failure(probed.Reason); err == nil {
			err = dnsReasonError(probed.Reason, 0, false)
		}
		return &dnsStageError{stage: cmp.Or(probed.FailedStage, dnsStagePoll), err: err}
	}, nil
}

type xdnsSettings struct {
	Domains []struct {
		Names      []string `json:"names"`
		LenLimit   int32    `json:"lenLimit"`
		LabelLimit int32    `json:"labelLimit"`
		EDNS0      int32    `json:"edns0"`
	} `json:"domains"`
}

func newXDNSProbe(config string) (dnsProbe, error) {
	var settings xdnsSettings
	if err := json.Unmarshal([]byte(config), &settings); err != nil {
		return nil, fmt.Errorf("decode xdns settings: %w", err)
	}
	for _, domain := range settings.Domains {
		for _, name := range domain.Names {
			if name = strings.TrimSpace(name); name == "" {
				continue
			}
			prober, err := newXDNSProber(name, domain.LenLimit, domain.LabelLimit, domain.EDNS0)
			if err != nil {
				return nil, err
			}
			return prober.probe, nil
		}
	}
	return nil, errors.New("the xdns settings have no domain")
}

var xdnsEncodedLen = func() [256]int {
	_, encoded := xdns.NewTable()
	return encoded
}()

type xdnsProber struct {
	domain     *xdns.Domain
	nameLength int
	labelLimit int
	edns0      uint16
	// Frame sizes in bytes: the largest the profile's lenLimit allows, and the largest a 60-byte name allows (0 = no check).
	required int
	short    int
}

func newXDNSProber(name string, lenLimit, labelLimit, edns0 int32) (*xdnsProber, error) {
	if lenLimit == 0 {
		lenLimit = xdnsMaxLenLimit
	}
	if labelLimit == 0 {
		labelLimit = xdnsDefaultLabelLimit
	}
	if edns0 < 0 || edns0 > xdnsMaxEDNS0 {
		return nil, fmt.Errorf("xdns domain %s: edns0 %d is out of range", name, edns0)
	}
	domain, err := xdns.NewDomain(name, int(lenLimit), int(labelLimit), []uint16{xdns.TypeTXT}, uint16(edns0))
	if err != nil {
		return nil, fmt.Errorf("xdns domain %s: %w", name, err)
	}
	p := &xdnsProber{
		domain: domain,
		// The bare domain as xdns sends it: punycode with the dots trimmed, which an IDN's raw string undercounts.
		nameLength: int(domain.Encode(nil).Length),
		labelLimit: int(labelLimit),
		edns0:      uint16(edns0),
	}
	limit := min(int(lenLimit), xdnsMaxLenLimit)
	for frame := xdnsCommandFrame; frame < len(xdnsEncodedLen) && p.wireLength(frame) <= limit; frame++ {
		p.required = frame
		if p.wireLength(frame) <= dnsShortQNameLen {
			p.short = frame
		}
	}
	if p.required < xdnsPollFrame {
		return nil, fmt.Errorf("xdns domain %s leaves no room for a poll", name)
	}
	if p.short == p.required {
		p.short = 0
	}
	return p, nil
}

// The QNAME wire length (Domain.lenMax) of a frame: labels of labelLimit base32 characters, then the domain.
func (p *xdnsProber) wireLength(frame int) int {
	encoded := xdnsEncodedLen[frame]
	return p.nameLength + 1 + encoded + (encoded+p.labelLimit-1)/p.labelLimit
}

func (p *xdnsProber) probe(ctx context.Context, egress Egress, server netip.AddrPort, result *Result) error {
	conn, err := listenUDP(ctx, egress, server.Addr())
	if err != nil {
		result.DNSStage, result.DNSReason = dnsStagePoll, dnsReasonTimeout
		return &dnsStageError{stage: dnsStagePoll, err: err}
	}
	defer conn.Close()
	stopClose := context.AfterFunc(ctx, func() { _ = conn.Close() })
	defer stopClose()
	exchanger := &xdnsExchanger{conn: conn, server: server, buffer: make([]byte, 4096)}
	var clientID [8]byte
	_, _ = rand.Read(clientID[:])
	clientID[0] = clientID[0]&0xFC | xdns.TypeMap[xdns.TypeTXT]

	// The server holds a lone poll for up to a second and answers it early when another query arrives, so two go out together.
	polls := []xdnsQuery{p.frameQuery(clientID, xdnsPollFrame, xdnsPollCommand), p.frameQuery(clientID, xdnsPollFrame, xdnsPollCommand)}
	replies, err := exchanger.exchange(ctx, polls, 0, xdnsReply.valid)
	first := firstXDNSReply(replies)
	if first.rtt > 0 {
		result.DNSMs = answeredMs(first.rtt)
	}
	if !slices.ContainsFunc(replies, xdnsReply.valid) {
		result.DNSStage, result.DNSReason = dnsStagePoll, dnsReasonTimeout
		switch {
		case first.rtt > 0 && first.rcode != dnsmessage.RCodeSuccess:
			result.DNSReason, err = dnsReasonRcode, dnsReasonError(dnsReasonRcode, int(first.rcode), false)
		case first.rtt > 0:
			result.DNSReason, err = dnsReasonAnswer, dnsReasonError(dnsReasonAnswer, 0, false)
		case err == nil:
			err = errNoReply
		}
		return &dnsStageError{stage: dnsStagePoll, err: err}
	}

	// The server answers an unknown command at once and queues nothing, so NOERROR proves the name was forwarded.
	if !p.forwarded(ctx, exchanger, clientID, p.required) {
		result.DNSStage, result.DNSReason = dnsStageQName, dnsReasonQName
		result.ShortNamesOnly = p.short > 0 && p.forwarded(ctx, exchanger, clientID, p.short)
		return &dnsStageError{stage: dnsStageQName, err: dnsReasonError(dnsReasonQName, 0, result.ShortNamesOnly)}
	}
	burst := make([]xdnsQuery, dnsBurstSize)
	for i := range burst {
		burst[i] = p.frameQuery(clientID, p.required, xdnsUnknownCommand)
	}
	replies, _ = exchanger.exchange(ctx, burst, dnsBurstInterval, nil)
	result.Sent = int32(len(replies))
	for _, reply := range replies {
		if reply.rtt > 0 && reply.rcode == dnsmessage.RCodeSuccess {
			result.Answered++
		}
	}
	return nil
}

func (p *xdnsProber) forwarded(ctx context.Context, exchanger *xdnsExchanger, clientID [8]byte, frame int) bool {
	replies, _ := exchanger.exchange(ctx, []xdnsQuery{p.frameQuery(clientID, frame, xdnsUnknownCommand)}, 0, nil)
	return len(replies) == 1 && replies[0].rtt > 0 && replies[0].rcode == dnsmessage.RCodeSuccess
}

func (p *xdnsProber) frameQuery(clientID [8]byte, size int, command byte) xdnsQuery {
	frame := make([]byte, size)
	_, _ = rand.Read(frame)
	copy(frame, clientID[:])
	frame[8] = command
	return p.query(frame)
}

type xdnsQuery struct {
	id     uint16
	name   dnsmessage.Name
	packet []byte
}

func (p *xdnsProber) query(frame []byte) xdnsQuery {
	var id [2]byte
	_, _ = rand.Read(id[:])
	query := xdnsQuery{id: binary.BigEndian.Uint16(id[:]), name: p.domain.Encode(frame)}
	message := dnsmessage.Message{
		Header:    dnsmessage.Header{ID: query.id, RecursionDesired: true},
		Questions: []dnsmessage.Question{{Name: query.name, Type: dnsmessage.TypeTXT, Class: dnsmessage.ClassINET}},
	}
	if p.edns0 > 0 {
		message.Additionals = []dnsmessage.Resource{{
			Header: dnsmessage.ResourceHeader{Name: dnsmessage.MustNewName("."), Type: dnsmessage.TypeOPT, Class: dnsmessage.Class(p.edns0)},
			Body:   &dnsmessage.OPTResource{},
		}}
	}
	query.packet, _ = message.Pack()
	return query
}

// rtt 0 = no reply.
type xdnsReply struct {
	rtt   time.Duration
	rcode dnsmessage.RCode
	txt   bool
}

func (r xdnsReply) valid() bool {
	return r.rtt > 0 && r.rcode == dnsmessage.RCodeSuccess && r.txt
}

func firstXDNSReply(replies []xdnsReply) xdnsReply {
	var first xdnsReply
	for _, reply := range replies {
		if reply.rtt > 0 && (first.rtt == 0 || reply.rtt < first.rtt) {
			first = reply
		}
	}
	return first
}

type xdnsExchanger struct {
	conn   net.PacketConn
	server netip.AddrPort
	buffer []byte
}

// exchange sends the queries interval apart and returns the replies to those sent, once stop accepts one, every one
// has a reply, or nothing came for dnsExchangeTimeout after the last.
func (x *xdnsExchanger) exchange(ctx context.Context, queries []xdnsQuery, interval time.Duration, stop func(xdnsReply) bool) ([]xdnsReply, error) {
	destination := net.UDPAddrFromAddrPort(x.server)
	replies := make([]xdnsReply, 0, len(queries))
	sentAt := make([]time.Time, 0, len(queries))
	begin := time.Now()
	var (
		waitUntil time.Time
		pending   int
	)
	for {
		if len(sentAt) < len(queries) && !time.Now().Before(begin.Add(time.Duration(len(sentAt))*interval)) {
			if _, err := x.conn.WriteTo(queries[len(sentAt)].packet, destination); err != nil {
				return replies, contextOr(ctx, err)
			}
			sentAt = append(sentAt, time.Now())
			replies = append(replies, xdnsReply{})
			pending++
			if len(sentAt) == len(queries) {
				waitUntil = time.Now().Add(dnsExchangeTimeout)
			}
			continue
		}
		if len(sentAt) == len(queries) && pending == 0 {
			return replies, nil
		}
		deadline := waitUntil
		if len(sentAt) < len(queries) {
			deadline = begin.Add(time.Duration(len(sentAt)) * interval)
		}
		if err := x.conn.SetReadDeadline(deadline); err != nil {
			return replies, contextOr(ctx, err)
		}
		n, from, err := x.conn.ReadFrom(x.buffer)
		if err != nil {
			if ctx.Err() == nil && errors.Is(err, os.ErrDeadlineExceeded) {
				if len(sentAt) < len(queries) {
					continue
				}
				return replies, nil
			}
			return replies, contextOr(ctx, err)
		}
		if udpAddr, isUDP := from.(*net.UDPAddr); !isUDP || udpAddr.AddrPort().Addr().Unmap() != x.server.Addr() || udpAddr.AddrPort().Port() != x.server.Port() {
			continue
		}
		for i := range replies {
			if replies[i].rtt > 0 {
				continue
			}
			reply, matched := parseXDNSReply(x.buffer[:n], queries[i])
			if !matched {
				continue
			}
			reply.rtt = max(time.Since(sentAt[i]), time.Nanosecond)
			replies[i] = reply
			pending--
			if stop != nil && stop(reply) {
				return replies, nil
			}
			break
		}
	}
}

func parseXDNSReply(packet []byte, query xdnsQuery) (xdnsReply, bool) {
	var parser dnsmessage.Parser
	header, err := parser.Start(packet)
	if err != nil || !header.Response || header.ID != query.id {
		return xdnsReply{}, false
	}
	question, err := parser.Question()
	if err != nil {
		// Some resolvers leave the question out of an error reply.
		return xdnsReply{rcode: header.RCode}, errors.Is(err, dnsmessage.ErrSectionDone) && header.RCode != dnsmessage.RCodeSuccess
	}
	if !sameDNSName(question.Name, query.name) {
		return xdnsReply{}, false
	}
	reply := xdnsReply{rcode: header.RCode}
	if parser.SkipAllQuestions() != nil {
		return reply, true
	}
	for {
		answer, err := parser.AnswerHeader()
		if err != nil {
			return reply, true
		}
		if answer.Type == dnsmessage.TypeTXT && answer.Class == dnsmessage.ClassINET && sameDNSName(answer.Name, query.name) {
			reply.txt = true
			return reply, true
		}
		if parser.SkipAnswer() != nil {
			return reply, true
		}
	}
}

// Resolvers may echo the name in another letter case (0x20 randomization).
func sameDNSName(a, b dnsmessage.Name) bool {
	if a.Length != b.Length {
		return false
	}
	for i := range int(a.Length) {
		if xdns.Lower(a.Data[i]) != xdns.Lower(b.Data[i]) {
			return false
		}
	}
	return true
}
