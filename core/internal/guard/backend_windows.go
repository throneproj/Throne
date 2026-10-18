package guard

import (
	"cmp"
	"encoding/binary"
	"fmt"
	"log"
	"net/netip"
	"os"
	"runtime"
	"slices"
	"strconv"
	"strings"
	"sync"
	"time"
	"unsafe"

	"ThroneCore/internal/winipcfg"

	"golang.org/x/sys/windows"
)

const (
	weightApp      = 15
	weightTun      = 14
	weightLoopback = 13
	weightLANDNS   = 12
	weightLAN      = 11
	weightLink     = 10
	weightAPI      = 9
	weightBlock    = 0
)

const (
	ipProtoIGMP   = 2
	ipProtoTCP    = 6
	ipProtoUDP    = 17
	ipProtoICMPv6 = 58
)

const tamperedMessage = "the kill switch firewall rules were removed (the Base Filtering Engine restarted or another program deleted them)"

const (
	errRPCServerUnavailable  = windows.Errno(1722)
	errEndpointNotRegistered = windows.Errno(1753)
)

var (
	aleLayers     = []windows.GUID{layerALEAuthConnectV4, layerALEAuthRecvAcceptV4, layerALEAuthConnectV6, layerALEAuthRecvAcceptV6}
	forwardLayers = []windows.GUID{layerIPForwardV4, layerIPForwardV6}
)

type wfpRule struct {
	name       string
	layers     []windows.GUID
	weight     uint8
	action     uint32
	conditions []fwpmFilterCondition0
}

type tunAdapter struct {
	luid  uint64
	index uint32
}

type wfpBackend struct {
	tunName  string
	stop     chan struct{}
	stopOnce sync.Once
	wg       sync.WaitGroup

	// access guards engine and every field below it.
	access     sync.Mutex
	engine     uintptr
	provider   windows.GUID
	sublayer   windows.GUID
	blockAll   []uint64
	tuns       []tunAdapter
	tunFilters []uint64
}

func newBackend() (backend, error) {
	if !windows.GetCurrentProcessToken().IsElevated() {
		return nil, newError(CodePrivilege, "the kill switch needs Throne to run as administrator")
	}
	return &wfpBackend{stop: make(chan struct{})}, nil
}

func (b *wfpBackend) Arm(cfg *config, events chan<- Event) error {
	appIDs, skipped, err := resolveAppIDs(cfg.ExtraPaths)
	defer func() {
		for i := range appIDs {
			fwpmFreeMemory0(unsafe.Pointer(&appIDs[i]))
		}
	}()
	if err != nil {
		return err
	}
	var tuns []tunAdapter
	b.tunName = cfg.TunName
	if b.tunName != "" {
		if tuns, err = findTuns(b.tunName); err != nil {
			return err
		}
	}
	if err = b.arm(cfg, tuns, appIDs); err != nil {
		return err
	}
	if len(skipped) > 0 {
		select {
		case events <- Event{Message: "the kill switch cannot exempt " + strings.Join(skipped, "; ")}:
		default:
		}
	}
	b.wg.Add(1)
	goSafe(events, func() {
		defer b.wg.Done()
		b.watchdog(events)
	})
	if b.tunName != "" {
		b.wg.Add(1)
		goSafe(events, func() {
			defer b.wg.Done()
			watchInterfaces(b.stop, 5*time.Second, b.recheck)
		})
	}
	return nil
}

func (b *wfpBackend) arm(cfg *config, tuns []tunAdapter, appIDs []*fwpByteBlob) error {
	b.access.Lock()
	defer b.access.Unlock()
	session := fwpmSession0{
		displayData:          displayData("Throne kill switch"),
		flags:                fwpmSessionFlagDynamic,
		txnWaitTimeoutInMSec: 10 * 1000,
	}
	var engine uintptr
	if err := fwpmEngineOpen0(&session, &engine); err != nil {
		switch err {
		case windows.ERROR_ACCESS_DENIED:
			return newError(CodePrivilege, "the filtering engine refused access: %v", err)
		case errEndpointNotRegistered, errRPCServerUnavailable:
			return newError(CodeUnsupported, "the Base Filtering Engine (BFE) service is not running: %v", err)
		}
		return fmt.Errorf("open the filtering engine: %w", err)
	}
	b.engine, b.tuns = engine, tuns
	var pin runtime.Pinner
	defer pin.Unpin()
	err := b.transaction(func() (err error) {
		if err = b.addSublayer(); err != nil {
			return err
		}
		if _, err = b.install(baseRules(cfg, appIDs, &pin)); err != nil {
			return err
		}
		if b.blockAll, err = b.install(blockRules()); err != nil {
			return err
		}
		b.tunFilters, err = b.install(tunRules(tuns, &pin))
		return err
	})
	if err != nil {
		b.closeEngine()
		return fmt.Errorf("install the kill switch rules: %w", err)
	}
	if b.tunName != "" {
		b.logTun()
	}
	return nil
}

func (b *wfpBackend) Disarm() {
	b.stopOnce.Do(func() { close(b.stop) })
	b.wg.Wait()
	b.access.Lock()
	defer b.access.Unlock()
	b.closeEngine()
}

// Closing a dynamic session makes the Base Filtering Engine delete every object added through it.
func (b *wfpBackend) closeEngine() {
	if b.engine == 0 {
		return
	}
	if err := fwpmEngineClose0(b.engine); err != nil {
		log.Printf("guard: close the filtering engine: %v", err)
	}
	b.engine = 0
}

func (b *wfpBackend) transaction(fn func() error) error {
	if err := fwpmTransactionBegin0(b.engine); err != nil {
		return fmt.Errorf("begin transaction: %w", err)
	}
	if err := fn(); err != nil {
		_ = fwpmTransactionAbort0(b.engine)
		return err
	}
	if err := fwpmTransactionCommit0(b.engine); err != nil {
		_ = fwpmTransactionAbort0(b.engine)
		return fmt.Errorf("commit transaction: %w", err)
	}
	return nil
}

func (b *wfpBackend) addSublayer() error {
	var err error
	if b.provider, err = windows.GenerateGUID(); err != nil {
		return err
	}
	if b.sublayer, err = windows.GenerateGUID(); err != nil {
		return err
	}
	provider := fwpmProvider0{providerKey: b.provider, displayData: displayData("Throne")}
	if err = fwpmProviderAdd0(b.engine, &provider); err != nil {
		return fmt.Errorf("add provider: %w", err)
	}
	sublayer := fwpmSublayer0{
		subLayerKey: b.sublayer,
		displayData: displayData("Throne kill switch"),
		providerKey: &b.provider,
		weight:      0xFFFF,
	}
	if err = fwpmSubLayerAdd0(b.engine, &sublayer); err != nil {
		return fmt.Errorf("add sublayer: %w", err)
	}
	return nil
}

func (b *wfpBackend) install(rules []wfpRule) ([]uint64, error) {
	var ids []uint64
	for _, rule := range rules {
		for _, layer := range rule.layers {
			id, err := b.addFilter(rule, layer)
			if err != nil {
				return nil, fmt.Errorf("add filter %q: %w", rule.name, err)
			}
			ids = append(ids, id)
		}
	}
	return ids, nil
}

func (b *wfpBackend) addFilter(rule wfpRule, layer windows.GUID) (uint64, error) {
	filter := fwpmFilter0{
		displayData: displayData("Throne: " + rule.name),
		providerKey: &b.provider,
		layerKey:    layer,
		subLayerKey: b.sublayer,
		weight:      fwpValue0{typ: fwpUint8, value: uintptr(rule.weight)},
		action:      fwpmAction0{typ: rule.action},
	}
	if len(rule.conditions) > 0 {
		filter.numFilterConditions = uint32(len(rule.conditions))
		filter.filterCondition = &rule.conditions[0]
	}
	var id uint64
	err := fwpmFilterAdd0(b.engine, &filter, &id)
	runtime.KeepAlive(rule.conditions)
	return id, err
}

func (b *wfpBackend) recheck() {
	tuns, err := findTuns(b.tunName)
	if err != nil {
		log.Printf("guard: %v", err)
		return
	}
	b.access.Lock()
	defer b.access.Unlock()
	if slices.Equal(tuns, b.tuns) {
		return
	}
	var pin runtime.Pinner
	defer pin.Unpin()
	var ids []uint64
	err = b.transaction(func() (err error) {
		if ids, err = b.install(tunRules(tuns, &pin)); err != nil {
			return err
		}
		for _, id := range b.tunFilters {
			if err = fwpmFilterDeleteById0(b.engine, id); err != nil {
				return fmt.Errorf("delete filter %d: %w", id, err)
			}
		}
		return nil
	})
	if err != nil {
		log.Printf("guard: update the tun permits: %v", err)
		return
	}
	b.tuns, b.tunFilters = tuns, ids
	b.logTun()
}

func (b *wfpBackend) logTun() {
	if len(b.tuns) == 0 {
		log.Printf("guard: tun %s is absent", b.tunName)
	}
	for _, tun := range b.tuns {
		log.Printf("guard: tun %s is LUID %#x, index %d", b.tunName, tun.luid, tun.index)
	}
}

func (b *wfpBackend) watchdog(events chan<- Event) {
	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()
	for {
		select {
		case <-b.stop:
			return
		case <-ticker.C:
		}
		if err := b.verify(); err != nil {
			log.Printf("guard: %v", err)
			select {
			case events <- Event{Fatal: true, Code: CodeTampered, Message: tamperedMessage}:
			case <-b.stop:
			}
			return
		}
	}
}

func (b *wfpBackend) verify() error {
	b.access.Lock()
	defer b.access.Unlock()
	for _, id := range b.blockAll {
		var filter *fwpmFilter0
		if err := fwpmFilterGetById0(b.engine, id, &filter); err != nil {
			return fmt.Errorf("look up block filter %d: %w", id, err)
		}
		fwpmFreeMemory0(unsafe.Pointer(&filter))
	}
	return nil
}

func resolveAppIDs(extraPaths []string) (appIDs []*fwpByteBlob, skipped []string, err error) {
	self, err := os.Executable()
	if err != nil {
		return nil, nil, fmt.Errorf("locate the ThroneCore executable: %w", err)
	}
	appID, err := appIDFromFile(self)
	if err != nil {
		return nil, nil, fmt.Errorf("resolve the app ID of %s: %w", self, err)
	}
	appIDs = append(appIDs, appID)
	for _, path := range extraPaths {
		if appID, err = appIDFromFile(path); err != nil {
			skipped = append(skipped, fmt.Sprintf("%s (%v)", path, err))
			continue
		}
		appIDs = append(appIDs, appID)
	}
	return appIDs, skipped, nil
}

func appIDFromFile(path string) (*fwpByteBlob, error) {
	name, err := windows.UTF16PtrFromString(path)
	if err != nil {
		return nil, err
	}
	var appID *fwpByteBlob
	if err = fwpmGetAppIdFromFileName0(name, &appID); err != nil {
		return nil, err
	}
	return appID, nil
}

// Matched by alias only: matching the tun's address would also permit a LAN that reuses its range.
func findTuns(name string) ([]tunAdapter, error) {
	rows, err := winipcfg.GetIfTable2Ex(winipcfg.MibIfEntryNormal)
	if err != nil {
		return nil, fmt.Errorf("list network interfaces: %w", err)
	}
	var tuns []tunAdapter
	for i := range rows {
		if isTunAlias(rows[i].Alias(), name) {
			tuns = append(tuns, tunAdapter{luid: uint64(rows[i].InterfaceLUID), index: rows[i].InterfaceIndex})
		}
	}
	slices.SortFunc(tuns, func(a, b tunAdapter) int { return cmp.Compare(a.luid, b.luid) })
	return tuns, nil
}

// Windows names a new adapter "throne-tun 2" while a stale instance still holds the plain name.
func isTunAlias(alias, name string) bool {
	if len(alias) < len(name) || !strings.EqualFold(alias[:len(name)], name) {
		return false
	}
	if len(alias) == len(name) {
		return true
	}
	number, ok := strings.CutPrefix(alias[len(name):], " ")
	_, err := strconv.ParseUint(number, 10, 16)
	return ok && err == nil
}

func baseRules(cfg *config, appIDs []*fwpByteBlob, pin *runtime.Pinner) []wfpRule {
	apps := make([]fwpmFilterCondition0, len(appIDs))
	for i, appID := range appIDs {
		apps[i] = condition(conditionALEAppID, fwpMatchEqual, fwpByteBlobType, uintptr(unsafe.Pointer(appID)))
	}
	rules := []wfpRule{
		{"permit Throne", aleLayers, weightApp, fwpActionPermit, apps},
		{"permit loopback", aleLayers, weightLoopback, fwpActionPermit, []fwpmFilterCondition0{
			condition(conditionFlags, fwpMatchFlagsAllSet, fwpUint32, fwpConditionFlagIsLoopback),
		}},
	}
	if cfg.APIPort != 0 {
		// ALE authorizes the whole flow: an accepted connection's replies never reach the connect layer's block.
		rules = append(rules, wfpRule{"permit the Throne API", []windows.GUID{layerALEAuthRecvAcceptV4, layerALEAuthRecvAcceptV6}, weightAPI, fwpActionPermit, []fwpmFilterCondition0{
			equal8(conditionIPProtocol, ipProtoTCP), equal16(conditionIPLocalPort, cfg.APIPort),
		}})
	}
	if cfg.AllowLAN {
		rules = append(rules, lanRules(pin)...)
	}
	return append(rules, linkRules(pin)...)
}

func lanRules(pin *runtime.Pinner) []wfpRule {
	dns := []fwpmFilterCondition0{equal8(conditionIPProtocol, ipProtoTCP), equal8(conditionIPProtocol, ipProtoUDP)}
	for _, port := range lanDNSPorts {
		dns = append(dns, equal16(conditionIPRemotePort, port))
	}
	var rules []wfpRule
	for _, family := range []struct {
		prefixes                 []netip.Prefix
		connect, accept, forward windows.GUID
	}{
		{lanPrefixes4, layerALEAuthConnectV4, layerALEAuthRecvAcceptV4, layerIPForwardV4},
		{lanPrefixes6, layerALEAuthConnectV6, layerALEAuthRecvAcceptV6, layerIPForwardV6},
	} {
		remote := inPrefixes(pin, conditionIPRemoteAddress, family.prefixes)
		rules = append(rules,
			wfpRule{"block DNS to the LAN", []windows.GUID{family.connect}, weightLANDNS, fwpActionBlock, slices.Concat(remote, dns)},
			wfpRule{"permit LAN", []windows.GUID{family.connect, family.accept}, weightLAN, fwpActionPermit, remote},
			wfpRule{"permit forwarding within the LAN", []windows.GUID{family.forward}, weightLAN, fwpActionPermit, slices.Concat(
				inPrefixes(pin, conditionIPSourceAddress, family.prefixes),
				inPrefixes(pin, conditionIPDestinationAddress, family.prefixes),
			)},
		)
	}
	return rules
}

func linkRules(pin *runtime.Pinner) []wfpRule {
	udp := equal8(conditionIPProtocol, ipProtoUDP)
	icmp6 := equal8(conditionIPProtocol, ipProtoICMPv6)
	linkLocal := netip.MustParsePrefix("fe80::/10")
	remoteLinkLocal := inPrefix(pin, conditionIPRemoteAddress, linkLocal)
	localLinkLocal := inPrefix(pin, conditionIPLocalAddress, linkLocal)
	ndp := func(icmpType uint16, extra ...fwpmFilterCondition0) []fwpmFilterCondition0 {
		return append([]fwpmFilterCondition0{icmp6, equal16(conditionICMPType, icmpType), equal16(conditionICMPCode, 0)}, extra...)
	}
	connect4 := []windows.GUID{layerALEAuthConnectV4}
	accept4 := []windows.GUID{layerALEAuthRecvAcceptV4}
	connect6 := []windows.GUID{layerALEAuthConnectV6}
	accept6 := []windows.GUID{layerALEAuthRecvAcceptV6}
	both6 := []windows.GUID{layerALEAuthConnectV6, layerALEAuthRecvAcceptV6}
	return []wfpRule{
		{"permit DHCP", connect4, weightLink, fwpActionPermit, []fwpmFilterCondition0{
			udp, equal16(conditionIPLocalPort, 68), equal16(conditionIPRemotePort, 67), equal32(conditionIPRemoteAddress, 0xffffffff),
		}},
		{"permit DHCP", accept4, weightLink, fwpActionPermit, []fwpmFilterCondition0{
			udp, equal16(conditionIPLocalPort, 68), equal16(conditionIPRemotePort, 67),
		}},
		// Mobile Hotspot's DHCP server; without the LAN address match anything bound to port 67 could reach the internet.
		{"permit DHCP server", connect4, weightLink, fwpActionPermit, slices.Concat(
			[]fwpmFilterCondition0{udp, equal16(conditionIPLocalPort, 67), equal16(conditionIPRemotePort, 68)},
			inPrefixes(pin, conditionIPRemoteAddress, lanPrefixes4),
		)},
		{"permit DHCP server", accept4, weightLink, fwpActionPermit, []fwpmFilterCondition0{
			udp, equal16(conditionIPLocalPort, 67), equal16(conditionIPRemotePort, 68),
		}},
		{"permit DHCPv6", connect6, weightLink, fwpActionPermit, []fwpmFilterCondition0{
			udp,
			equalAddr6(pin, conditionIPRemoteAddress, netip.MustParseAddr("ff02::1:2")),
			equalAddr6(pin, conditionIPRemoteAddress, netip.MustParseAddr("ff05::1:3")),
			equal16(conditionIPRemotePort, 547), localLinkLocal, equal16(conditionIPLocalPort, 546),
		}},
		{"permit DHCPv6", accept6, weightLink, fwpActionPermit, []fwpmFilterCondition0{
			udp, remoteLinkLocal, equal16(conditionIPRemotePort, 547), localLinkLocal, equal16(conditionIPLocalPort, 546),
		}},
		{"permit NDP", connect6, weightLink, fwpActionPermit, ndp(133, equalAddr6(pin, conditionIPRemoteAddress, netip.MustParseAddr("ff02::2")))},
		{"permit NDP", accept6, weightLink, fwpActionPermit, ndp(134, remoteLinkLocal)},
		{"permit NDP", both6, weightLink, fwpActionPermit, ndp(135)},
		{"permit NDP", both6, weightLink, fwpActionPermit, ndp(136)},
		{"permit NDP", accept6, weightLink, fwpActionPermit, ndp(137, remoteLinkLocal)},
		{"permit MLD", connect6, weightLink, fwpActionPermit, []fwpmFilterCondition0{
			icmp6, equal16(conditionICMPType, 130), equal16(conditionICMPType, 131), equal16(conditionICMPType, 132), equal16(conditionICMPType, 143),
		}},
		{"permit IGMP", connect4, weightLink, fwpActionPermit, []fwpmFilterCondition0{equal8(conditionIPProtocol, ipProtoIGMP)}},
	}
}

func blockRules() []wfpRule {
	return []wfpRule{{"block everything else", slices.Concat(aleLayers, forwardLayers), weightBlock, fwpActionBlock, nil}}
}

func tunRules(tuns []tunAdapter, pin *runtime.Pinner) []wfpRule {
	if len(tuns) == 0 {
		return nil
	}
	var local, from, to []fwpmFilterCondition0
	for _, tun := range tuns {
		local = append(local, condition(conditionIPLocalInterface, fwpMatchEqual, fwpUint64, pinned(pin, tun.luid)))
		from = append(from, equal32(conditionSourceInterfaceIndex, tun.index))
		to = append(to, equal32(conditionDestinationInterfaceIndex, tun.index))
	}
	return []wfpRule{
		{"permit tun", aleLayers, weightTun, fwpActionPermit, local},
		{"permit forwarding from tun", forwardLayers, weightTun, fwpActionPermit, from},
		{"permit forwarding to tun", forwardLayers, weightTun, fwpActionPermit, to},
	}
}

func condition(field windows.GUID, match, typ uint32, value uintptr) fwpmFilterCondition0 {
	return fwpmFilterCondition0{fieldKey: field, matchType: match, conditionValue: fwpValue0{typ: typ, value: value}}
}

func equal8(field windows.GUID, v uint8) fwpmFilterCondition0 {
	return condition(field, fwpMatchEqual, fwpUint8, uintptr(v))
}

func equal16(field windows.GUID, v uint16) fwpmFilterCondition0 {
	return condition(field, fwpMatchEqual, fwpUint16, uintptr(v))
}

func equal32(field windows.GUID, v uint32) fwpmFilterCondition0 {
	return condition(field, fwpMatchEqual, fwpUint32, uintptr(v))
}

func equalAddr6(pin *runtime.Pinner, field windows.GUID, addr netip.Addr) fwpmFilterCondition0 {
	return condition(field, fwpMatchEqual, fwpByteArray16Type, pinned(pin, addr.As16()))
}

func inPrefixes(pin *runtime.Pinner, field windows.GUID, prefixes []netip.Prefix) []fwpmFilterCondition0 {
	conditions := make([]fwpmFilterCondition0, len(prefixes))
	for i, prefix := range prefixes {
		conditions[i] = inPrefix(pin, field, prefix)
	}
	return conditions
}

func inPrefix(pin *runtime.Pinner, field windows.GUID, prefix netip.Prefix) fwpmFilterCondition0 {
	prefix = prefix.Masked()
	if prefix.Addr().Is4() {
		addr := prefix.Addr().As4()
		mask := fwpV4AddrAndMask{addr: binary.BigEndian.Uint32(addr[:]), mask: ^uint32(0) << (32 - prefix.Bits())}
		return condition(field, fwpMatchEqual, fwpV4AddrMask, pinned(pin, mask))
	}
	mask := fwpV6AddrAndMask{addr: prefix.Addr().As16(), prefixLength: uint8(prefix.Bits())}
	return condition(field, fwpMatchEqual, fwpV6AddrMask, pinned(pin, mask))
}

// A condition value reaches WFP as a bare uintptr, which neither keeps its target alive nor follows a stack move.
func pinned[T any](pin *runtime.Pinner, v T) uintptr {
	p := &v
	pin.Pin(p)
	return uintptr(unsafe.Pointer(p))
}

func displayData(name string) fwpmDisplayData0 {
	p, _ := windows.UTF16PtrFromString(name)
	return fwpmDisplayData0{name: p}
}
