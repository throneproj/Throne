package scan

import (
	"context"
	"crypto/rand"
	"encoding/binary"
	"encoding/json"
	"errors"
	"fmt"
	"net"
	"net/netip"
	"strings"
	"sync"
	"time"

	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/include"
	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing-box/protocol/dnstt"
	"github.com/sagernet/sing-box/transport/masterdns"
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

	DNSLevelPoll      = "poll"
	DNSLevelQName     = "qname"
	DNSLevelHandshake = "handshake"
	DNSLevelQuick     = "quick"
	DNSLevelFull      = "full"
)

const (
	dnsDefaultPort    = 53
	defaultDNSTimeout = 5 * time.Second
	maxDNSTimeout     = 5 * time.Minute

	xdnsExchangeTimeout   = 5 * time.Second
	xdnsAttempts          = 2
	xdnsMaxLenLimit       = 255
	xdnsDefaultLabelLimit = 63
	xdnsMaxEDNS0          = 4096
	xdnsPollFrame         = 17
	xdnsPollCommand       = 8
	xdnsUnknownCommand    = 0
)

type DNSOptions struct {
	Enabled    bool
	Protocol   string
	ConfigJSON string
	Level      string
	Timeout    time.Duration
}

type dnsProbe func(ctx context.Context, egress Egress, server netip.AddrPort, result *Result) error

type dnsPlan struct {
	timeout time.Duration
	probe   dnsProbe
}

func newDNSPlan(options DNSOptions) (*dnsPlan, error) {
	plan := &dnsPlan{timeout: defaultDNSTimeout}
	if options.Timeout > 0 {
		plan.timeout = min(max(options.Timeout, minPhaseTimeout), maxDNSTimeout)
	}
	var err error
	switch options.Protocol {
	case DNSProtocolDNSTT:
		plan.probe, err = newDNSTTProbe(options.ConfigJSON, options.Level)
	case DNSProtocolMasterDNS:
		plan.probe, err = newMasterDNSProbe(options.ConfigJSON, options.Level)
	case DNSProtocolXDNS:
		plan.probe, err = newXDNSProbe(options.ConfigJSON, options.Level)
	default:
		err = fmt.Errorf("unsupported DNS tunnel protocol %q", options.Protocol)
	}
	if err != nil {
		return nil, err
	}
	return plan, nil
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
	probeCtx, cancel := context.WithTimeout(ctx, r.plan.dns.timeout)
	defer cancel()
	if err := r.plan.dns.probe(probeCtx, r.egress, netip.AddrPortFrom(target.Addr.Unmap(), port), &result); err != nil {
		if result.DNSStage != "" {
			err = &dnsStageError{stage: result.DNSStage, err: err}
		}
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

// DNSMs 0 means the resolver never answered, so an answer faster than the clock resolution still counts.
func answeredMs(latency time.Duration) int32 {
	return int32(min(max(latency.Milliseconds(), 1), int64(maxDNSTimeout/time.Millisecond)))
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

var dnsttLevels = map[string]dnstt.ProbeLevel{
	"":                dnstt.ProbeLevelPoll,
	DNSLevelPoll:      dnstt.ProbeLevelPoll,
	DNSLevelQName:     dnstt.ProbeLevelQName,
	DNSLevelHandshake: dnstt.ProbeLevelHandshake,
}

func newDNSTTProbe(config, levelName string) (dnsProbe, error) {
	level, known := dnsttLevels[levelName]
	if !known {
		return nil, fmt.Errorf("unknown dnstt probe level %q", levelName)
	}
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
	// Option errors surface before any exchange, with no stage.
	probed, err := dnstt.Probe(context.Background(), dryRunDialer{}, *options, resolverAt(netip.AddrPortFrom(netip.IPv4Unspecified(), dnsDefaultPort)), dnstt.ProbeLevelPoll)
	if err != nil && probed.Stage == "" {
		return nil, fmt.Errorf("dnstt outbound: %w", err)
	}
	return func(ctx context.Context, egress Egress, server netip.AddrPort, result *Result) error {
		probed, err := dnstt.Probe(ctx, egressDialer{egress}, *options, resolverAt(server), level)
		if probed.Rcode >= 0 {
			result.DNSMs = answeredMs(probed.Latency)
		}
		result.MaxQName = int32(probed.MaxQNameLen)
		if err != nil {
			result.DNSStage = probed.Stage
			return err
		}
		return nil
	}, nil
}

var masterDNSModes = map[string]masterdns.ProbeMode{
	"":            masterdns.ProbeModeQuick,
	DNSLevelQuick: masterdns.ProbeModeQuick,
	DNSLevelFull:  masterdns.ProbeModeFull,
}

var masterDNSStageErrors = map[string]error{
	masterdns.ProbeStageTimeout:  errors.New("no answer"),
	masterdns.ProbeStageDecode:   errors.New("undecodable answer"),
	masterdns.ProbeStageUpload:   errors.New("no size passed"),
	masterdns.ProbeStageDownload: errors.New("no size passed"),
}

func newMasterDNSProbe(config, levelName string) (dnsProbe, error) {
	mode, known := masterDNSModes[levelName]
	if !known {
		return nil, fmt.Errorf("unknown masterdns probe level %q", levelName)
	}
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
	if _, err := masterdns.ProbeResolver(context.Background(), dryRunDialer{}, options, netip.AddrPortFrom(netip.AddrFrom4([4]byte{127, 0, 0, 1}), dnsDefaultPort), mode); err != nil {
		return nil, fmt.Errorf("masterdns outbound: %w", err)
	}
	return func(ctx context.Context, egress Egress, server netip.AddrPort, result *Result) error {
		dialer := &recordingDialer{egressDialer: egressDialer{egress}}
		probed, err := masterdns.ProbeResolver(ctx, dialer, options, server, mode)
		if err != nil {
			return err
		}
		result.UploadMTU = int32(probed.UploadMTU)
		result.DownloadMTU = int32(probed.DownloadMTU)
		if probed.Passed {
			result.DNSMs = answeredMs(probed.Latency)
			return nil
		}
		result.DNSStage = probed.FailedStage
		// The probe reports an undialable resolver as a timeout; the dial error is what tells a lost egress apart.
		if dialErr := dialer.firstError(); dialErr != nil && probed.FailedStage == masterdns.ProbeStageTimeout {
			return dialErr
		}
		if err = masterDNSStageErrors[probed.FailedStage]; err == nil {
			err = errors.New("failed")
		}
		return err
	}, nil
}

type recordingDialer struct {
	egressDialer
	access sync.Mutex
	err    error
}

func (d *recordingDialer) DialContext(ctx context.Context, network string, destination M.Socksaddr) (net.Conn, error) {
	conn, err := d.egressDialer.DialContext(ctx, network, destination)
	if err != nil && ctx.Err() == nil {
		d.access.Lock()
		if d.err == nil {
			d.err = err
		}
		d.access.Unlock()
	}
	return conn, err
}

func (d *recordingDialer) firstError() error {
	d.access.Lock()
	defer d.access.Unlock()
	return d.err
}

type xdnsSettings struct {
	Domains []struct {
		Names      []string `json:"names"`
		LenLimit   int32    `json:"lenLimit"`
		LabelLimit int32    `json:"labelLimit"`
		EDNS0      int32    `json:"edns0"`
	} `json:"domains"`
}

func newXDNSProbe(config, levelName string) (dnsProbe, error) {
	var qname bool
	switch levelName {
	case "", DNSLevelPoll:
	case DNSLevelQName:
		qname = true
	default:
		return nil, fmt.Errorf("unknown xdns probe level %q", levelName)
	}
	var settings xdnsSettings
	if err := json.Unmarshal([]byte(config), &settings); err != nil {
		return nil, fmt.Errorf("decode xdns settings: %w", err)
	}
	for _, domain := range settings.Domains {
		for _, name := range domain.Names {
			if name = strings.TrimSpace(name); name == "" {
				continue
			}
			prober, err := newXDNSProber(name, domain.LenLimit, domain.LabelLimit, domain.EDNS0, qname)
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
	lenLimit   int
	labelLimit int
	edns0      uint16
	qname      bool
	// Frame sizes in bytes: the largest the profile's lenLimit allows, and the largest any name allows.
	required int
	largest  int
}

func newXDNSProber(name string, lenLimit, labelLimit, edns0 int32, qname bool) (*xdnsProber, error) {
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
		domain:     domain,
		nameLength: len(name) + 1,
		lenLimit:   int(lenLimit),
		labelLimit: int(labelLimit),
		edns0:      uint16(edns0),
		qname:      qname,
	}
	for frame := xdnsPollFrame; frame < len(xdnsEncodedLen) && p.wireLength(frame) <= xdnsMaxLenLimit; frame++ {
		p.largest = frame
		if p.wireLength(frame) <= p.lenLimit {
			p.required = frame
		}
	}
	if p.required < xdnsPollFrame {
		return nil, fmt.Errorf("xdns domain %s leaves no room for a poll", name)
	}
	return p, nil
}

// The QNAME wire length (Domain.lenMax) of a frame: labels of labelLimit base32 characters, then the domain.
func (p *xdnsProber) wireLength(frame int) int {
	encoded := xdnsEncodedLen[frame]
	return p.nameLength + 1 + encoded + (encoded+p.labelLimit-1)/p.labelLimit
}

// The largest lenLimit whose frames all fit within frame bytes.
func (p *xdnsProber) lenLimitFor(frame int) int {
	if frame >= p.largest {
		return xdnsMaxLenLimit
	}
	return p.wireLength(frame+1) - 1
}

func (p *xdnsProber) probe(ctx context.Context, egress Egress, server netip.AddrPort, result *Result) error {
	conn, err := listenUDP(ctx, egress, server.Addr())
	if err != nil {
		result.DNSStage = DNSLevelPoll
		return err
	}
	defer conn.Close()
	stopClose := context.AfterFunc(ctx, func() { _ = conn.Close() })
	defer stopClose()
	exchanger := &xdnsExchanger{conn: conn, server: server, buffer: make([]byte, 4096)}
	var clientID [8]byte
	_, _ = rand.Read(clientID[:])
	clientID[0] = clientID[0]&0xFC | xdns.TypeMap[xdns.TypeTXT]
	if err = p.poll(ctx, exchanger, clientID, result); err != nil {
		result.DNSStage = DNSLevelPoll
		return err
	}
	if !p.qname {
		return nil
	}
	if err = p.searchQName(ctx, exchanger, clientID, result); err != nil {
		result.DNSStage = DNSLevelQName
		return err
	}
	return nil
}

// The server holds a lone poll for up to a second and answers it early when another query arrives, so two go out together.
func (p *xdnsProber) poll(ctx context.Context, exchanger *xdnsExchanger, clientID [8]byte, result *Result) error {
	for attempt := 1; ; attempt++ {
		queries := make([]xdnsQuery, 2)
		for i := range queries {
			frame := make([]byte, xdnsPollFrame)
			_, _ = rand.Read(frame)
			copy(frame, clientID[:])
			frame[8] = xdnsPollCommand
			queries[i] = p.query(frame)
		}
		reply, latency, err := exchanger.exchange(ctx, queries...)
		if err == nil {
			result.DNSMs = answeredMs(latency)
			if reply.rcode != dnsmessage.RCodeSuccess {
				return fmt.Errorf("resolver answered with RCODE %d", reply.rcode)
			}
			if !reply.txt {
				return errors.New("resolver answered without the tunnel's TXT record")
			}
			return nil
		}
		if attempt == xdnsAttempts || ctx.Err() != nil {
			return err
		}
	}
}

func (p *xdnsProber) searchQName(ctx context.Context, exchanger *xdnsExchanger, clientID [8]byte, result *Result) error {
	// The server answers an unknown command at once and queues nothing, so NOERROR proves the name was forwarded.
	forwarded := func(size int) (bool, error) {
		for range xdnsAttempts {
			frame := make([]byte, size)
			_, _ = rand.Read(frame)
			copy(frame, clientID[:])
			frame[8] = xdnsUnknownCommand
			reply, _, err := exchanger.exchange(ctx, p.query(frame))
			if ctx.Err() != nil {
				return false, ctx.Err()
			}
			if err == nil && reply.rcode == dnsmessage.RCodeSuccess {
				return true, nil
			}
		}
		return false, nil
	}
	low, high := xdnsPollFrame, p.largest+1
	step := func(size int) error {
		ok, err := forwarded(size)
		if err != nil {
			return err
		}
		if ok {
			low = size
		} else {
			high = size
		}
		return nil
	}
	err := step(p.largest)
	if err == nil && low < p.required && p.required < high {
		err = step(p.required)
	}
	for err == nil && high-low > 1 {
		err = step(low + (high-low)/2)
	}
	result.MaxQName = int32(p.lenLimitFor(low))
	switch {
	case low >= p.required:
		return nil
	case high <= p.required:
		return fmt.Errorf("resolver forwards queries up to lenLimit %d, %d required", result.MaxQName, p.lenLimit)
	default:
		return err
	}
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

type xdnsReply struct {
	rcode dnsmessage.RCode
	txt   bool
}

type xdnsExchanger struct {
	conn   net.PacketConn
	server netip.AddrPort
	buffer []byte
}

func (x *xdnsExchanger) exchange(ctx context.Context, queries ...xdnsQuery) (xdnsReply, time.Duration, error) {
	deadline := time.Now().Add(xdnsExchangeTimeout)
	if ctxDeadline, ok := ctx.Deadline(); ok && ctxDeadline.Before(deadline) {
		deadline = ctxDeadline
	}
	if err := x.conn.SetReadDeadline(deadline); err != nil {
		return xdnsReply{}, 0, contextOr(ctx, err)
	}
	destination := net.UDPAddrFromAddrPort(x.server)
	begin := time.Now()
	for _, query := range queries {
		if _, err := x.conn.WriteTo(query.packet, destination); err != nil {
			return xdnsReply{}, 0, contextOr(ctx, err)
		}
	}
	for {
		n, from, err := x.conn.ReadFrom(x.buffer)
		if err != nil {
			return xdnsReply{}, 0, contextOr(ctx, err)
		}
		if udpAddr, isUDP := from.(*net.UDPAddr); !isUDP || udpAddr.AddrPort().Addr().Unmap() != x.server.Addr() || udpAddr.AddrPort().Port() != x.server.Port() {
			continue
		}
		for _, query := range queries {
			if reply, matched := parseXDNSReply(x.buffer[:n], query); matched {
				return reply, time.Since(begin), nil
			}
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
