package rpc

import (
	"slices"
	"sync"

	"ThroneCore/internal/boxbox"
	"ThroneCore/internal/rulematch"

	"github.com/gofrs/uuid/v5"
	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/common/trafficcontrol"
	"github.com/sagernet/sing-box/option"
)

// connRules caches what QueryConnections reports as each connection's matched entries:
// a connection's rule and metadata never change, and a rule is split into entries once per box.
var connRules ruleMatchCache

type ruleMatchCache struct {
	mu      sync.Mutex
	box     *boxbox.Box
	rules   []option.Rule
	entries map[adapter.Rule][]rulematch.Entry
	conns   map[uuid.UUID]string
	seen    map[uuid.UUID]string
}

// reset ties the cache to box and the route rules its router was built from; a nil box lets a stopped one go.
func (m *ruleMatchCache) reset(box *boxbox.Box, rules []option.Rule) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.box, m.rules = box, rules
	m.entries, m.conns = make(map[adapter.Rule][]rulematch.Entry), make(map[uuid.UUID]string)
}

// routeRules gives the route rules box was built from, aligned with its router's; nil once another box replaced it.
func (m *ruleMatchCache) routeRules(box *boxbox.Box) []option.Rule {
	m.mu.Lock()
	defer m.mu.Unlock()
	if box != m.box {
		return nil
	}
	return m.rules
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
				entries = rulematch.Entries(m.rules[i], m.box.Router().RuleSet)
			}
			m.entries[c.Rule] = entries
		}
		item = rulematch.Matched(entries, c.Metadata)
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
