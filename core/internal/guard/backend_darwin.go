package guard

import (
	"crypto/rand"
	"encoding/hex"
	"errors"
	"fmt"
	"log"
	"net"
	"net/netip"
	"os"
	"regexp"
	"slices"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"time"
)

const pfTamperedMessage = "pf was disabled or the kill switch rules were removed by another program"

var pfTokenPattern = regexp.MustCompile(`(?m)^Token\s*:\s*(\d+)\s*$`)

type pfBackend struct {
	cfg      *config
	events   chan<- Event
	id       string
	anchor   string
	lock     *os.File
	token    string
	tokenPID string
	loaded   bool

	tuns          []string
	reloadFailing bool

	stop     chan struct{}
	stopOnce sync.Once
	workers  sync.WaitGroup
}

func newBackend() (backend, error) {
	if os.Geteuid() != 0 {
		return nil, newError(CodePrivilege, "the kill switch needs the core to have root privileges")
	}
	return &pfBackend{}, nil
}

func (b *pfBackend) Arm(cfg *config, events chan<- Event) error {
	b.cfg = cfg
	b.events = events
	if err := b.arm(); err != nil {
		b.teardown()
		return err
	}
	b.stop = make(chan struct{})
	b.workers.Add(2)
	goSafe(events, func() {
		defer b.workers.Done()
		watchInterfaces(b.stop, 5*time.Second, b.recheck)
	})
	goSafe(events, func() {
		defer b.workers.Done()
		b.watchdog()
	})
	return nil
}

func (b *pfBackend) arm() error {
	var raw [4]byte
	if _, err := rand.Read(raw[:]); err != nil {
		return newError(CodeInternal, "generate the anchor id: %w", err)
	}
	b.id = hex.EncodeToString(raw[:])
	b.anchor = pfAnchorPrefix + b.id

	lock, err := os.OpenFile(pfLockPath(b.id), os.O_RDWR|os.O_CREATE|os.O_EXCL, 0o600)
	if err != nil {
		return newError(CodeInternal, "create the kill switch lock: %w", err)
	}
	b.lock = lock
	// The kernel drops a flock even on SIGKILL: CleanupStale reads a free lock as a dead guard.
	if err = syscall.Flock(int(lock.Fd()), syscall.LOCK_EX|syscall.LOCK_NB); err != nil {
		return newError(CodeInternal, "lock %s: %w", lock.Name(), err)
	}

	// -E takes a reference-counted enable; -e and -d would switch pf for its other users too.
	enable, err := pfctl(nil, "-E")
	if match := pfTokenPattern.FindStringSubmatch(enable.stderr + "\n" + enable.stdout); match != nil {
		b.token, b.tokenPID = match[1], strconv.Itoa(enable.pid)
	}
	if err != nil {
		return newError(CodeInternal, "enable pf: %w", err)
	}
	if b.token == "" {
		return newError(CodeInternal, "enable pf: pfctl printed no reference token")
	}
	if _, err = fmt.Fprintf(lock, "%s %s\n", b.token, b.tokenPID); err != nil {
		return newError(CodeInternal, "record the pf token: %w", err)
	}

	b.tuns, _ = findTuns(b.cfg.TunPrefixes)
	b.loaded = true
	if err = b.load(b.tuns); err != nil {
		return newError(CodeInternal, "load the kill switch rules: %w", err)
	}
	if err = checkMainRuleset(); err != nil {
		return err
	}
	// pf states bypass rule evaluation; a replacing guard keeps them (the API pass's) only if its predecessor never stopped blocking.
	if !b.cfg.KeepStates || !liveGuardAnchor(b.id) {
		if _, err = pfctl(nil, "-F", "states"); err != nil {
			return newError(CodeInternal, "flush the pf states: %w", err)
		}
	}
	cleanupStale(b.id)
	log.Printf("guard: pf anchor %s armed, tun %v", b.anchor, b.tuns)
	return nil
}

func (b *pfBackend) Disarm() {
	b.stopOnce.Do(func() {
		if b.stop != nil {
			close(b.stop)
		}
	})
	b.workers.Wait()
	b.teardown()
}

func (b *pfBackend) teardown() {
	if b.lock == nil {
		return
	}
	if b.loaded {
		err := flushAnchor(b.anchor)
		if err != nil {
			err = flushAnchor(b.anchor)
		}
		if err != nil {
			log.Printf("guard: %v", err)
		}
		b.loaded = false
	}
	if b.token != "" {
		if err := releaseToken(b.token, b.tokenPID); err != nil {
			log.Printf("guard: release the pf reference: %v", err)
		}
		b.token = ""
	}
	if err := os.Remove(b.lock.Name()); err != nil {
		log.Printf("guard: %v", err)
	}
	_ = b.lock.Close()
	b.lock = nil
}

func (b *pfBackend) load(tuns []string) error {
	_, err := pfctl([]byte(pfRuleset(b.cfg, tuns)), "-o", "none", "-a", b.anchor, "-f", "-")
	return err
}

// recheck runs on the interface watcher only, which therefore owns tuns and reloadFailing.
func (b *pfBackend) recheck() {
	tuns, err := findTuns(b.cfg.TunPrefixes)
	if err != nil || slices.Equal(tuns, b.tuns) {
		return
	}
	if err = b.load(tuns); err != nil {
		log.Printf("guard: reload pf rules for tun %v: %v", tuns, err)
		if !b.reloadFailing {
			b.reloadFailing = true
			select {
			case b.events <- Event{Message: "the kill switch could not follow the tun interface: " + err.Error()}:
			default:
			}
		}
		return
	}
	b.reloadFailing = false
	b.tuns = tuns
	log.Printf("guard: tun %v", tuns)
}

func (b *pfBackend) watchdog() {
	ticker := time.NewTicker(5 * time.Second)
	defer ticker.Stop()
	retried := false
	for {
		select {
		case <-b.stop:
			return
		case <-ticker.C:
		}
		err := b.verify()
		if err == nil {
			retried = false
			continue
		}
		if errors.Is(err, errPfctlIncomplete) && !retried {
			retried = true
			log.Printf("guard: pf check did not finish, retrying: %v", err)
			continue
		}
		log.Printf("guard: pf check failed: %v", err)
		select {
		case b.events <- Event{Fatal: true, Code: CodeTampered, Message: pfTamperedMessage}:
		case <-b.stop:
		}
		return
	}
}

func (b *pfBackend) verify() error {
	info, err := pfctl(nil, "-s", "info")
	if err != nil {
		return err
	}
	if !strings.Contains(info.stdout, "Status: Enabled") {
		return errors.New("pf is disabled")
	}
	rules, err := pfctl(nil, "-a", b.anchor, "-s", "rules")
	if err != nil {
		return err
	}
	if strings.TrimSpace(rules.stdout) == "" {
		return fmt.Errorf("pf anchor %s is empty", b.anchor)
	}
	return checkMainRuleset()
}

func checkMainRuleset() error {
	rules, err := pfctl(nil, "-s", "rules")
	if err != nil {
		return newError(CodeInternal, "read the pf main ruleset: %w", err)
	}
	for line := range strings.Lines(rules.stdout) {
		if strings.HasPrefix(strings.TrimSpace(line), `anchor "`+pfRootAnchor+`/*"`) {
			return nil
		}
	}
	return newError(CodeUnsupported, "the pf main ruleset (/etc/pf.conf) does not evaluate com.apple anchors")
}

// cfg.BridgeName is ignored: the bridge's forwarded packets carry no pf tag yet, so they stay blocked.
func pfRuleset(cfg *config, tuns []string) string {
	var rules strings.Builder
	add := func(format string, args ...any) {
		_, _ = fmt.Fprintf(&rules, format+"\n", args...)
	}
	add("pass out quick on lo0 all no state")
	// Ahead of the LAN DNS block: the tun's own DNS address may fall inside a LAN prefix.
	for _, tun := range tuns {
		add("pass out quick on %s all no state", tun)
	}
	// pf ignores group on non-TCP/UDP packets and on fragments: proto and the port range keep both off this rule.
	add("pass out quick proto { tcp udp } from any to any port 0:65535 group %d no state", GID)
	add("pass out quick inet proto udp from any port 68 to any port 67 no state")
	add("pass out quick inet6 proto udp from any port 546 to any port 547 no state")
	add("pass out quick inet6 proto 58 all icmp6-type { 130 131 132 133 134 135 136 137 143 } no state")
	add("pass out quick inet proto 2 from any to 224.0.0.0/4 no state")
	if cfg.APIPort != 0 {
		// The state an inbound connection creates carries the API's replies past the final block.
		add("pass in quick proto tcp from any to any port %d keep state", cfg.APIPort)
	}
	if cfg.AllowLAN {
		var ports []string
		for _, port := range lanDNSPorts {
			ports = append(ports, strconv.Itoa(int(port)))
		}
		lan4, lan6 := pfHosts(lanPrefixes4), pfHosts(lanPrefixes6)
		add("block drop out quick inet proto { tcp udp } from any to { %s } port { %s }", lan4, strings.Join(ports, " "))
		add("block drop out quick inet6 proto { tcp udp } from any to { %s } port { %s }", lan6, strings.Join(ports, " "))
		add("pass out quick inet from any to { %s } no state", lan4)
		add("pass out quick inet6 from any to { %s } no state", lan6)
	}
	add("block drop out quick all")
	return rules.String()
}

func pfHosts(prefixes []netip.Prefix) string {
	hosts := make([]string, 0, len(prefixes))
	for _, prefix := range prefixes {
		hosts = append(hosts, prefix.String())
	}
	return strings.Join(hosts, " ")
}

// Only a utun carrying one of the tun's exact addresses counts: a LAN may share the tun's subnet.
func findTuns(prefixes []netip.Prefix) ([]string, error) {
	if len(prefixes) == 0 {
		return nil, nil
	}
	interfaces, err := net.Interfaces()
	if err != nil {
		return nil, err
	}
	var names []string
	for _, iface := range interfaces {
		if !strings.HasPrefix(iface.Name, "utun") || !interfaceNamePattern.MatchString(iface.Name) {
			continue
		}
		addrs, err := iface.Addrs()
		if err != nil {
			return nil, err
		}
		if carriesAny(addrs, prefixes) {
			names = append(names, iface.Name)
		}
	}
	slices.Sort(names)
	return names, nil
}

func carriesAny(addrs []net.Addr, prefixes []netip.Prefix) bool {
	for _, addr := range addrs {
		ipNet, ok := addr.(*net.IPNet)
		if !ok {
			continue
		}
		ip, ok := netip.AddrFromSlice(ipNet.IP)
		if !ok {
			continue
		}
		for _, prefix := range prefixes {
			if ip.Unmap() == prefix.Addr().Unmap() {
				return true
			}
		}
	}
	return false
}
