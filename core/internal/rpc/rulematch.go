package rpc

import (
	"net/netip"
	"path/filepath"
	"regexp"
	"slices"
	"strings"

	"github.com/sagernet/sing-box/adapter"
	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/option"
	N "github.com/sagernet/sing/common/network"
)

// matchedRuleIndex finds the matched rule among the router's, which are built one per config rule and in the same order.
func matchedRuleIndex(router adapter.Router, rule adapter.Rule) int {
	if router == nil || rule == nil {
		return -1
	}
	return slices.IndexFunc(router.Rules(), func(it adapter.Rule) bool { return it == rule })
}

// matchedRuleItem names the entries of a route rule that caught the connection: the rule's own String()
// keeps only the first three entries of every list, which rarely include the one that matched.
// Empty when the rule is logical or inverted, or no entry could be told apart.
func matchedRuleItem(rule option.Rule, metadata adapter.InboundContext, ruleSet func(tag string) (adapter.RuleSet, bool)) string {
	if rule.Type != "" && rule.Type != C.RuleTypeDefault {
		return ""
	}
	r := rule.DefaultOptions.RawDefaultRule
	if r.Invert {
		return ""
	}

	var items []string
	add := func(key string, values []string, match func(string) bool) {
		if i := slices.IndexFunc(values, match); i >= 0 {
			items = append(items, key+"="+values[i])
		}
	}

	host := metadata.Domain
	if host == "" {
		host = metadata.Destination.Fqdn
	}
	host = strings.ToLower(host)
	if host != "" {
		add("domain", r.Domain, func(v string) bool { return host == strings.ToLower(v) })
		add("domain_suffix", r.DomainSuffix, func(v string) bool {
			v = strings.ToLower(v)
			if strings.HasPrefix(v, ".") {
				return strings.HasSuffix(host, v)
			}
			return host == v || strings.HasSuffix(host, "."+v)
		})
		add("domain_keyword", r.DomainKeyword, func(v string) bool { return strings.Contains(host, strings.ToLower(v)) })
		add("domain_regex", r.DomainRegex, func(v string) bool {
			re, err := regexp.Compile(v)
			return err == nil && re.MatchString(host)
		})
	}

	var addrs []netip.Addr
	if metadata.Destination.IsIP() {
		addrs = []netip.Addr{metadata.Destination.Addr}
	} else {
		addrs = metadata.DestinationAddresses
	}
	if len(addrs) > 0 {
		for _, it := range r.IPCIDR {
			if prefix := (*netip.Prefix)(it); prefix != nil && slices.ContainsFunc(addrs, prefix.Contains) {
				items = append(items, "ip_cidr="+prefix.String())
				break
			}
		}
		if r.IPIsPrivate && slices.ContainsFunc(addrs, func(a netip.Addr) bool { return !N.IsPublicAddr(a) }) {
			items = append(items, "ip_is_private")
		}
	}

	if ruleSet != nil {
		add("rule_set", r.RuleSet, func(tag string) bool {
			set, found := ruleSet(tag)
			if !found {
				return false
			}
			// A copy, so the set's match bookkeeping never reaches the tracker's metadata.
			probe := metadata
			probe.ResetRuleMatchCache()
			probe.IPCIDRMatchSource = r.RuleSetIPCIDRMatchSource
			probe.IPCIDRAcceptEmpty = false
			return set.Match(&probe)
		})
	}

	if metadata.ProcessInfo != nil {
		paths := metadata.ProcessInfo.ProcessPaths
		add("process_name", r.ProcessName, func(v string) bool {
			return slices.ContainsFunc(paths, func(p string) bool { return filepath.Base(p) == v })
		})
		add("process_path", r.ProcessPath, func(v string) bool { return slices.Contains(paths, v) })
		add("process_path_regex", r.ProcessPathRegex, func(v string) bool {
			re, err := regexp.Compile(v)
			return err == nil && slices.ContainsFunc(paths, re.MatchString)
		})
	}

	return strings.Join(items, " ")
}
