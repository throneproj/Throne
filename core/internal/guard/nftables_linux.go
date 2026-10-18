package guard

import (
	"net/netip"

	"github.com/sagernet/nftables"
	"github.com/sagernet/nftables/binaryutil"
	"github.com/sagernet/nftables/expr"
	"go4.org/netipx"
	"golang.org/x/sys/unix"
)

const ctLabelBit = 127

const ctDirectionReply = 1 // IP_CT_DIR_REPLY

type nftRuleset struct {
	table  *nftables.Table
	sets   []*nftables.Set
	elems  [][]nftables.SetElement
	chains []*nftables.Chain
	rules  []*nftables.Rule
}

type lanFamily struct {
	nfproto      byte
	saddr, daddr uint32
	addrLen      uint32
	set          *nftables.Set
}

func newRuleset(table *nftables.Table, cfg *config) (*nftRuleset, error) {
	r := &nftRuleset{table: table}
	var lan []lanFamily
	var dnsPorts *nftables.Set
	if cfg.AllowLAN {
		elems4, err := prefixElements(lanPrefixes4)
		if err != nil {
			return nil, err
		}
		elems6, err := prefixElements(lanPrefixes6)
		if err != nil {
			return nil, err
		}
		lan = []lanFamily{
			{unix.NFPROTO_IPV4, 12, 16, 4, r.addSet(&nftables.Set{ID: 1, Name: "lan4", KeyType: nftables.TypeIPAddr, Interval: true}, elems4)},
			{unix.NFPROTO_IPV6, 8, 24, 16, r.addSet(&nftables.Set{ID: 2, Name: "lan6", KeyType: nftables.TypeIP6Addr, Interval: true}, elems6)},
		}
		var ports []nftables.SetElement
		for _, port := range lanDNSPorts {
			ports = append(ports, nftables.SetElement{Key: binaryutil.BigEndian.PutUint16(port)})
		}
		dnsPorts = r.addSet(&nftables.Set{ID: 3, Name: "lan_dns_ports", KeyType: nftables.TypeInetService}, ports)
	}

	v4 := matchMeta(expr.MetaKeyNFPROTO, []byte{unix.NFPROTO_IPV4})
	v6 := matchMeta(expr.MetaKeyNFPROTO, []byte{unix.NFPROTO_IPV6})
	udp := matchMeta(expr.MetaKeyL4PROTO, []byte{unix.IPPROTO_UDP})
	icmpv6 := matchMeta(expr.MetaKeyL4PROTO, []byte{unix.IPPROTO_ICMPV6})

	// Postrouting, not output: output's oifname predates reroutes by NAT and route chains, such as auto_redirect's.
	post := r.addChain("postrouting", nftables.ChainHookPostrouting)
	r.accept(post, matchIfname(expr.MetaKeyOIFNAME, "lo"))
	// Forwarded packets reach postrouting too, so it repeats the forward chain's interface permits.
	r.interfaces(post, cfg)
	// skgid needs the socket file, gone after close() and in TIME_WAIT, so flows carry a ct label (auto_redirect owns whole ct marks).
	r.accept(post, matchLabel())
	r.accept(post, matchMeta(expr.MetaKeySKGID, binaryutil.NativeEndian.PutUint32(GID)), setLabel())
	r.accept(post, v4, udp, matchPort(0, 68), matchPort(2, 67))
	r.accept(post, v6, udp, matchPort(0, 546), matchPort(2, 547))
	r.accept(post, v6, icmpv6, []expr.Any{loadPayload(expr.PayloadBaseTransportHeader, 0, 1), &expr.Range{Op: expr.CmpOpEq, Register: 1, FromData: []byte{130}, ToData: []byte{137}}})
	r.accept(post, v6, icmpv6, []expr.Any{loadPayload(expr.PayloadBaseTransportHeader, 0, 1), cmpEq([]byte{143})})
	r.accept(post, v4, matchMeta(expr.MetaKeyL4PROTO, []byte{unix.IPPROTO_IGMP}))
	if cfg.APIPort != 0 {
		// ct direction reply means a peer opened the flow: no local program can originate traffic from the API port this way.
		r.accept(post, matchMeta(expr.MetaKeyL4PROTO, []byte{unix.IPPROTO_TCP}), matchPort(0, cfg.APIPort), matchReply())
	}
	// The tun permits must precede this: the tun subnet lies inside 172.16.0.0/12.
	r.lan(post, lan, dnsPorts, false)

	forward := r.addChain("forward", nftables.ChainHookForward)
	r.interfaces(forward, cfg)
	r.lan(forward, lan, dnsPorts, true)
	return r, nil
}

func (r *nftRuleset) queue(conn *nftables.Conn) error {
	conn.CreateTable(r.table)
	for i, set := range r.sets {
		if err := conn.AddSet(set, r.elems[i]); err != nil {
			return err
		}
	}
	for _, chain := range r.chains {
		conn.AddChain(chain)
	}
	for _, rule := range r.rules {
		conn.AddRule(rule)
	}
	return nil
}

func (r *nftRuleset) addSet(set *nftables.Set, elems []nftables.SetElement) *nftables.Set {
	set.Table = r.table
	r.sets = append(r.sets, set)
	r.elems = append(r.elems, elems)
	return set
}

func (r *nftRuleset) addChain(name string, hook *nftables.ChainHook) *nftables.Chain {
	policy := nftables.ChainPolicyDrop
	chain := &nftables.Chain{
		Name:     name,
		Table:    r.table,
		Type:     nftables.ChainTypeFilter,
		Hooknum:  hook,
		Priority: nftables.ChainPriorityFilter,
		Policy:   &policy,
	}
	r.chains = append(r.chains, chain)
	return chain
}

func (r *nftRuleset) accept(chain *nftables.Chain, matches ...[]expr.Any) {
	r.add(chain, expr.VerdictAccept, matches)
}

func (r *nftRuleset) drop(chain *nftables.Chain, matches ...[]expr.Any) {
	r.add(chain, expr.VerdictDrop, matches)
}

func (r *nftRuleset) add(chain *nftables.Chain, verdict expr.VerdictKind, matches [][]expr.Any) {
	var exprs []expr.Any
	for _, match := range matches {
		exprs = append(exprs, match...)
	}
	exprs = append(exprs, &expr.Verdict{Kind: verdict})
	r.rules = append(r.rules, &nftables.Rule{Table: r.table, Chain: chain, Exprs: exprs})
}

func (r *nftRuleset) interfaces(chain *nftables.Chain, cfg *config) {
	if cfg.TunName != "" {
		r.accept(chain, matchIfname(expr.MetaKeyIIFNAME, cfg.TunName))
		r.accept(chain, matchIfname(expr.MetaKeyOIFNAME, cfg.TunName))
	}
	if cfg.BridgeName != "" {
		// No NUL: the cmp covers only the prefix, and the bridge tun is BridgeName plus an index.
		r.accept(chain, matchMeta(expr.MetaKeyIIFNAME, []byte(cfg.BridgeName)))
		r.accept(chain, matchMeta(expr.MetaKeyOIFNAME, []byte(cfg.BridgeName)))
	}
}

func (r *nftRuleset) lan(chain *nftables.Chain, families []lanFamily, dnsPorts *nftables.Set, forwarded bool) {
	for _, family := range families {
		for _, proto := range []byte{unix.IPPROTO_TCP, unix.IPPROTO_UDP} {
			r.drop(chain,
				matchMeta(expr.MetaKeyNFPROTO, []byte{family.nfproto}),
				matchMeta(expr.MetaKeyL4PROTO, []byte{proto}),
				matchSet(expr.PayloadBaseTransportHeader, 2, 2, dnsPorts),
				matchSet(expr.PayloadBaseNetworkHeader, family.daddr, family.addrLen, family.set))
		}
	}
	for _, family := range families {
		matches := [][]expr.Any{matchMeta(expr.MetaKeyNFPROTO, []byte{family.nfproto})}
		if forwarded {
			matches = append(matches, matchSet(expr.PayloadBaseNetworkHeader, family.saddr, family.addrLen, family.set))
		}
		matches = append(matches, matchSet(expr.PayloadBaseNetworkHeader, family.daddr, family.addrLen, family.set))
		r.accept(chain, matches...)
	}
}

func prefixElements(prefixes []netip.Prefix) ([]nftables.SetElement, error) {
	var builder netipx.IPSetBuilder
	for _, prefix := range prefixes {
		builder.AddPrefix(prefix)
	}
	set, err := builder.IPSet()
	if err != nil {
		return nil, err
	}
	var elems []nftables.SetElement
	for _, rng := range set.Ranges() {
		elems = append(elems, nftables.SetElement{Key: rng.From().AsSlice()})
		// A range ending at the family's last address has no end key; nft leaves such an interval open too.
		if end := rng.To().Next(); end.IsValid() {
			elems = append(elems, nftables.SetElement{Key: end.AsSlice(), IntervalEnd: true})
		}
	}
	return elems, nil
}

func matchMeta(key expr.MetaKey, data []byte) []expr.Any {
	return []expr.Any{&expr.Meta{Key: key, Register: 1}, cmpEq(data)}
}

func matchIfname(key expr.MetaKey, name string) []expr.Any {
	data := make([]byte, unix.IFNAMSIZ)
	copy(data, name)
	return matchMeta(key, data)
}

func matchPort(offset uint32, port uint16) []expr.Any {
	return []expr.Any{loadPayload(expr.PayloadBaseTransportHeader, offset, 2), cmpEq(binaryutil.BigEndian.PutUint16(port))}
}

func matchSet(base expr.PayloadBase, offset, length uint32, set *nftables.Set) []expr.Any {
	return []expr.Any{loadPayload(base, offset, length), &expr.Lookup{SourceRegister: 1, SetName: set.Name, SetID: set.ID}}
}

func matchReply() []expr.Any {
	return []expr.Any{&expr.Ct{Key: expr.CtKeyDIRECTION, Register: 1}, cmpEq([]byte{ctDirectionReply})}
}

func matchLabel() []expr.Any {
	return []expr.Any{
		&expr.Ct{Key: expr.CtKeyLABELS, Register: 1},
		&expr.Bitwise{SourceRegister: 1, DestRegister: 1, Len: 16, Mask: ctLabel(), Xor: make([]byte, 16)},
		&expr.Cmp{Op: expr.CmpOpNeq, Register: 1, Data: make([]byte, 16)},
	}
}

func setLabel() []expr.Any {
	return []expr.Any{
		&expr.Immediate{Register: 1, Data: ctLabel()},
		&expr.Ct{Key: expr.CtKeyLABELS, Register: 1, SourceRegister: true},
	}
}

func ctLabel() []byte {
	label := make([]byte, 16)
	label[ctLabelBit/8] = 1 << (ctLabelBit % 8)
	return label
}

func loadPayload(base expr.PayloadBase, offset, length uint32) *expr.Payload {
	return &expr.Payload{OperationType: expr.PayloadLoad, DestRegister: 1, Base: base, Offset: offset, Len: length}
}

func cmpEq(data []byte) *expr.Cmp {
	return &expr.Cmp{Op: expr.CmpOpEq, Register: 1, Data: data}
}
