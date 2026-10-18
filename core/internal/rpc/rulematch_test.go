package rpc

import (
	"net/netip"
	"testing"

	"github.com/sagernet/sing-box/adapter"
	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing/common/json/badoption"
	M "github.com/sagernet/sing/common/metadata"
)

type fakeRuleSet struct {
	adapter.RuleSet
	match func(*adapter.InboundContext) bool
}

func (s fakeRuleSet) Match(metadata *adapter.InboundContext) bool { return s.match(metadata) }

func TestMatchedEntries(t *testing.T) {
	telegram := netip.MustParsePrefix("149.154.160.0/20")
	ruleSets := map[string]adapter.RuleSet{
		"geosite-empty": fakeRuleSet{match: func(*adapter.InboundContext) bool { return false }},
		"geoip-telegram": fakeRuleSet{match: func(m *adapter.InboundContext) bool {
			return m.Destination.IsIP() && telegram.Contains(m.Destination.Addr)
		}},
	}
	lookup := func(tag string) (adapter.RuleSet, bool) {
		set, found := ruleSets[tag]
		return set, found
	}
	rule := func(raw option.RawDefaultRule) option.Rule {
		return option.Rule{Type: C.RuleTypeDefault, DefaultOptions: option.DefaultRule{RawDefaultRule: raw}}
	}
	domain := func(host string) adapter.InboundContext {
		return adapter.InboundContext{Domain: host, Destination: M.ParseSocksaddrHostPort("1.2.3.4", 443)}
	}
	cidr := badoption.Prefixable(netip.MustParsePrefix("10.0.0.0/8"))
	proxyList := option.RawDefaultRule{
		Domain:        badoption.Listable[string]{"example.org"},
		DomainSuffix:  badoption.Listable[string]{"a.com", "b.com", "c.com", "githubusercontent.com"},
		DomainKeyword: badoption.Listable[string]{"twitch"},
		IPCIDR:        badoption.Listable[*badoption.Prefixable]{&cidr},
		RuleSet:       badoption.Listable[string]{"geosite-empty", "geoip-telegram"},
	}

	for _, tc := range []struct {
		name     string
		rule     option.Rule
		metadata adapter.InboundContext
		want     string
	}{
		{"suffix past the three String() keeps", rule(proxyList), domain("private-user-images.githubusercontent.com"), "domain_suffix=githubusercontent.com"},
		{"suffix matches the bare domain", rule(proxyList), domain("githubusercontent.com"), "domain_suffix=githubusercontent.com"},
		{"suffix does not match a longer label", rule(proxyList), domain("notgithubusercontent.com"), ""},
		{"exact domain", rule(proxyList), domain("example.org"), "domain=example.org"},
		{"keyword", rule(proxyList), domain("gql.twitch.tv"), "domain_keyword=twitch"},
		{"ip_cidr", rule(proxyList), adapter.InboundContext{Destination: M.ParseSocksaddrHostPort("10.1.2.3", 80)}, "ip_cidr=10.0.0.0/8"},
		{"rule_set picks the set that matched", rule(proxyList), adapter.InboundContext{Destination: M.ParseSocksaddrHostPort("149.154.167.51", 443)}, "rule_set=geoip-telegram"},
		{"dot suffix skips the bare domain", rule(option.RawDefaultRule{DomainSuffix: badoption.Listable[string]{".github.com"}}), domain("github.com"), ""},
		// sing-box lowercases the host but takes values as written, so such a value never matches.
		{"uppercase value never matches", rule(option.RawDefaultRule{DomainSuffix: badoption.Listable[string]{"GitHub.com", "github.com"}}), domain("API.github.com"), "domain_suffix=github.com"},
		{
			"every matched field is listed, each once",
			rule(option.RawDefaultRule{DomainSuffix: badoption.Listable[string]{"github.com", "api.github.com"}, ProcessName: badoption.Listable[string]{"chrome.exe"}}),
			adapter.InboundContext{Domain: "api.github.com", ProcessInfo: &adapter.ConnectionOwner{ProcessPaths: []string{"/opt/google/chrome/chrome.exe"}}},
			"domain_suffix=github.com process_name=chrome.exe",
		},
		{"inverted rule is left to String()", rule(option.RawDefaultRule{DomainSuffix: badoption.Listable[string]{"github.com"}, Invert: true}), domain("api.github.com"), ""},
		{"logical rule is left to String()", option.Rule{Type: C.RuleTypeLogical}, domain("api.github.com"), ""},
	} {
		t.Run(tc.name, func(t *testing.T) {
			if got := matchedEntries(ruleEntries(tc.rule, lookup), tc.metadata); got != tc.want {
				t.Errorf("matchedEntries() = %q, want %q", got, tc.want)
			}
		})
	}
}
