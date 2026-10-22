package rpc

import (
	"slices"
	"strings"
	"sync"

	"ThroneCore/internal/boxbox"

	"github.com/gofrs/uuid/v5"
	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/common/trafficcontrol"
	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/option"
	R "github.com/sagernet/sing-box/route/rule"
	"github.com/sagernet/sing/common/json/badoption"
)

// ruleEntry is one value of a route rule, matched by the very item sing-box builds for it.
type ruleEntry struct {
	key  string
	item R.RuleItem
}

// ruleSetEntry probes one set of a rule_set list, the way sing-box's own item probes each of its sets.
type ruleSetEntry struct {
	tag         string
	set         adapter.RuleSet
	matchSource bool
}

func (e ruleSetEntry) Match(metadata *adapter.InboundContext) bool {
	probe := *metadata
	probe.ResetRuleCache()
	probe.IPCIDRMatchSource = e.matchSource
	return e.set.Match(&probe)
}

func (e ruleSetEntry) String() string {
	return "rule_set=" + e.tag
}

// ruleEntries splits a default route rule into one entry per value, so the one that matched can be named:
// the rule's own String() keeps only the first three values of every list.
// Nil for a logical or inverted rule, which has no single entry to blame.
func ruleEntries(rule option.Rule, ruleSet func(tag string) (adapter.RuleSet, bool)) []ruleEntry {
	if rule.Type != "" && rule.Type != C.RuleTypeDefault {
		return nil
	}
	r := rule.DefaultOptions.RawDefaultRule
	if r.Invert {
		return nil
	}

	var entries []ruleEntry
	add := func(key string, item R.RuleItem, err error) {
		if err == nil {
			entries = append(entries, ruleEntry{key, item})
		}
	}
	for _, v := range r.Domain {
		item, err := R.NewDomainItem([]string{v}, nil)
		add("domain", item, err)
	}
	for _, v := range r.DomainSuffix {
		item, err := R.NewDomainItem(nil, []string{v})
		add("domain_suffix", item, err)
	}
	for _, v := range r.DomainKeyword {
		add("domain_keyword", R.NewDomainKeywordItem([]string{v}), nil)
	}
	for _, v := range r.DomainRegex {
		item, err := R.NewDomainRegexItem([]string{v})
		add("domain_regex", item, err)
	}
	for _, v := range r.IPCIDR {
		item, err := R.NewIPCIDRItem(false, []*badoption.Prefixable{v})
		add("ip_cidr", item, err)
	}
	if r.IPIsPrivate {
		add("ip_is_private", R.NewIPIsPrivateItem(false), nil)
	}
	for _, tag := range r.RuleSet {
		if set, found := ruleSet(tag); found {
			add("rule_set", ruleSetEntry{tag, set, r.RuleSetIPCIDRMatchSource}, nil)
		}
	}
	for _, v := range r.ProcessName {
		add("process_name", R.NewProcessItem([]string{v}), nil)
	}
	for _, v := range r.ProcessPath {
		add("process_path", R.NewProcessPathItem([]string{v}), nil)
	}
	for _, v := range r.ProcessPathRegex {
		item, err := R.NewProcessPathRegexItem([]string{v})
		add("process_path_regex", item, err)
	}
	return entries
}

// matchedEntries names the first matching value of every field, e.g. "domain_suffix=github.com process_name=chrome.exe".
func matchedEntries(entries []ruleEntry, metadata adapter.InboundContext) string {
	metadata.ResetRuleCache()
	var matched []string
	matchedKey := ""
	for _, entry := range entries {
		if entry.key == matchedKey || !entry.item.Match(&metadata) {
			continue
		}
		matched = append(matched, entry.item.String())
		matchedKey = entry.key
	}
	return strings.Join(matched, " ")
}

// connRules caches what QueryConnections reports as each connection's matched entries:
// a connection's rule and metadata never change, and a rule is split into entries once per box.
var connRules ruleMatchCache

type ruleMatchCache struct {
	mu      sync.Mutex
	box     *boxbox.Box
	rules   []option.Rule
	entries map[adapter.Rule][]ruleEntry
	conns   map[uuid.UUID]string
	seen    map[uuid.UUID]string
}

// reset ties the cache to box and the route rules its router was built from; a nil box lets a stopped one go.
func (m *ruleMatchCache) reset(box *boxbox.Box, rules []option.Rule) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.box, m.rules = box, rules
	m.entries, m.conns = make(map[adapter.Rule][]ruleEntry), make(map[uuid.UUID]string)
}

// begin opens one poll; matched must only be called between begin and end.
func (m *ruleMatchCache) begin(box *boxbox.Box) {
	m.mu.Lock()
	// A poll that read the box just before a restart replaced it has no rules to match against.
	if box != m.box {
		return
	}
	m.seen = make(map[uuid.UUID]string, len(m.conns))
}

func (m *ruleMatchCache) matched(c *trafficcontrol.TrackerMetadata) string {
	if c.Rule == nil || m.seen == nil {
		return ""
	}
	item, cached := m.conns[c.ID]
	if !cached {
		entries, split := m.entries[c.Rule]
		if !split {
			// The router builds one rule per config entry, in order.
			if i := slices.Index(m.box.Router().Rules(), c.Rule); i >= 0 && i < len(m.rules) {
				entries = ruleEntries(m.rules[i], m.box.Router().RuleSet)
			}
			m.entries[c.Rule] = entries
		}
		item = matchedEntries(entries, c.Metadata)
	}
	m.seen[c.ID] = item
	return item
}

// end keeps only the connections this poll reported, so the ones gone from the closed ring do not pile up.
func (m *ruleMatchCache) end() {
	if m.seen != nil {
		m.conns, m.seen = m.seen, nil
	}
	m.mu.Unlock()
}
