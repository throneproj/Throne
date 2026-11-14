// Package routetest walks route.rules for one made-up connection, dialing nothing: through a running box, or
// through a box started from a config stripped down to route matching.
package routetest

import (
	"context"
	"net/netip"
	"slices"
	"strconv"
	"strings"
	"time"

	"ThroneCore/internal/rulematch"

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
	Timeout            = 30 * time.Second
	stubConnectTimeout = 10 * time.Second
)

// A stripped box keeps the profile's cache.db, which bbolt opens exclusively.
var boxSlot = make(chan struct{}, 1)

var (
	streamSniffable = []string{C.ProtocolTLS, C.ProtocolHTTP, C.ProtocolDNS, C.ProtocolBitTorrent, C.ProtocolSSH, C.ProtocolRDP}
	packetSniffable = []string{C.ProtocolDNS, C.ProtocolQUIC, C.ProtocolSTUN, C.ProtocolBitTorrent, C.ProtocolDTLS, C.ProtocolNTP}
	domainSniffers  = []string{C.ProtocolTLS, C.ProtocolHTTP, C.ProtocolQUIC}
)

// Box is what a test reads from a started box; the desktop's boxbox.Box and the mobile instance's box both fit.
type Box interface {
	Context() context.Context
	Inbound() adapter.InboundManager
	Outbound() adapter.OutboundManager
	Router() adapter.Router
}

// Request is libcore.proto's TestRouteRequest without the config, plus the Android package name.
type Request struct {
	Process     string
	PackageName string
	Domain      string
	IP          string
	Port        int32
	Network     string
	Protocol    string
}

// Step and Result are TestRouteStep and TestRouteResponse, keyed like libcore.proto.
type Step struct {
	Index         int32    `json:"index"`
	Rule          string   `json:"rule"`
	Action        string   `json:"action"`
	ActionDetail  string   `json:"action_detail"`
	Matched       string   `json:"matched,omitempty"`
	Protocol      string   `json:"protocol,omitempty"`
	SniffedDomain string   `json:"sniffed_domain,omitempty"`
	Addresses     []string `json:"addresses,omitempty"`
	Error         string   `json:"error,omitempty"`
	Note          string   `json:"note,omitempty"`
}

type Result struct {
	Error         string   `json:"error,omitempty"`
	Steps         []*Step  `json:"steps,omitempty"`
	IsFinal       bool     `json:"is_final,omitempty"`
	Action        string   `json:"action,omitempty"`
	Outbound      string   `json:"outbound,omitempty"`
	OutboundChain []string `json:"outbound_chain,omitempty"`
	Skipped       []int32  `json:"skipped,omitempty"`
	Destination   string   `json:"destination,omitempty"`
	Domain        string   `json:"domain,omitempty"`
	Addresses     []string `json:"addresses,omitempty"`
	Inbound       string   `json:"inbound,omitempty"`
}

type Query struct {
	Metadata adapter.InboundContext
	// What a sniff rule finds; empty for nothing.
	protocol string
	// What a TLS, HTTP or QUIC sniff reads from the connection.
	domain string
}

func Parse(in Request) (*Query, error) {
	network := strings.ToLower(strings.TrimSpace(in.Network))
	switch network {
	case "":
		network = N.NetworkTCP
	case N.NetworkTCP, N.NetworkUDP:
	default:
		return nil, E.New("unknown network: ", in.Network)
	}
	if in.Port < 1 || in.Port > 65535 {
		return nil, E.New("port out of range: ", in.Port)
	}
	port := uint16(in.Port)

	domain := strings.TrimSuffix(strings.ToLower(strings.TrimSpace(in.Domain)), ".")
	ipText := strings.TrimSpace(in.IP)
	if _, err := parseIP(domain); err == nil {
		if ipText == "" {
			ipText = domain
		}
		domain = ""
	}
	if domain != "" && !M.IsDomainName(domain) {
		return nil, E.New("invalid domain: ", in.Domain)
	}
	destination := M.Socksaddr{Fqdn: domain, Port: port}
	if ipText != "" {
		ip, err := parseIP(ipText)
		if err != nil {
			return nil, E.New("invalid IP address: ", ipText)
		}
		destination = M.Socksaddr{Addr: ip, Port: port}
	}

	metadata := adapter.InboundContext{
		Network:     network,
		Source:      M.Socksaddr{Addr: netip.AddrFrom4([4]byte{127, 0, 0, 1})},
		Destination: destination,
	}
	if process := strings.TrimSpace(in.Process); process != "" {
		metadata.ProcessInfo = &adapter.ConnectionOwner{ProcessPaths: []string{process}}
	}
	if packageName := strings.TrimSpace(in.PackageName); packageName != "" {
		if metadata.ProcessInfo == nil {
			metadata.ProcessInfo = &adapter.ConnectionOwner{}
		}
		metadata.ProcessInfo.PackageNames = []string{packageName}
	}

	protocol := strings.ToLower(strings.TrimSpace(in.Protocol))
	switch protocol {
	case "", "auto":
		protocol = guessProtocol(network, port)
	case "none":
		protocol = ""
	}
	return &Query{Metadata: metadata, protocol: protocol, domain: domain}, nil
}

func parseIP(text string) (netip.Addr, error) {
	addr, err := netip.ParseAddr(strings.TrimSuffix(strings.TrimPrefix(text, "["), "]"))
	return addr.Unmap(), err
}

func guessProtocol(network string, port uint16) string {
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

type inboundInfo struct {
	tag  string
	kind string
}

func pickInbound(inbounds []inboundInfo) inboundInfo {
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
	return inboundInfo{}
}

type ruleList struct {
	rules []adapter.Rule
	// The options each rule was built from; nil when unknown.
	options []option.Rule
	// The config index of each rule; nil when every rule kept its place.
	indexes []int
	ruleSet func(tag string) (adapter.RuleSet, bool)
}

func (r *ruleList) index(i int) int {
	if i < len(r.indexes) {
		return r.indexes[i]
	}
	return i
}

func (r *ruleList) matched(i int, metadata adapter.InboundContext) string {
	if i >= len(r.options) {
		return ""
	}
	return rulematch.Matched(rulematch.Entries(r.options[i], r.ruleSet), metadata)
}

type resolver func(ctx context.Context, metadata *adapter.InboundContext, action *R.RuleActionResolve) ([]netip.Addr, error)

type Env struct {
	box      Box
	rules    ruleList
	inbound  inboundInfo
	skipped  []int32
	stripped bool
	close    func()
}

func (env *Env) Close() {
	env.close()
}

// Running tests box as it runs; options are the route rules it was built from, nil when unknown.
func Running(box Box, options []option.Rule) *Env {
	var inbounds []inboundInfo
	for _, inbound := range box.Inbound().Inbounds() {
		inbounds = append(inbounds, inboundInfo{inbound.Tag(), inbound.Type()})
	}
	rules := box.Router().Rules()
	if len(options) != len(rules) {
		options = nil
	}
	return &Env{
		box:     box,
		rules:   ruleList{rules: rules, options: options, ruleSet: box.Router().RuleSet},
		inbound: pickInbound(inbounds),
		close:   func() {},
	}
}

// Create builds and starts a box from the config under test, after strip has adjusted the parsed options;
// release closes that box and returns once it is closed.
type Create func(strip func(*option.Options)) (box Box, release func(), err error)

// StartStripped starts one stripped box at a time. keepCacheFile, asked once the slot is taken, says whether the
// config's cache file is free to open: not while a running box holds it.
func StartStripped(ctx context.Context, keepCacheFile func() bool, create Create) (*Env, error) {
	select {
	case boxSlot <- struct{}{}:
	case <-ctx.Done():
		return nil, E.New("another route test is still running")
	}
	keep := keepCacheFile()
	type created struct {
		box     Box
		release func()
		err     error
	}
	done := make(chan created, 1)
	var stripped strippedConfig
	go func() {
		var c created
		// On a goroutine of its own, which no caller's recover covers.
		defer func() {
			if r := recover(); r != nil {
				c = created{err: E.New("core panic: ", r)}
			}
			done <- c
		}()
		c.box, c.release, c.err = create(func(options *option.Options) {
			stripped = strip(options, keep)
		})
	}()
	release := func(c created) {
		if c.err == nil {
			c.release()
		}
		<-boxSlot
	}
	select {
	case c := <-done:
		if c.err != nil {
			release(c)
			return nil, c.err
		}
		return &Env{
			box:      c.box,
			rules:    ruleList{rules: c.box.Router().Rules(), options: stripped.rules, indexes: stripped.indexes, ruleSet: c.box.Router().RuleSet},
			inbound:  stripped.inbound,
			skipped:  stripped.skipped,
			stripped: true,
			close:    func() { go release(c) },
		}, nil
	case <-ctx.Done():
		go func() { release(<-done) }()
		return nil, E.New("the config did not start in ", Timeout)
	}
}

type strippedConfig struct {
	inbound inboundInfo
	// The kept route rules, aligned with the stripped router's.
	rules   []option.Rule
	indexes []int
	skipped []int32
}

// strip keeps only what route matching reads: nothing left listens, dials at start or names a DNS server.
func strip(options *option.Options, keepCacheFile bool) strippedConfig {
	var stripped strippedConfig
	inbounds := make([]inboundInfo, 0, len(options.Inbounds))
	for i, inbound := range options.Inbounds {
		inbounds = append(inbounds, inboundInfo{tagOrIndex(inbound.Tag, i), inbound.Type})
	}
	stripped.inbound = pickInbound(inbounds)
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
				ConnectTimeout: badoption.Duration(stubConnectTimeout),
			}},
		}})
	}
	for i, outbound := range options.Outbounds {
		addStub(tagOrIndex(outbound.Tag, i))
	}
	for i, endpoint := range options.Endpoints {
		addStub(tagOrIndex(endpoint.Tag, i))
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
		if needsLiveConfig(rule) {
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
func tagOrIndex(tag string, index int) string {
	if tag == "" {
		return strconv.Itoa(index)
	}
	return tag
}

// Such rules fail to start without a live endpoint (preferred_by) or a DNS server.
func needsLiveConfig(rule option.Rule) bool {
	if rule.Type == C.RuleTypeLogical {
		return slices.ContainsFunc(rule.LogicalOptions.Rules, needsLiveConfig)
	}
	r := rule.DefaultOptions.RawDefaultRule
	return len(r.PreferredBy) > 0 ||
		r.DNSServerAddress != nil && r.DNSServerAddress.Size() > 0 ||
		r.DNSSearchDomain != nil && r.DNSSearchDomain.Size() > 0
}

func (env *Env) Test(ctx context.Context, query *Query) *Result {
	result := &Result{Skipped: env.skipped, Inbound: env.inbound.tag}
	metadata := query.Metadata
	metadata.Inbound, metadata.InboundType = env.inbound.tag, env.inbound.kind
	if !env.stripped {
		if err := prepareMetadata(env.box, &metadata); err != nil {
			result.Error = err.Error()
			return result
		}
	}
	if metadata.Destination.IsIPv4() {
		metadata.IPVersion = 4
	} else if metadata.Destination.IsIPv6() {
		metadata.IPVersion = 6
	}

	walk := ruleWalk{
		ruleList: env.rules,
		resolve:  boxResolver(env.box, env.stripped),
		stripped: env.stripped,
		protocol: query.protocol,
		domain:   query.domain,
	}
	steps, decided, failed := walk.run(ctx, &metadata)
	result.Steps = steps
	result.Destination = metadata.Destination.String()
	result.Domain = metadata.Domain
	result.Addresses = addressTexts(metadata.DestinationAddresses)
	if failed {
		result.Action = "error"
		return result
	}

	var outboundTag string
	switch action := decided.(type) {
	case nil:
		result.IsFinal = true
		result.Action = C.RuleActionTypeRoute
		outboundTag = env.box.Outbound().Default().Tag()
	case *R.RuleActionRoute:
		result.Action = C.RuleActionTypeRoute
		outboundTag = action.Outbound
	case *R.RuleActionBypass:
		result.Action = C.RuleActionTypeBypass
		outboundTag = action.Outbound
	default:
		result.Action = decided.Type()
	}
	if outboundTag == "" {
		return result
	}
	result.Outbound = outboundTag
	outbound, loaded := env.box.Outbound().Outbound(outboundTag)
	if !loaded {
		steps[len(steps)-1].Error = "outbound not found: " + outboundTag
		result.Action = "error"
		return result
	}
	result.OutboundChain = chain(outbound, metadata.Network)
	return result
}

// Like the router's prepareMatchMetadata, minus the process search: the request names the process.
func prepareMetadata(box Box, metadata *adapter.InboundContext) error {
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
func boxResolver(box Box, ignoreServer bool) resolver {
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

func chain(outbound adapter.Outbound, network string) []string {
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

func addressTexts(addresses []netip.Addr) []string {
	texts := make([]string, 0, len(addresses))
	for _, address := range addresses {
		texts = append(texts, address.String())
	}
	return texts
}

// ruleWalk mirrors the router's matchRule for a connection that sends nothing: sniffing is simulated from the request.
type ruleWalk struct {
	ruleList
	resolve  resolver
	stripped bool
	protocol string
	domain   string
}

func (w *ruleWalk) run(ctx context.Context, metadata *adapter.InboundContext) (steps []*Step, decided adapter.RuleAction, failed bool) {
	for i, rule := range w.rules {
		metadata.ResetRuleCache()
		if !rule.Match(metadata) {
			continue
		}
		action := rule.Action()
		step := &Step{
			Index:        int32(w.index(i)),
			Rule:         rule.String(),
			Action:       action.Type(),
			ActionDetail: action.String(),
			Matched:      w.matched(i, *metadata),
		}
		steps = append(steps, step)
		var notes []string
		if routeOptions := routeOptionsOf(action); routeOptions != nil {
			if overridden := applyOverride(metadata, routeOptions); overridden {
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
			step.Note = strings.Join(notes, "; ")
		}
		if decided != nil || failed {
			return
		}
	}
	return
}

// The route options matchRule applies before the action itself; a bypass without an outbound carries none.
func routeOptionsOf(action adapter.RuleAction) *R.RuleActionRouteOptions {
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

func applyOverride(metadata *adapter.InboundContext, options *R.RuleActionRouteOptions) bool {
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

func (w *ruleWalk) sniff(metadata *adapter.InboundContext, action *R.RuleActionSniff, step *Step) []string {
	if sniff.Skip(metadata) {
		return []string{F.ToString("skipped: port ", metadata.Destination.Port, " is server-first")}
	}
	if metadata.Protocol != "" {
		return []string{"skipped: " + metadata.Protocol + " was already sniffed"}
	}
	sniffable, sniffers := streamSniffable, len(action.StreamSniffers)
	if metadata.Network == N.NetworkUDP {
		sniffable, sniffers = packetSniffable, len(action.PacketSniffers)
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
	step.Protocol = w.protocol
	if w.domain != "" && slices.Contains(domainSniffers, w.protocol) {
		metadata.Domain = w.domain
		step.SniffedDomain = w.domain
	}
	if action.OverrideDestination && M.IsDomainName(metadata.Domain) {
		metadata.Destination = M.Socksaddr{Fqdn: metadata.Domain, Port: metadata.Destination.Port}
		return []string{"destination overridden to " + metadata.Destination.String()}
	}
	return nil
}

func (w *ruleWalk) resolveStep(ctx context.Context, metadata *adapter.InboundContext, action *R.RuleActionResolve, step *Step) (note string, failed bool) {
	if !metadata.Destination.IsDomain() {
		return "nothing to resolve: the destination is not a domain", false
	}
	if w.stripped && action.Server != "" {
		note = "the stripped config has no DNS server " + action.Server + ", so the system resolver was asked"
	}
	addresses, err := w.resolve(ctx, metadata, action)
	if err != nil {
		step.Error = err.Error()
		if !w.stripped {
			return note, true
		}
		return strings.TrimPrefix(note+"; matching continued without addresses", "; "), false
	}
	metadata.DestinationAddresses = addresses
	step.Addresses = addressTexts(addresses)
	return note, false
}
