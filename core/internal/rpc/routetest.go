package rpc

import (
	"context"
	"log"
	"net/netip"
	"path/filepath"
	"runtime"
	"slices"
	"strconv"
	"strings"
	"time"

	"ThroneCore/gen"
	"ThroneCore/internal/boxbox"
	"ThroneCore/internal/boxmain"
	"ThroneCore/internal/proclist"

	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/common/sniff"
	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/option"
	R "github.com/sagernet/sing-box/route/rule"
	E "github.com/sagernet/sing/common/exceptions"
	F "github.com/sagernet/sing/common/format"
	"github.com/sagernet/sing/common/json/badoption"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/service"
)

const (
	routeTestTimeout            = 30 * time.Second
	routeTestStubConnectTimeout = 10 * time.Second
)

// A stripped box keeps the profile's cache.db, which bbolt opens exclusively.
var routeTestBoxSlot = make(chan struct{}, 1)

var (
	routeTestStreamSniffable = []string{C.ProtocolTLS, C.ProtocolHTTP, C.ProtocolDNS, C.ProtocolBitTorrent, C.ProtocolSSH, C.ProtocolRDP}
	routeTestPacketSniffable = []string{C.ProtocolDNS, C.ProtocolQUIC, C.ProtocolSTUN, C.ProtocolBitTorrent, C.ProtocolDTLS, C.ProtocolNTP}
	routeTestDomainSniffers  = []string{C.ProtocolTLS, C.ProtocolHTTP, C.ProtocolQUIC}
)

func (s *server) TestRoute(ctx context.Context, in *gen.TestRouteRequest) (*gen.TestRouteResponse, error) {
	ctx, cancel := context.WithTimeout(ctx, routeTestTimeout)
	defer cancel()
	request, err := parseRouteTestRequest(in)
	if err != nil {
		return &gen.TestRouteResponse{Error: To(err.Error())}, nil
	}
	if owner := request.metadata.ProcessInfo; owner != nil {
		owner.ProcessPaths = routeTestProcessPaths(owner.ProcessPaths[0])
	}
	var env *routeTestEnv
	if in.GetConfig() != "" {
		env, err = startRouteTestBox(ctx, in.GetConfig())
	} else {
		env, err = runningRouteTestEnv()
	}
	if err != nil {
		return &gen.TestRouteResponse{Error: To(err.Error())}, nil
	}
	defer env.close()
	return env.test(ctx, request), nil
}

type routeTestRequest struct {
	metadata adapter.InboundContext
	// What a sniff rule finds; empty for nothing.
	protocol string
	// What a TLS, HTTP or QUIC sniff reads from the connection.
	domain string
}

func parseRouteTestRequest(in *gen.TestRouteRequest) (routeTestRequest, error) {
	network := strings.ToLower(strings.TrimSpace(in.GetNetwork()))
	switch network {
	case "":
		network = N.NetworkTCP
	case N.NetworkTCP, N.NetworkUDP:
	default:
		return routeTestRequest{}, E.New("unknown network: ", in.GetNetwork())
	}
	if in.GetPort() < 1 || in.GetPort() > 65535 {
		return routeTestRequest{}, E.New("port out of range: ", in.GetPort())
	}
	port := uint16(in.GetPort())

	domain := strings.TrimSuffix(strings.ToLower(strings.TrimSpace(in.GetDomain())), ".")
	ipText := strings.TrimSpace(in.GetIp())
	if _, err := parseRouteTestIP(domain); err == nil {
		if ipText == "" {
			ipText = domain
		}
		domain = ""
	}
	if domain != "" && !M.IsDomainName(domain) {
		return routeTestRequest{}, E.New("invalid domain: ", in.GetDomain())
	}
	destination := M.Socksaddr{Fqdn: domain, Port: port}
	if ipText != "" {
		ip, err := parseRouteTestIP(ipText)
		if err != nil {
			return routeTestRequest{}, E.New("invalid IP address: ", ipText)
		}
		destination = M.Socksaddr{Addr: ip, Port: port}
	}

	metadata := adapter.InboundContext{
		Network:     network,
		Source:      M.Socksaddr{Addr: netip.AddrFrom4([4]byte{127, 0, 0, 1})},
		Destination: destination,
	}
	if process := strings.TrimSpace(in.GetProcess()); process != "" {
		metadata.ProcessInfo = &adapter.ConnectionOwner{ProcessPaths: []string{process}}
	}

	protocol := strings.ToLower(strings.TrimSpace(in.GetProtocol()))
	switch protocol {
	case "", "auto":
		protocol = guessRouteTestProtocol(network, port)
	case "none":
		protocol = ""
	}
	return routeTestRequest{metadata: metadata, protocol: protocol, domain: domain}, nil
}

// sing-box's Windows searcher reports a svchost-hosted service as its DLL followed by the svchost.exe hosting it.
func routeTestProcessPaths(process string) []string {
	if runtime.GOOS != "windows" || !strings.EqualFold(filepath.Ext(process), ".dll") {
		return []string{process}
	}
	entries, _ := proclist.List()
	var hostPID uint32
	for _, entry := range entries {
		if entry.Service != "" && (entry.Path == process || entry.Name == process) {
			hostPID = entry.PID
			break
		}
	}
	host := ""
	for _, entry := range entries {
		if entry.Service != "" || entry.Path == "" || !strings.EqualFold(entry.Name, "svchost.exe") {
			continue
		}
		if entry.PID == hostPID {
			host = entry.Path
			break
		}
		if host == "" {
			host = entry.Path
		}
	}
	if host == "" {
		return []string{process}
	}
	return []string{process, host}
}

func parseRouteTestIP(text string) (netip.Addr, error) {
	addr, err := netip.ParseAddr(strings.TrimSuffix(strings.TrimPrefix(text, "["), "]"))
	return addr.Unmap(), err
}

func guessRouteTestProtocol(network string, port uint16) string {
	if network == N.NetworkUDP {
		switch port {
		case 443:
			return C.ProtocolQUIC
		case 53:
			return C.ProtocolDNS
		case 123:
			return C.ProtocolNTP
		case 3478, 19302:
			return C.ProtocolSTUN
		}
		return ""
	}
	switch port {
	case 443:
		return C.ProtocolTLS
	case 80:
		return C.ProtocolHTTP
	case 22:
		return C.ProtocolSSH
	case 3389:
		return C.ProtocolRDP
	case 53:
		return C.ProtocolDNS
	}
	return ""
}

type routeTestInbound struct {
	tag  string
	kind string
}

func pickRouteTestInbound(inbounds []routeTestInbound) routeTestInbound {
	for _, kind := range []string{C.TypeTun, C.TypeMixed} {
		for _, inbound := range inbounds {
			if inbound.kind == kind {
				return inbound
			}
		}
	}
	if len(inbounds) > 0 {
		return inbounds[0]
	}
	return routeTestInbound{}
}

type routeTestRules struct {
	rules []adapter.Rule
	// The options each rule was built from; nil when unknown.
	options []option.Rule
	// The config index of each rule; nil when every rule kept its place.
	indexes []int
	ruleSet func(tag string) (adapter.RuleSet, bool)
}

func (r *routeTestRules) index(i int) int {
	if i < len(r.indexes) {
		return r.indexes[i]
	}
	return i
}

func (r *routeTestRules) matched(i int, metadata adapter.InboundContext) string {
	if i >= len(r.options) {
		return ""
	}
	return matchedEntries(ruleEntries(r.options[i], r.ruleSet), metadata)
}

type routeTestResolver func(ctx context.Context, metadata *adapter.InboundContext, action *R.RuleActionResolve) ([]netip.Addr, error)

type routeTestEnv struct {
	box      *boxbox.Box
	rules    routeTestRules
	inbound  routeTestInbound
	skipped  []int32
	stripped bool
	close    func()
}

func runningRouteTestEnv() (*routeTestEnv, error) {
	box := currentBox()
	if box == nil {
		return nil, errInstanceNotRunning
	}
	var inbounds []routeTestInbound
	for _, inbound := range box.Inbound().Inbounds() {
		inbounds = append(inbounds, routeTestInbound{inbound.Tag(), inbound.Type()})
	}
	rules := box.Router().Rules()
	options := connRules.routeRules(box)
	if len(options) != len(rules) {
		options = nil
	}
	return &routeTestEnv{
		box:     box,
		rules:   routeTestRules{rules: rules, options: options, ruleSet: box.Router().RuleSet},
		inbound: pickRouteTestInbound(inbounds),
		close:   func() {},
	}, nil
}

func startRouteTestBox(ctx context.Context, config string) (*routeTestEnv, error) {
	select {
	case routeTestBoxSlot <- struct{}{}:
	case <-ctx.Done():
		return nil, E.New("another route test is still running")
	}
	keepCacheFile := currentBox() == nil
	type created struct {
		box    *boxbox.Box
		cancel context.CancelFunc
		err    error
	}
	done := make(chan created, 1)
	var stripped strippedRouteConfig
	go func() {
		var c created
		// Off the dispatcher's goroutine, so its recover no longer covers a panic in sing-box.
		defer func() {
			if r := recover(); r != nil {
				c = created{err: E.New("core panic: ", r)}
			}
			done <- c
		}()
		c.box, c.cancel, c.err = boxmain.Create([]byte(config), nil, func(options *option.Options) {
			stripped = stripRouteTestConfig(options, keepCacheFile)
			applyAutoRedirectMark(options, autoRedirectMark.Load())
		})
	}()
	release := func(c created) {
		if c.err == nil {
			c.box.CloseWithTimeout(c.cancel, 2*time.Second, log.Println, true)
		}
		<-routeTestBoxSlot
	}
	select {
	case c := <-done:
		if c.err != nil {
			release(c)
			return nil, c.err
		}
		return &routeTestEnv{
			box:      c.box,
			rules:    routeTestRules{rules: c.box.Router().Rules(), options: stripped.rules, indexes: stripped.indexes, ruleSet: c.box.Router().RuleSet},
			inbound:  stripped.inbound,
			skipped:  stripped.skipped,
			stripped: true,
			close:    func() { go release(c) },
		}, nil
	case <-ctx.Done():
		go func() { release(<-done) }()
		return nil, E.New("the config did not start in ", routeTestTimeout)
	}
}

type strippedRouteConfig struct {
	inbound routeTestInbound
	// The kept route rules, aligned with the stripped router's.
	rules   []option.Rule
	indexes []int
	skipped []int32
}

// stripRouteTestConfig keeps only what route matching reads: nothing left listens, dials at start or names a DNS server.
func stripRouteTestConfig(options *option.Options, keepCacheFile bool) strippedRouteConfig {
	var stripped strippedRouteConfig
	inbounds := make([]routeTestInbound, 0, len(options.Inbounds))
	for i, inbound := range options.Inbounds {
		inbounds = append(inbounds, routeTestInbound{routeTestTag(inbound.Tag, i), inbound.Type})
	}
	stripped.inbound = pickRouteTestInbound(inbounds)
	options.Inbounds = nil

	stubs := make([]option.Outbound, 0, len(options.Outbounds)+len(options.Endpoints))
	seen := make(map[string]bool)
	addStub := func(tag string) {
		if seen[tag] {
			return
		}
		seen[tag] = true
		stubs = append(stubs, option.Outbound{Type: C.TypeDirect, Tag: tag, Options: &option.DirectOutboundOptions{
			// A direct outbound with default dialer options counts as empty, and a detour to it fails.
			DialerOptions: option.DialerOptions{AbstractDialerOptions: option.AbstractDialerOptions{
				ConnectTimeout: badoption.Duration(routeTestStubConnectTimeout),
			}},
		}})
	}
	for i, outbound := range options.Outbounds {
		addStub(routeTestTag(outbound.Tag, i))
	}
	for i, endpoint := range options.Endpoints {
		addStub(routeTestTag(endpoint.Tag, i))
	}
	options.Outbounds, options.Endpoints = stubs, nil

	options.DNS, options.NTP, options.Services, options.CertificateProviders = nil, nil, nil, nil
	for i := range options.HTTPClients {
		options.HTTPClients[i].DomainResolver = nil
	}
	var experimental *option.ExperimentalOptions
	if keepCacheFile && options.Experimental != nil && options.Experimental.CacheFile != nil {
		cacheFile := *options.Experimental.CacheFile
		cacheFile.StoreFakeIP, cacheFile.StoreDNS, cacheFile.StoreRDRC = false, false, false
		experimental = &option.ExperimentalOptions{CacheFile: &cacheFile}
	}
	options.Experimental = experimental
	options.Log = &option.LogOptions{Disabled: true}

	if options.Route == nil {
		return stripped
	}
	options.Route.DefaultDomainResolver = nil
	kept := make([]option.Rule, 0, len(options.Route.Rules))
	for i, rule := range options.Route.Rules {
		if routeTestRuleNeedsStripped(rule) {
			stripped.skipped = append(stripped.skipped, int32(i))
			continue
		}
		kept = append(kept, rule)
		stripped.indexes = append(stripped.indexes, i)
	}
	options.Route.Rules = kept
	stripped.rules = kept
	return stripped
}

// sing-box names an untagged entry by its index.
func routeTestTag(tag string, index int) string {
	if tag == "" {
		return strconv.Itoa(index)
	}
	return tag
}

// Such rules fail to start without a live endpoint (preferred_by) or a DNS server.
func routeTestRuleNeedsStripped(rule option.Rule) bool {
	if rule.Type == C.RuleTypeLogical {
		return slices.ContainsFunc(rule.LogicalOptions.Rules, routeTestRuleNeedsStripped)
	}
	r := rule.DefaultOptions.RawDefaultRule
	return len(r.PreferredBy) > 0 ||
		r.DNSServerAddress != nil && r.DNSServerAddress.Size() > 0 ||
		r.DNSSearchDomain != nil && r.DNSSearchDomain.Size() > 0
}

func (env *routeTestEnv) test(ctx context.Context, request routeTestRequest) *gen.TestRouteResponse {
	response := &gen.TestRouteResponse{Skipped: env.skipped, Inbound: To(env.inbound.tag)}
	metadata := request.metadata
	metadata.Inbound, metadata.InboundType = env.inbound.tag, env.inbound.kind
	if !env.stripped {
		if err := prepareRouteTestMetadata(env.box, &metadata); err != nil {
			response.Error = To(err.Error())
			return response
		}
	}
	if metadata.Destination.IsIPv4() {
		metadata.IPVersion = 4
	} else if metadata.Destination.IsIPv6() {
		metadata.IPVersion = 6
	}

	walk := routeWalk{
		routeTestRules: env.rules,
		resolve:        boxRouteTestResolver(env.box, env.stripped),
		stripped:       env.stripped,
		protocol:       request.protocol,
		domain:         request.domain,
	}
	steps, decided, failed := walk.run(ctx, &metadata)
	response.Steps = steps
	response.Destination = To(metadata.Destination.String())
	response.Domain = To(metadata.Domain)
	response.Addresses = routeTestAddresses(metadata.DestinationAddresses)
	if failed {
		response.Action = To("error")
		return response
	}

	var outboundTag string
	switch action := decided.(type) {
	case nil:
		response.IsFinal = To(true)
		response.Action = To(C.RuleActionTypeRoute)
		outboundTag = env.box.Outbound().Default().Tag()
	case *R.RuleActionRoute:
		response.Action = To(C.RuleActionTypeRoute)
		outboundTag = action.Outbound
	case *R.RuleActionBypass:
		response.Action = To(C.RuleActionTypeBypass)
		outboundTag = action.Outbound
	default:
		response.Action = To(decided.Type())
	}
	if outboundTag == "" {
		return response
	}
	response.Outbound = To(outboundTag)
	outbound, loaded := env.box.Outbound().Outbound(outboundTag)
	if !loaded {
		steps[len(steps)-1].Error = To("outbound not found: " + outboundTag)
		response.Action = To("error")
		return response
	}
	response.OutboundChain = routeTestChain(outbound, metadata.Network)
	return response
}

// Like the router's prepareMatchMetadata, minus the process search: the request names the process.
func prepareRouteTestMetadata(box *boxbox.Box, metadata *adapter.InboundContext) error {
	address := metadata.Destination.Addr
	if !address.IsValid() {
		return nil
	}
	transports := service.FromContext[adapter.DNSTransportManager](box.Context())
	if transports != nil && transports.FakeIP() != nil && transports.FakeIP().Store().Contains(address) {
		domain, loaded := transports.FakeIP().Store().Lookup(address)
		if !loaded {
			return E.New("no domain is recorded for the fake IP ", address)
		}
		if domain != "" {
			metadata.OriginDestination = metadata.Destination
			metadata.Destination = M.Socksaddr{Fqdn: domain, Port: metadata.Destination.Port}
			metadata.FakeIP = true
		}
	} else if metadata.Domain == "" {
		if dnsRouter := service.FromContext[adapter.DNSRouter](box.Context()); dnsRouter != nil {
			if domain, loaded := dnsRouter.LookupReverseMapping(address); loaded {
				metadata.Domain = domain
			}
		}
	}
	return nil
}

// A stripped config has no DNS section, so a named server is gone and the default (system) resolver answers.
func boxRouteTestResolver(box *boxbox.Box, ignoreServer bool) routeTestResolver {
	dnsRouter := service.FromContext[adapter.DNSRouter](box.Context())
	transports := service.FromContext[adapter.DNSTransportManager](box.Context())
	return func(ctx context.Context, metadata *adapter.InboundContext, action *R.RuleActionResolve) ([]netip.Addr, error) {
		var transport adapter.DNSTransport
		if action.Server != "" && !ignoreServer {
			var loaded bool
			transport, loaded = transports.Transport(action.Server)
			if !loaded {
				return nil, E.New("DNS server not found: ", action.Server)
			}
		}
		return dnsRouter.Lookup(adapter.WithContext(ctx, metadata), metadata.Destination.Fqdn, adapter.DNSQueryOptions{
			Transport:              transport,
			Strategy:               action.Strategy,
			DisableCache:           action.DisableCache,
			DisableOptimisticCache: action.DisableOptimisticCache,
			RewriteTTL:             action.RewriteTTL,
			Timeout:                action.Timeout,
			ClientSubnet:           action.ClientSubnet,
		})
	}
}

func routeTestChain(outbound adapter.Outbound, network string) []string {
	var chain []string
	for range 8 {
		group, isGroup := outbound.(adapter.OutboundGroup)
		if !isGroup {
			break
		}
		outbound = group.Selected(network)
		if outbound == nil {
			break
		}
		chain = append(chain, outbound.Tag())
	}
	return chain
}

func routeTestAddresses(addresses []netip.Addr) []string {
	texts := make([]string, 0, len(addresses))
	for _, address := range addresses {
		texts = append(texts, address.String())
	}
	return texts
}

// routeWalk mirrors the router's matchRule for a connection that sends nothing: sniffing is simulated from the request.
type routeWalk struct {
	routeTestRules
	resolve  routeTestResolver
	stripped bool
	protocol string
	domain   string
}

func (w *routeWalk) run(ctx context.Context, metadata *adapter.InboundContext) (steps []*gen.TestRouteStep, decided adapter.RuleAction, failed bool) {
	for i, rule := range w.rules {
		metadata.ResetRuleCache()
		if !rule.Match(metadata) {
			continue
		}
		action := rule.Action()
		step := &gen.TestRouteStep{
			Index:        To(int32(w.index(i))),
			Rule:         To(rule.String()),
			Action:       To(action.Type()),
			ActionDetail: To(action.String()),
			Matched:      To(w.matched(i, *metadata)),
		}
		steps = append(steps, step)
		var notes []string
		if routeOptions := routeTestRouteOptions(action); routeOptions != nil {
			if overridden := applyRouteTestOverride(metadata, routeOptions); overridden {
				notes = append(notes, "destination overridden to "+metadata.Destination.String())
			}
		}
		switch action := action.(type) {
		case *R.RuleActionRoute, *R.RuleActionReject, *R.RuleActionHijackDNS:
			decided = action
		case *R.RuleActionBypass:
			if action.Outbound != "" {
				decided = action
			} else {
				notes = append(notes, "bypass without an outbound acts only in Tun pre-match; the connection router passes over it")
			}
		case *R.RuleActionSniff:
			notes = append(notes, w.sniff(metadata, action, step)...)
		case *R.RuleActionResolve:
			var note string
			note, failed = w.resolveStep(ctx, metadata, action, step)
			if note != "" {
				notes = append(notes, note)
			}
		}
		if len(notes) > 0 {
			step.Note = To(strings.Join(notes, "; "))
		}
		if decided != nil || failed {
			return
		}
	}
	return
}

// The route options matchRule applies before the action itself; a bypass without an outbound carries none.
func routeTestRouteOptions(action adapter.RuleAction) *R.RuleActionRouteOptions {
	switch action := action.(type) {
	case *R.RuleActionRoute:
		return &action.RuleActionRouteOptions
	case *R.RuleActionRouteOptions:
		return action
	case *R.RuleActionBypass:
		if action.Outbound != "" {
			return &action.RuleActionRouteOptions
		}
	}
	return nil
}

func applyRouteTestOverride(metadata *adapter.InboundContext, options *R.RuleActionRouteOptions) bool {
	if !options.OverrideAddress.IsValid() && options.OverridePort == 0 {
		return false
	}
	if !metadata.RouteOriginalDestination.IsValid() {
		metadata.RouteOriginalDestination = metadata.Destination
	}
	if options.OverrideAddress.IsValid() {
		metadata.DestinationAddresses = nil
		metadata.Destination = M.Socksaddr{
			Addr: options.OverrideAddress.Addr,
			Port: metadata.Destination.Port,
			Fqdn: options.OverrideAddress.Fqdn,
		}
	}
	if options.OverridePort > 0 {
		metadata.Destination.Port = options.OverridePort
	}
	return true
}

func (w *routeWalk) sniff(metadata *adapter.InboundContext, action *R.RuleActionSniff, step *gen.TestRouteStep) []string {
	if sniff.Skip(metadata) {
		return []string{F.ToString("skipped: port ", metadata.Destination.Port, " is server-first")}
	}
	if metadata.Protocol != "" {
		return []string{"skipped: " + metadata.Protocol + " was already sniffed"}
	}
	sniffable, sniffers := routeTestStreamSniffable, len(action.StreamSniffers)
	if metadata.Network == N.NetworkUDP {
		sniffable, sniffers = routeTestPacketSniffable, len(action.PacketSniffers)
	}
	switch {
	case sniffers == 0 && len(action.SnifferNames) > 0:
		return []string{"skipped: none of its sniffers reads " + metadata.Network}
	case w.protocol == "":
		return []string{"found nothing"}
	case !slices.Contains(sniffable, w.protocol):
		return []string{"found nothing: " + w.protocol + " is never sniffed on " + metadata.Network}
	case len(action.SnifferNames) > 0 && !slices.Contains(action.SnifferNames, w.protocol):
		return []string{"found nothing: " + w.protocol + " is not among its sniffers"}
	}
	metadata.Protocol = w.protocol
	step.Protocol = To(w.protocol)
	if w.domain != "" && slices.Contains(routeTestDomainSniffers, w.protocol) {
		metadata.Domain = w.domain
		step.SniffedDomain = To(w.domain)
	}
	if action.OverrideDestination && M.IsDomainName(metadata.Domain) {
		metadata.Destination = M.Socksaddr{Fqdn: metadata.Domain, Port: metadata.Destination.Port}
		return []string{"destination overridden to " + metadata.Destination.String()}
	}
	return nil
}

func (w *routeWalk) resolveStep(ctx context.Context, metadata *adapter.InboundContext, action *R.RuleActionResolve, step *gen.TestRouteStep) (note string, failed bool) {
	if !metadata.Destination.IsDomain() {
		return "nothing to resolve: the destination is not a domain", false
	}
	if w.stripped && action.Server != "" {
		note = "the stripped config has no DNS server " + action.Server + ", so the system resolver was asked"
	}
	addresses, err := w.resolve(ctx, metadata, action)
	if err != nil {
		step.Error = To(err.Error())
		if !w.stripped {
			return note, true
		}
		return strings.TrimPrefix(note+"; matching continued without addresses", "; "), false
	}
	metadata.DestinationAddresses = addresses
	step.Addresses = routeTestAddresses(addresses)
	return note, false
}
