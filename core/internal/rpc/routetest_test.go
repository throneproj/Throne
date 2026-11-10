package rpc

import (
	"context"
	"errors"
	"net/netip"
	"path/filepath"
	"reflect"
	"runtime"
	"slices"
	"strings"
	"testing"
	"time"

	"ThroneCore/gen"
	"ThroneCore/internal/boxmain"

	"github.com/sagernet/sing-box/adapter"
	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/include"
	sblog "github.com/sagernet/sing-box/log"
	"github.com/sagernet/sing-box/option"
	R "github.com/sagernet/sing-box/route/rule"
	"github.com/sagernet/sing/common/json"
	N "github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/service"
)

const routeTestConfig = `{
  "log": {"level": "warn"},
  "inbounds": [
    {"type": "direct", "tag": "dns-in", "listen": "127.0.0.1", "listen_port": 6450},
    {"type": "mixed", "tag": "mixed-in", "listen": "127.0.0.1", "listen_port": 2080}
  ],
  "endpoints": [
    {"type": "wireguard", "tag": "wg", "address": ["10.9.0.2/32"], "private_key": "x",
     "peers": [{"address": "127.0.0.1", "port": 1, "public_key": "y", "allowed_ips": ["0.0.0.0/0"]}]}
  ],
  "outbounds": [
    {"type": "direct", "tag": "direct"},
    {"type": "socks", "tag": "proxy", "server": "127.0.0.1", "server_port": 1, "domain_resolver": "remote"}
  ],
  "dns": {"servers": [{"type": "udp", "tag": "remote", "server": "127.0.0.1"}]},
  "route": {
    "rules": [
      {"action": "sniff"},
      {"preferred_by": ["wg"], "action": "route", "outbound": "wg"},
      {"type": "logical", "mode": "or", "rules": [{"domain": ["x.test"]}, {"preferred_by": ["wg"]}], "action": "reject"},
      {"process_name": ["chrome.exe"], "domain_suffix": ["blocked.test"], "action": "reject"},
      {"domain_suffix": ["github.com"], "action": "route", "outbound": "direct"},
      {"ip_cidr": ["10.0.0.0/8"], "action": "reject"},
      {"domain_suffix": ["invalid"], "action": "resolve", "server": "remote", "timeout": "3s"}
    ],
    "final": "proxy",
    "default_domain_resolver": "remote"
  },
  "experimental": {
    "cache_file": {"enabled": true, "path": "routetest-unused.db", "store_fakeip": true, "store_dns": true},
    "clash_api": {"external_controller": "127.0.0.1:9090"}
  }
}`

func TestRouteTestRequest(t *testing.T) {
	type want struct {
		destination string
		domain      string
		protocol    string
		process     string
		network     string
	}
	for _, tc := range []struct {
		name string
		in   *gen.TestRouteRequest
		want want
		err  bool
	}{
		{"ip and domain: the domain is only what a sniff finds",
			&gen.TestRouteRequest{Domain: To(" API.GitHub.com. "), Ip: To("140.82.112.5")},
			want{destination: "140.82.112.5:443", domain: "api.github.com", protocol: C.ProtocolTLS, network: N.NetworkTCP}, false},
		{"domain only", &gen.TestRouteRequest{Domain: To("example.org"), Port: To(int32(80))},
			want{destination: "example.org:80", domain: "example.org", protocol: C.ProtocolHTTP, network: N.NetworkTCP}, false},
		{"process only has no address", &gen.TestRouteRequest{Process: To(` C:\Apps\chrome.exe `)},
			want{destination: ":443", protocol: C.ProtocolTLS, process: `C:\Apps\chrome.exe`, network: N.NetworkTCP}, false},
		{"an IP in the domain field is the IP", &gen.TestRouteRequest{Domain: To("1.1.1.1"), Network: To("UDP"), Port: To(int32(53))},
			want{destination: "1.1.1.1:53", protocol: C.ProtocolDNS, network: N.NetworkUDP}, false},
		{"an IP in the domain field never overrides ip", &gen.TestRouteRequest{Domain: To("1.1.1.1"), Ip: To("8.8.8.8")},
			want{destination: "8.8.8.8:443", protocol: C.ProtocolTLS, network: N.NetworkTCP}, false},
		{"bracketed and mapped IPs", &gen.TestRouteRequest{Ip: To("[::ffff:10.0.0.1]"), Protocol: To("none")},
			want{destination: "10.0.0.1:443", network: N.NetworkTCP}, false},
		{"ipv6", &gen.TestRouteRequest{Ip: To("2606:4700::1111"), Network: To("udp")},
			want{destination: "[2606:4700::1111]:443", protocol: C.ProtocolQUIC, network: N.NetworkUDP}, false},
		{"explicit protocol", &gen.TestRouteRequest{Ip: To("1.2.3.4"), Port: To(int32(6881)), Protocol: To("BitTorrent")},
			want{destination: "1.2.3.4:6881", protocol: C.ProtocolBitTorrent, network: N.NetworkTCP}, false},
		{"bad ip", &gen.TestRouteRequest{Ip: To("1.2.3")}, want{}, true},
		{"bad domain", &gen.TestRouteRequest{Domain: To("exa mple.org")}, want{}, true},
		{"bad network", &gen.TestRouteRequest{Network: To("icmp")}, want{}, true},
		{"bad port", &gen.TestRouteRequest{Port: To(int32(70000))}, want{}, true},
	} {
		t.Run(tc.name, func(t *testing.T) {
			request, err := parseRouteTestRequest(tc.in)
			if tc.err {
				if err == nil {
					t.Fatal("expected an error")
				}
				return
			}
			if err != nil {
				t.Fatal(err)
			}
			metadata := request.metadata
			var process string
			if metadata.ProcessInfo != nil {
				process = metadata.ProcessInfo.ProcessPaths[0]
			}
			got := want{metadata.Destination.String(), request.domain, request.protocol, process, metadata.Network}
			if got != tc.want {
				t.Errorf("got %+v, want %+v", got, tc.want)
			}
			if metadata.Domain != "" {
				t.Errorf("metadata.Domain = %q before any sniff", metadata.Domain)
			}
		})
	}
}

func TestRouteTestProtocolGuess(t *testing.T) {
	for _, tc := range []struct {
		network string
		port    uint16
		want    string
	}{
		{N.NetworkTCP, 443, C.ProtocolTLS}, {N.NetworkTCP, 80, C.ProtocolHTTP}, {N.NetworkTCP, 22, C.ProtocolSSH},
		{N.NetworkTCP, 3389, C.ProtocolRDP}, {N.NetworkTCP, 53, C.ProtocolDNS}, {N.NetworkTCP, 8443, ""},
		{N.NetworkUDP, 443, C.ProtocolQUIC}, {N.NetworkUDP, 53, C.ProtocolDNS}, {N.NetworkUDP, 123, C.ProtocolNTP},
		{N.NetworkUDP, 3478, C.ProtocolSTUN}, {N.NetworkUDP, 19302, C.ProtocolSTUN}, {N.NetworkUDP, 80, ""},
	} {
		if got := guessRouteTestProtocol(tc.network, tc.port); got != tc.want {
			t.Errorf("guessRouteTestProtocol(%s, %d) = %q, want %q", tc.network, tc.port, got, tc.want)
		}
	}
}

func TestRouteTestStrip(t *testing.T) {
	options, err := json.UnmarshalExtendedContext[option.Options](include.Context(context.Background()), []byte(routeTestConfig))
	if err != nil {
		t.Fatal(err)
	}
	stripped := stripRouteTestConfig(&options, true)

	if stripped.inbound != (routeTestInbound{"mixed-in", C.TypeMixed}) {
		t.Errorf("inbound = %+v", stripped.inbound)
	}
	if !slices.Equal(stripped.skipped, []int32{1, 2}) {
		t.Errorf("skipped = %v, want [1 2]", stripped.skipped)
	}
	if !slices.Equal(stripped.indexes, []int{0, 3, 4, 5, 6}) || len(stripped.rules) != 5 || len(options.Route.Rules) != 5 {
		t.Errorf("indexes = %v, %d rules", stripped.indexes, len(stripped.rules))
	}
	if options.Inbounds != nil || options.Endpoints != nil || options.DNS != nil || options.Route.DefaultDomainResolver != nil {
		t.Error("inbounds, endpoints, dns or the default domain resolver survived")
	}
	var tags []string
	for _, outbound := range options.Outbounds {
		tags = append(tags, outbound.Tag)
		direct, isDirect := outbound.Options.(*option.DirectOutboundOptions)
		if outbound.Type != C.TypeDirect || !isDirect {
			t.Fatalf("%s is a %s, not a direct stub", outbound.Tag, outbound.Type)
		}
		// The direct outbound's own emptiness test, which detours refuse.
		empty := option.DialerOptions{AbstractDialerOptions: option.AbstractDialerOptions{UDPFragmentDefault: true}}
		dialer := direct.DialerOptions
		dialer.UDPFragmentDefault = true
		if reflect.DeepEqual(dialer, empty) {
			t.Errorf("stub %s is an empty direct outbound", outbound.Tag)
		}
	}
	if !slices.Equal(tags, []string{"direct", "proxy", "wg"}) {
		t.Errorf("stub tags = %v", tags)
	}
	experimental := options.Experimental
	if experimental == nil || experimental.ClashAPI != nil || experimental.CacheFile == nil ||
		experimental.CacheFile.Path != "routetest-unused.db" || experimental.CacheFile.StoreFakeIP || experimental.CacheFile.StoreDNS {
		t.Errorf("experimental = %+v", experimental)
	}
	if options.Log == nil || !options.Log.Disabled {
		t.Error("log is still enabled")
	}

	options.Experimental = &option.ExperimentalOptions{CacheFile: &option.CacheFileOptions{Enabled: true}}
	stripRouteTestConfig(&options, false)
	if options.Experimental != nil {
		t.Error("the cache file was kept while another box holds it")
	}

	nested := option.Rule{Type: C.RuleTypeLogical, LogicalOptions: option.LogicalRule{RawLogicalRule: option.RawLogicalRule{
		Mode: C.LogicalTypeAnd,
		Rules: []option.Rule{
			{Type: C.RuleTypeDefault, DefaultOptions: option.DefaultRule{RawDefaultRule: option.RawDefaultRule{Port: []uint16{443}}}},
			{Type: C.RuleTypeLogical, LogicalOptions: option.LogicalRule{RawLogicalRule: option.RawLogicalRule{Mode: C.LogicalTypeOr, Rules: []option.Rule{
				{Type: C.RuleTypeDefault, DefaultOptions: option.DefaultRule{RawDefaultRule: option.RawDefaultRule{PreferredBy: []string{"wg"}}}},
			}}}},
		},
	}}}
	if !routeTestRuleNeedsStripped(nested) {
		t.Error("a preferred_by two levels down was kept")
	}
	if routeTestRuleNeedsStripped(nested.LogicalOptions.Rules[0]) {
		t.Error("a plain port rule was dropped")
	}
}

func routeTestRulesFrom(t *testing.T, rulesJSON string) routeTestRules {
	t.Helper()
	options, err := json.UnmarshalExtendedContext[[]option.Rule](context.Background(), []byte(rulesJSON))
	if err != nil {
		t.Fatal(err)
	}
	logger := sblog.NewNOPFactory().NewLogger("")
	rules := make([]adapter.Rule, 0, len(options))
	for i, ruleOptions := range options {
		rule, err := R.NewRule(context.Background(), logger, ruleOptions, false)
		if err != nil {
			t.Fatalf("rule[%d]: %v", i, err)
		}
		rules = append(rules, rule)
	}
	return routeTestRules{rules: rules, options: options, ruleSet: func(string) (adapter.RuleSet, bool) { return nil, false }}
}

type routeTestStepView struct {
	index    int32
	action   string
	matched  string
	protocol string
	sniffed  string
	failed   bool
}

func viewRouteTestSteps(steps []*gen.TestRouteStep) []routeTestStepView {
	views := make([]routeTestStepView, 0, len(steps))
	for _, step := range steps {
		views = append(views, routeTestStepView{step.GetIndex(), step.GetAction(), step.GetMatched(), step.GetProtocol(), step.GetSniffedDomain(), step.GetError() != ""})
	}
	return views
}

func TestRouteWalk(t *testing.T) {
	failingResolver := func(context.Context, *adapter.InboundContext, *R.RuleActionResolve) ([]netip.Addr, error) {
		return nil, errors.New("lookup failed")
	}
	fixedResolver := func(_ context.Context, metadata *adapter.InboundContext, _ *R.RuleActionResolve) ([]netip.Addr, error) {
		if metadata.Destination.Fqdn == "" {
			return nil, errors.New("resolved a non-domain")
		}
		return []netip.Addr{netip.MustParseAddr("10.0.0.7")}, nil
	}
	for _, tc := range []struct {
		name     string
		request  *gen.TestRouteRequest
		rules    string
		resolve  routeTestResolver
		stripped bool
		want     []routeTestStepView
		decided  string
		failed   bool
		note     string
	}{
		{
			name:    "sniff finds the domain a later rule routes on",
			request: &gen.TestRouteRequest{Process: To("/usr/bin/chrome.exe"), Domain: To("api.github.com"), Ip: To("140.82.112.5")},
			rules: `[{"action": "sniff"},
				{"domain_suffix": ["github.com"], "process_name": ["firefox"], "action": "reject"},
				{"domain_suffix": ["a.com", "b.com", "c.com", "github.com"], "process_name": ["chrome.exe"], "action": "route", "outbound": "proxy"}]`,
			want: []routeTestStepView{
				{index: 0, action: C.RuleActionTypeSniff, protocol: C.ProtocolTLS, sniffed: "api.github.com"},
				{index: 2, action: C.RuleActionTypeRoute, matched: "domain_suffix=github.com process_name=chrome.exe"},
			},
			decided: "route(proxy)",
		},
		{
			name:    "a server-first port is not sniffed",
			request: &gen.TestRouteRequest{Ip: To("1.2.3.4"), Domain: To("smtp.example.org"), Port: To(int32(25))},
			rules:   `[{"action": "sniff"}, {"domain_suffix": ["example.org"], "action": "reject"}]`,
			want:    []routeTestStepView{{index: 0, action: C.RuleActionTypeSniff}},
			note:    "server-first",
		},
		{
			name:    "a sniffer list without the protocol finds nothing",
			request: &gen.TestRouteRequest{Ip: To("1.2.3.4"), Domain: To("example.org")},
			rules:   `[{"action": "sniff", "sniffer": ["http"]}, {"action": "sniff"}, {"protocol": ["tls"], "action": "reject"}]`,
			want: []routeTestStepView{
				{index: 0, action: C.RuleActionTypeSniff},
				{index: 1, action: C.RuleActionTypeSniff, protocol: C.ProtocolTLS, sniffed: "example.org"},
				{index: 2, action: C.RuleActionTypeReject},
			},
			decided: "reject",
		},
		{
			name:    "quic is never sniffed on tcp",
			request: &gen.TestRouteRequest{Ip: To("1.2.3.4"), Protocol: To("quic")},
			rules:   `[{"action": "sniff"}]`,
			want:    []routeTestStepView{{index: 0, action: C.RuleActionTypeSniff}},
			note:    "never sniffed on tcp",
		},
		{
			name:    "override_destination turns the IP into the sniffed domain",
			request: &gen.TestRouteRequest{Ip: To("1.2.3.4"), Domain: To("example.org")},
			rules:   `[{"action": "sniff", "override_destination": true}, {"ip_cidr": ["1.2.3.4/32"], "action": "reject"}, {"domain": ["example.org"], "action": "route", "outbound": "direct"}]`,
			want: []routeTestStepView{
				{index: 0, action: C.RuleActionTypeSniff, protocol: C.ProtocolTLS, sniffed: "example.org"},
				{index: 2, action: C.RuleActionTypeRoute, matched: "domain=example.org"},
			},
			decided: "route(direct)",
			note:    "overridden to example.org:443",
		},
		{
			name:    "route-options override the port and matching goes on",
			request: &gen.TestRouteRequest{Domain: To("example.org")},
			rules:   `[{"domain": ["example.org"], "action": "route-options", "override_port": 8443}, {"port": [443], "action": "reject"}, {"port": [8443], "action": "route", "outbound": "direct"}]`,
			want: []routeTestStepView{
				{index: 0, action: C.RuleActionTypeRouteOptions, matched: "domain=example.org"},
				{index: 2, action: C.RuleActionTypeRoute},
			},
			decided: "route(direct)",
		},
		{
			name:    "a bypass without an outbound is passed over",
			request: &gen.TestRouteRequest{Ip: To("192.168.1.1")},
			rules:   `[{"ip_is_private": true, "action": "bypass"}, {"ip_is_private": true, "action": "bypass", "outbound": "direct"}]`,
			want: []routeTestStepView{
				{index: 0, action: C.RuleActionTypeBypass, matched: "ip_is_private=true"},
				{index: 1, action: C.RuleActionTypeBypass, matched: "ip_is_private=true"},
			},
			decided: "bypass(direct)",
		},
		{
			name:    "resolve feeds ip_cidr",
			request: &gen.TestRouteRequest{Domain: To("intranet.example")},
			resolve: fixedResolver,
			rules:   `[{"ip_cidr": ["10.0.0.0/8"], "action": "reject"}, {"action": "resolve"}, {"ip_cidr": ["10.0.0.0/8"], "action": "route", "outbound": "direct"}]`,
			want: []routeTestStepView{
				{index: 1, action: C.RuleActionTypeResolve},
				{index: 2, action: C.RuleActionTypeRoute, matched: "ip_cidr=10.0.0.0/8"},
			},
			decided: "route(direct)",
		},
		{
			name:    "resolve on an IP does nothing",
			request: &gen.TestRouteRequest{Ip: To("1.1.1.1")},
			resolve: fixedResolver,
			rules:   `[{"action": "resolve"}]`,
			want:    []routeTestStepView{{index: 0, action: C.RuleActionTypeResolve}},
			note:    "nothing to resolve",
		},
		{
			name:    "a failed resolve fails the running box's connection",
			request: &gen.TestRouteRequest{Domain: To("example.org")},
			resolve: failingResolver,
			rules:   `[{"action": "resolve"}, {"action": "route", "outbound": "direct", "port": 443}]`,
			want:    []routeTestStepView{{index: 0, action: C.RuleActionTypeResolve, failed: true}},
			failed:  true,
		},
		{
			name:     "a failed resolve in a stripped config goes on",
			request:  &gen.TestRouteRequest{Domain: To("example.org")},
			resolve:  failingResolver,
			stripped: true,
			rules:    `[{"action": "resolve", "server": "remote"}, {"port": [443], "action": "route", "outbound": "direct"}]`,
			want: []routeTestStepView{
				{index: 0, action: C.RuleActionTypeResolve, failed: true},
				{index: 1, action: C.RuleActionTypeRoute},
			},
			decided: "route(direct)",
		},
		{
			// Every destination-reading item must cope with a connection that has no address at all.
			name:    "a process-only connection",
			request: &gen.TestRouteRequest{Process: To("chrome.exe"), Protocol: To("none")},
			resolve: fixedResolver,
			rules: `[{"action": "sniff", "override_destination": true}, {"action": "resolve"},
				{"domain": ["a.test"], "domain_suffix": ["test"], "domain_keyword": ["a"], "domain_regex": ["^a"], "action": "reject"},
				{"ip_cidr": ["0.0.0.0/0", "::/0"], "action": "reject"},
				{"ip_is_private": true, "action": "reject"},
				{"ip_version": 6, "action": "reject"},
				{"port_range": ["1:100"], "action": "reject"},
				{"type": "logical", "mode": "or", "rules": [{"ip_cidr": ["10.0.0.0/8"]}, {"domain": ["x.test"]}], "action": "reject"},
				{"process_name": ["chrome.exe"], "invert": true, "action": "reject"},
				{"process_name": ["chrome.exe"], "network": ["tcp"], "port": [443], "action": "route", "outbound": "proxy"}]`,
			want: []routeTestStepView{
				{index: 0, action: C.RuleActionTypeSniff},
				{index: 1, action: C.RuleActionTypeResolve},
				{index: 9, action: C.RuleActionTypeRoute, matched: "process_name=chrome.exe"},
			},
			decided: "route(proxy)",
		},
	} {
		t.Run(tc.name, func(t *testing.T) {
			request, err := parseRouteTestRequest(tc.request)
			if err != nil {
				t.Fatal(err)
			}
			walk := routeWalk{
				routeTestRules: routeTestRulesFrom(t, tc.rules),
				resolve:        tc.resolve,
				stripped:       tc.stripped,
				protocol:       request.protocol,
				domain:         request.domain,
			}
			metadata := request.metadata
			steps, decided, failed := walk.run(context.Background(), &metadata)
			if got := viewRouteTestSteps(steps); !slices.Equal(got, tc.want) {
				t.Errorf("steps =\n%+v\nwant\n%+v", got, tc.want)
			}
			var decidedText string
			if decided != nil {
				decidedText = decided.String()
			}
			if decidedText != tc.decided || failed != tc.failed {
				t.Errorf("decided %q, failed %v; want %q, %v", decidedText, failed, tc.decided, tc.failed)
			}
			if tc.note != "" && (len(steps) == 0 || !strings.Contains(steps[0].GetNote(), tc.note)) {
				t.Errorf("first step's note does not mention %q: %+v", tc.note, steps)
			}
		})
	}
}

func TestRouteWalkReportsConfigIndexes(t *testing.T) {
	rules := routeTestRulesFrom(t, `[{"port": [443], "action": "route-options", "udp_connect": true}, {"port": [443], "action": "reject"}]`)
	rules.indexes = []int{3, 7}
	request, _ := parseRouteTestRequest(&gen.TestRouteRequest{Ip: To("1.2.3.4")})
	steps, _, _ := (&routeWalk{routeTestRules: rules}).run(context.Background(), &request.metadata)
	if len(steps) != 2 || steps[0].GetIndex() != 3 || steps[1].GetIndex() != 7 {
		t.Errorf("indexes = %+v", viewRouteTestSteps(steps))
	}
}

func TestRouteTestStrippedBox(t *testing.T) {
	config := strings.Replace(routeTestConfig, `"cache_file": {"enabled": true, "path": "routetest-unused.db", "store_fakeip": true, "store_dns": true},`, "", 1)
	test := func(in *gen.TestRouteRequest) *gen.TestRouteResponse {
		t.Helper()
		in.Config = To(config)
		response, err := globalServer.TestRoute(context.Background(), in)
		if err != nil {
			t.Fatal(err)
		}
		if message := response.GetError(); message != "" {
			lowered := strings.ToLower(message)
			if strings.Contains(lowered, "permission") || strings.Contains(lowered, "access is denied") || strings.Contains(lowered, "not permitted") {
				t.Skipf("the stripped box needs privileges here: %s", message)
			}
			t.Fatalf("TestRoute: %s", message)
		}
		if !slices.Equal(response.GetSkipped(), []int32{1, 2}) || response.GetInbound() != "mixed-in" {
			t.Errorf("skipped %v, inbound %q", response.GetSkipped(), response.GetInbound())
		}
		return response
	}

	response := test(&gen.TestRouteRequest{Process: To("chrome.exe"), Domain: To("api.github.com"), Ip: To("140.82.112.5")})
	if got := viewRouteTestSteps(response.GetSteps()); !slices.Equal(got, []routeTestStepView{
		{index: 0, action: C.RuleActionTypeSniff, protocol: C.ProtocolTLS, sniffed: "api.github.com"},
		{index: 4, action: C.RuleActionTypeRoute, matched: "domain_suffix=github.com"},
	}) {
		t.Errorf("steps = %+v", got)
	}
	if response.GetAction() != C.RuleActionTypeRoute || response.GetOutbound() != "direct" || response.GetIsFinal() ||
		response.GetDestination() != "140.82.112.5:443" || response.GetDomain() != "api.github.com" {
		t.Errorf("response = %v", response)
	}

	response = test(&gen.TestRouteRequest{Ip: To("10.1.2.3"), Port: To(int32(25))})
	if response.GetAction() != C.RuleActionTypeReject || len(response.GetSteps()) != 2 || response.GetSteps()[1].GetIndex() != 5 {
		t.Errorf("response = %v", response)
	}

	// The resolve rule names a server the stripped config no longer has; a lookup failure only notes itself.
	response = test(&gen.TestRouteRequest{Process: To(`C:\Program Files\Google\chrome.exe`), Domain: To("nothing.invalid")})
	steps := response.GetSteps()
	if len(steps) != 2 || steps[1].GetIndex() != 6 || !strings.Contains(steps[1].GetNote(), "remote") {
		t.Errorf("steps = %v", steps)
	}
	if !response.GetIsFinal() || response.GetAction() != C.RuleActionTypeRoute || response.GetOutbound() != "proxy" || len(response.GetOutboundChain()) != 0 {
		t.Errorf("response = %v", response)
	}
}

func TestRouteTestRunningBox(t *testing.T) {
	if currentBox() != nil {
		t.Skip("a box is running")
	}
	const config = `{
	  "log": {"disabled": true},
	  "inbounds": [{"type": "mixed", "tag": "mixed-in", "listen": "127.0.0.1", "listen_port": 0}],
	  "outbounds": [
	    {"type": "selector", "tag": "proxy", "outbounds": ["inner"]},
	    {"type": "selector", "tag": "inner", "outbounds": ["direct"]},
	    {"type": "direct", "tag": "direct"}
	  ],
	  "dns": {"servers": [{"type": "local", "tag": "local"}, {"type": "fakeip", "tag": "fake", "inet4_range": "198.18.0.0/15"}], "final": "local"},
	  "route": {
	    "rules": [
	      {"action": "sniff"},
	      {"domain_suffix": ["example.org"], "action": "route", "outbound": "proxy"},
	      {"domain_suffix": ["invalid"], "action": "resolve", "server": "missing"}
	    ],
	    "final": "direct"
	  }
	}`
	var rules []option.Rule
	box, cancel, err := boxmain.Create([]byte(config), nil, func(options *option.Options) { rules = options.Route.Rules })
	if err != nil {
		t.Fatal(err)
	}
	setBoxInstance(box, cancel, rules)
	defer func() {
		setBoxInstance(nil, nil, nil)
		box.CloseWithTimeout(cancel, 2*time.Second, t.Log, true)
	}()
	fakeIP, err := service.FromContext[adapter.DNSTransportManager](box.Context()).FakeIP().Store().Create("www.example.org", false)
	if err != nil {
		t.Fatal(err)
	}

	response, err := globalServer.TestRoute(context.Background(), &gen.TestRouteRequest{Process: To("chrome.exe"), Ip: To(fakeIP.String())})
	if err != nil || response.GetError() != "" {
		t.Fatalf("response %v, err %v", response, err)
	}
	if got := viewRouteTestSteps(response.GetSteps()); !slices.Equal(got, []routeTestStepView{
		{index: 0, action: C.RuleActionTypeSniff, protocol: C.ProtocolTLS},
		{index: 1, action: C.RuleActionTypeRoute, matched: "domain_suffix=example.org"},
	}) {
		t.Errorf("steps = %+v", got)
	}
	if response.GetDestination() != "www.example.org:443" || response.GetInbound() != "mixed-in" || response.GetOutbound() != "proxy" ||
		!slices.Equal(response.GetOutboundChain(), []string{"inner", "direct"}) {
		t.Errorf("response = %v", response)
	}

	response, err = globalServer.TestRoute(context.Background(), &gen.TestRouteRequest{Domain: To("x.invalid")})
	if err != nil || response.GetAction() != "error" || len(response.GetSteps()) != 2 ||
		!strings.Contains(response.GetSteps()[1].GetError(), "DNS server not found: missing") {
		t.Errorf("response %v, err %v", response, err)
	}
}

func TestRouteTestServiceDLLHost(t *testing.T) {
	if got := routeTestProcessPaths(`C:\Program Files\App\app.exe`); !slices.Equal(got, []string{`C:\Program Files\App\app.exe`}) {
		t.Errorf("exe: %v", got)
	}
	if runtime.GOOS != "windows" {
		t.Skip("svchost services are Windows only")
	}
	// The DNS Client service runs in svchost on every desktop Windows.
	for _, dll := range []string{`C:\Windows\System32\dnsrslvr.dll`, "dnsrslvr.dll"} {
		got := routeTestProcessPaths(dll)
		if len(got) != 2 || got[0] != dll || !strings.EqualFold(filepath.Base(got[1]), "svchost.exe") {
			t.Errorf("%s: %v", dll, got)
		}
	}
}

func TestRouteTestNeedsARunningBox(t *testing.T) {
	if currentBox() != nil {
		t.Skip("a box is running")
	}
	response, err := globalServer.TestRoute(context.Background(), &gen.TestRouteRequest{Domain: To("example.org")})
	if err != nil || response.GetError() != errInstanceNotRunning.Error() {
		t.Errorf("response %v, err %v", response, err)
	}
}
