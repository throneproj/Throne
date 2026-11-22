package sysdns

import (
	"bytes"
	"context"
	"crypto/rand"
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"maps"
	"net"
	"net/netip"
	"os"
	"regexp"
	"slices"
	"strconv"
	"strings"
	"sync"
	"time"

	"ThroneCore/internal/pfctl"

	"github.com/sagernet/sing-box/option"
	tun "github.com/sagernet/sing-tun"
	"github.com/sagernet/sing/common/control"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/shell"
	"github.com/sagernet/sing/common/x/list"
	"golang.org/x/sys/unix"
)

const (
	stateDir  = "/var/db/Throne"
	stateFile = stateDir + "/system-dns.json"
	lockFile  = stateDir + "/system-dns.lock"
	// Ahead of Apple's children, like the kill switch's anchors.
	redirectAnchor = pfctl.RootAnchor + "/000.throne-dns"
	// Must match kCoreSupervisorDnsRestoreMarker in src/engine/supervisor/CoreSupervisor.cpp.
	restoreFailedMarker = "The system DNS could not be restored on: "

	restoreAttempts   = 3
	restoreAttemptGap = 500 * time.Millisecond
	retryFirstDelay   = 5 * time.Second
	retryMaxDelay     = time.Minute
)

// record is written before every change, so whatever a dead core left behind can be put back by the next one.
type record struct {
	PID      int                 `json:"pid"`
	Alias    bool                `json:"alias,omitempty"`
	Services map[string][]string `json:"services,omitempty"`
	Redirect bool                `json:"redirect,omitempty"`
	Token    string              `json:"token,omitempty"`
	TokenPID string              `json:"tokenPid,omitempty"`
}

var (
	mu        sync.Mutex
	owned     record
	monitor   tun.DefaultInterfaceMonitor
	callback  *list.Element[tun.DefaultInterfaceUpdateCallback]
	followGen int
	// listenPort is where the dns-system inbound listens: behind the redirect, or 53 when pf cannot redirect.
	listenPort      uint16
	redirectFailing bool
)

var serviceOrderPattern = regexp.MustCompile(`(?m)^\((\d+|\*)\) (.+)\n\(Hardware Port: .*, Device: (.*)\)$`)

// Prepare adds the lo0 alias and checks that Address:53 can be bound; on error the system DNS must be left alone.
func Prepare() error {
	if os.Geteuid() != 0 {
		return errors.New("the core is not running as root")
	}
	mu.Lock()
	defer mu.Unlock()
	// Ends a retryRelease: from here the services belong to this Tun again.
	followGen++
	leftovers := restoreStaleLocked()
	owned.PID = os.Getpid()
	// Decided before the change is recorded: an alias that is already there belongs to someone else.
	owned.Alias = owned.Alias || !aliasPresent()
	for service, previous := range leftovers {
		if owned.Services == nil {
			owned.Services = map[string][]string{}
		}
		owned.Services[service] = previous
	}
	if err := saveLocked(); err != nil {
		return err
	}
	if owned.Alias && !aliasPresent() {
		if err := addAlias(); err != nil {
			return err
		}
	}
	port, err := redirectLocked()
	if err == nil {
		listenPort = port
		return nil
	}
	log.Println("system DNS: listening on port 53 itself, so macOS Internet Sharing's DNS proxy cannot start:", err)
	if err = releaseRedirectLocked(); err != nil {
		log.Println("system DNS:", err)
	}
	listenPort = 53
	return probe()
}

// AdjustInbound moves generate.cpp's dns-system inbound, written for port 53, to the port Prepare chose.
func AdjustInbound(options *option.Options) {
	mu.Lock()
	port := listenPort
	mu.Unlock()
	if port == 0 {
		return
	}
	for _, inbound := range options.Inbounds {
		if listen, ok := inbound.Options.(option.ListenOptionsWrapper); ok && inbound.Tag == InboundTag {
			listenOptions := listen.TakeListenOptions()
			listenOptions.ListenPort = port
			listen.ReplaceListenOptions(listenOptions)
		}
	}
}

// Apply points every network service with a device at Address, so switching interfaces never meets a service's own DNS,
// and points services that show up later on each default interface change.
func Apply(interfaceMonitor tun.DefaultInterfaceMonitor) error {
	mu.Lock()
	defer mu.Unlock()
	if owned.PID == 0 {
		return errors.New("not prepared")
	}
	monitor = interfaceMonitor
	followGen++
	gen := followGen
	callback = monitor.RegisterCallback(func(*control.Interface, int) {
		// Off the monitor's goroutine: networksetup is slow, and each run reconciles to the latest default anyway.
		go follow(gen)
	})
	if owned.Redirect {
		go keepRedirect(gen)
	}
	return followLocked(true)
}

// Release puts back every service Apply changed and removes the alias; it is a no-op when nothing was changed.
// What it cannot put back is retried in the background until it succeeds or the next Prepare takes the services over.
func Release() {
	mu.Lock()
	defer mu.Unlock()
	followGen++
	if monitor != nil && callback != nil {
		monitor.UnregisterCallback(callback)
	}
	monitor, callback = nil, nil
	listenPort = 0
	if owned.PID == 0 {
		return
	}
	failed, err := releaseLocked()
	if err == nil {
		return
	}
	log.Println("system DNS:", err)
	if len(failed) > 0 {
		fmt.Println(restoreFailedMarker + strings.Join(failed, ", "))
	}
	go retryRelease(followGen)
}

// releaseLocked returns the services still pointed at Address; the record stays under this PID while anything is left,
// so the next core finishes the job once this process is gone.
func releaseLocked() ([]string, error) {
	var errs []error
	for service := range owned.Services {
		if err := restoreLocked(service); err != nil {
			errs = append(errs, E.Cause(err, "restore ", service))
		}
	}
	if err := releaseRedirectLocked(); err != nil {
		errs = append(errs, err)
	}
	if owned.Alias {
		removeAlias()
		owned.Alias = false
	}
	if len(owned.Services) == 0 && !owned.Redirect {
		removeRecordLocked()
		owned = record{}
		return nil, nil
	}
	if err := saveLocked(); err != nil {
		errs = append(errs, err)
	}
	return slices.Sorted(maps.Keys(owned.Services)), E.Errors(errs...)
}

func retryRelease(gen int) {
	delay := retryFirstDelay
	for {
		time.Sleep(delay)
		delay = min(delay*2, retryMaxDelay)
		mu.Lock()
		if gen != followGen || owned.PID == 0 {
			mu.Unlock()
			return
		}
		dropMissingServicesLocked()
		_, err := releaseLocked()
		mu.Unlock()
		if err == nil {
			log.Println("system DNS: restored")
			return
		}
	}
}

// dropMissingServicesLocked forgets services deleted or renamed since they were pointed, which no retry can put back.
func dropMissingServicesLocked() {
	output, err := shell.Exec("/usr/sbin/networksetup", "-listnetworkserviceorder").ReadOutput()
	if err != nil {
		return
	}
	present := map[string]bool{}
	for _, match := range serviceOrderPattern.FindAllStringSubmatch(output, -1) {
		present[match[2]] = true
	}
	for service := range owned.Services {
		if !present[service] {
			log.Println("system DNS:", service, "no longer exists, so its DNS servers are not restored")
			delete(owned.Services, service)
		}
	}
}

// RestoreStale undoes what a core that died without Release left in the system DNS.
func RestoreStale() {
	if os.Geteuid() != 0 {
		return
	}
	mu.Lock()
	defer mu.Unlock()
	restoreStaleLocked()
}

func follow(gen int) {
	mu.Lock()
	defer mu.Unlock()
	if gen != followGen {
		return
	}
	if err := followLocked(false); err != nil {
		log.Println("system DNS:", err)
	}
}

// followLocked points every service not pointed yet; only a failure on the default interface's service is an error.
// repoint also covers services kept from a failed release, whose current servers are unknown.
func followLocked(repoint bool) error {
	services, err := listServices()
	if err != nil {
		return err
	}
	var defaultDevice string
	if defaultInterface := monitor.DefaultInterface(); defaultInterface != nil {
		defaultDevice = defaultInterface.Name
	}
	defaultFound := defaultDevice == ""
	var defaultErr error
	for _, service := range services {
		isDefault := service.device == defaultDevice
		defaultFound = defaultFound || isDefault
		if _, pointed := owned.Services[service.name]; pointed && !repoint {
			continue
		}
		if err = pointLocked(service.name); err == nil {
			continue
		}
		if isDefault {
			defaultErr = err
		} else {
			log.Println("system DNS: point", service.name+":", err)
		}
	}
	if defaultErr != nil {
		return defaultErr
	}
	if !defaultFound {
		return E.New("no network service uses ", defaultDevice)
	}
	return nil
}

func pointLocked(service string) error {
	if _, ok := owned.Services[service]; !ok {
		previous, err := dnsServers(service)
		if err != nil {
			return err
		}
		// Only a dead core leaves exactly this behind, and its record has already been replayed.
		if slices.Equal(previous, []string{Address}) {
			previous = nil
		}
		if owned.Services == nil {
			owned.Services = map[string][]string{}
		}
		owned.Services[service] = previous
		if err = saveLocked(); err != nil {
			delete(owned.Services, service)
			return err
		}
	}
	return setDNSServers(service, []string{Address})
}

// restoreLocked reads the servers back: only what networksetup reports afterwards proves the change was committed.
func restoreLocked(service string) error {
	previous := owned.Services[service]
	var err error
	for attempt := range restoreAttempts {
		if attempt > 0 {
			time.Sleep(restoreAttemptGap)
		}
		if err = setDNSServers(service, previous); err != nil {
			continue
		}
		var current []string
		if current, err = dnsServers(service); err != nil {
			continue
		}
		if !slices.Equal(current, previous) {
			err = E.New("the servers read back as [", strings.Join(current, " "), "]")
			continue
		}
		delete(owned.Services, service)
		if err = saveLocked(); err != nil {
			log.Println("system DNS:", err)
		}
		return nil
	}
	return err
}

// restoreStaleLocked replays the record of a dead core and returns the services it could not put back.
func restoreStaleLocked() map[string][]string {
	var leftovers map[string][]string
	err := withStateLock(func() error {
		stored, err := loadRecord()
		if err != nil || stored.PID == os.Getpid() || alive(stored.PID) {
			return nil
		}
		for service, previous := range stored.Services {
			if err = setDNSServers(service, previous); err != nil {
				log.Println("system DNS: restore", service+":", err)
				continue
			}
			delete(stored.Services, service)
		}
		if stored.Redirect {
			if err = pfctl.FlushAnchor(redirectAnchor); err != nil {
				log.Println("system DNS:", err)
			} else {
				stored.Redirect = false
			}
		}
		if stored.Token != "" {
			_ = pfctl.ReleaseToken(stored.Token, stored.TokenPID)
			stored.Token, stored.TokenPID = "", ""
		}
		if stored.Alias {
			removeAlias()
			stored.Alias = false
		}
		if len(stored.Services) == 0 && !stored.Redirect {
			return os.Remove(stateFile)
		}
		leftovers = stored.Services
		return writeRecord(stored)
	})
	if err != nil {
		log.Println("system DNS:", err)
	}
	return leftovers
}

func saveLocked() error {
	return withStateLock(func() error {
		return writeRecord(owned)
	})
}

func removeRecordLocked() {
	err := withStateLock(func() error {
		// A later core may have taken the file over since.
		if stored, err := loadRecord(); err != nil || stored.PID != os.Getpid() {
			return nil
		}
		return os.Remove(stateFile)
	})
	if err != nil {
		log.Println("system DNS:", err)
	}
}

func withStateLock(fn func() error) error {
	if err := os.MkdirAll(stateDir, 0o700); err != nil {
		return err
	}
	file, err := os.OpenFile(lockFile, os.O_CREATE|os.O_RDWR, 0o600)
	if err != nil {
		return err
	}
	defer file.Close()
	if err = unix.Flock(int(file.Fd()), unix.LOCK_EX); err != nil {
		return err
	}
	defer unix.Flock(int(file.Fd()), unix.LOCK_UN)
	return fn()
}

func loadRecord() (record, error) {
	var stored record
	data, err := os.ReadFile(stateFile)
	if err != nil {
		return stored, err
	}
	return stored, json.Unmarshal(data, &stored)
}

func writeRecord(stored record) error {
	data, err := json.Marshal(stored)
	if err != nil {
		return err
	}
	temp := stateFile + ".tmp"
	if err = os.WriteFile(temp, data, 0o600); err != nil {
		return err
	}
	return os.Rename(temp, stateFile)
}

// alive also matches the command name: the record survives reboots, where its PID often belongs to someone else.
func alive(pid int) bool {
	if pid <= 0 {
		return false
	}
	owner, err := unix.SysctlKinfoProc("kern.proc.pid", pid)
	if err != nil || int(owner.Proc.P_pid) != pid {
		return false
	}
	self, err := unix.SysctlKinfoProc("kern.proc.pid", os.Getpid())
	return err == nil && owner.Proc.P_comm == self.Proc.P_comm
}

func aliasPresent() bool {
	loopback, err := net.InterfaceByName("lo0")
	if err != nil {
		return false
	}
	addrs, err := loopback.Addrs()
	if err != nil {
		return false
	}
	for _, addr := range addrs {
		if prefix, ok := addr.(*net.IPNet); ok && prefix.IP.String() == Address {
			return true
		}
	}
	return false
}

func addAlias() error {
	output, err := shell.Exec("/sbin/ifconfig", "lo0", "alias", Address, "netmask", "255.255.255.255").Read()
	return withOutput(err, output)
}

func removeAlias() {
	if !aliasPresent() {
		return
	}
	output, err := shell.Exec("/sbin/ifconfig", "lo0", "-alias", Address).Read()
	if err = withOutput(err, output); err != nil {
		log.Println("system DNS: remove the loopback alias:", err)
	}
}

// probe binds with the reuse options sing-box's reuse_addr sets, so success means the inbound will start.
func probe() error {
	config := net.ListenConfig{Control: control.ReuseAddr()}
	address := net.JoinHostPort(Address, "53")
	packetConn, err := config.ListenPacket(context.Background(), "udp4", address)
	if err != nil {
		return E.Cause(err, "listen on ", address)
	}
	packetConn.Close()
	listener, err := config.Listen(context.Background(), "tcp4", address)
	if err != nil {
		return E.Cause(err, "listen on ", address)
	}
	return listener.Close()
}

// redirectLocked keeps the listener off port 53: mDNSResponder's Internet Sharing DNS proxy binds *:53 without
// SO_REUSEADDR, which XNU refuses next to any specific-address :53 socket (#2061).
func redirectLocked() (uint16, error) {
	target, port, err := reservePort()
	if err != nil {
		return 0, err
	}
	defer target.Close()
	token, tokenPID, err := pfctl.Enable()
	if token != "" {
		owned.Token, owned.TokenPID = token, tokenPID
	}
	if err != nil {
		return 0, E.Cause(err, "enable pf")
	}
	owned.Redirect = true
	if err = saveLocked(); err != nil {
		return 0, err
	}
	if err = loadRedirect(port); err != nil {
		return 0, err
	}
	return port, checkRedirect(target)
}

func releaseRedirectLocked() error {
	var err error
	if owned.Redirect {
		if err = pfctl.FlushAnchor(redirectAnchor); err == nil {
			owned.Redirect = false
		}
	}
	if owned.Token != "" {
		if tokenErr := pfctl.ReleaseToken(owned.Token, owned.TokenPID); tokenErr != nil {
			log.Println("system DNS: release the pf reference:", tokenErr)
		}
		owned.Token, owned.TokenPID = "", ""
	}
	redirectFailing = false
	return err
}

func loadRedirect(port uint16) error {
	rule := fmt.Sprintf("rdr pass on lo0 inet proto { tcp udp } from any to %s port 53 -> %s port %d\n", Address, Address, port)
	_, err := pfctl.Run([]byte(rule), "-a", redirectAnchor, "-f", "-")
	return err
}

// reservePort finds a port on Address free for both UDP and TCP, and keeps the UDP socket for checkRedirect.
func reservePort() (*net.UDPConn, uint16, error) {
	config := net.ListenConfig{Control: control.ReuseAddr()}
	for range 8 {
		packetConn, err := config.ListenPacket(context.Background(), "udp4", net.JoinHostPort(Address, "0"))
		if err != nil {
			return nil, 0, E.Cause(err, "listen on ", Address)
		}
		udpConn := packetConn.(*net.UDPConn)
		port := udpConn.LocalAddr().(*net.UDPAddr).Port
		listener, err := config.Listen(context.Background(), "tcp4", net.JoinHostPort(Address, strconv.Itoa(port)))
		if err == nil {
			listener.Close()
			return udpConn, uint16(port), nil
		}
		udpConn.Close()
	}
	return nil, 0, E.New("no port on ", Address, " is free for both UDP and TCP")
}

// checkRedirect catches a pf.conf that skips lo0 or has no rdr-anchor "com.apple/*".
func checkRedirect(target *net.UDPConn) error {
	client, err := net.DialUDP("udp4", nil, net.UDPAddrFromAddrPort(netip.AddrPortFrom(netip.MustParseAddr(Address), 53)))
	if err != nil {
		return err
	}
	defer client.Close()
	deadline := time.Now().Add(time.Second)
	_ = client.SetDeadline(deadline)
	_ = target.SetDeadline(deadline)
	payload := make([]byte, 16)
	_, _ = rand.Read(payload)
	buffer := make([]byte, 64)
	if _, err = client.Write(payload); err != nil {
		return E.Cause(err, "check the pf redirect")
	}
	n, from, err := target.ReadFromUDPAddrPort(buffer)
	if err != nil || !bytes.Equal(buffer[:n], payload) {
		return E.New("pf did not redirect ", Address, ":53")
	}
	if _, err = target.WriteToUDPAddrPort(payload, from); err != nil {
		return E.Cause(err, "check the pf redirect")
	}
	// The client is connected to port 53, so it only takes the reply if pf translated the source back.
	n, err = client.Read(buffer)
	if err != nil || !bytes.Equal(buffer[:n], payload) {
		return E.New("pf did not translate replies from ", Address, ":53 back")
	}
	return nil
}

// keepRedirect puts the redirect back when another program flushes it or turns pf off: system DNS dies without it.
func keepRedirect(gen int) {
	ticker := time.NewTicker(10 * time.Second)
	defer ticker.Stop()
	for range ticker.C {
		mu.Lock()
		if gen != followGen || !owned.Redirect {
			mu.Unlock()
			return
		}
		err := ensureRedirectLocked()
		if err != nil && !redirectFailing {
			log.Println("system DNS: restore the pf redirect:", err)
		}
		redirectFailing = err != nil
		mu.Unlock()
	}
}

func ensureRedirectLocked() error {
	info, err := pfctl.Run(nil, "-s", "info")
	if err != nil {
		return err
	}
	if !strings.Contains(info.Stdout, "Status: Enabled") {
		// pfctl -d freed our reference along with everyone else's.
		token, tokenPID, enableErr := pfctl.Enable()
		if token != "" {
			owned.Token, owned.TokenPID = token, tokenPID
			_ = saveLocked()
		}
		if enableErr != nil {
			return enableErr
		}
	}
	rules, err := pfctl.Run(nil, "-a", redirectAnchor, "-s", "nat")
	if err != nil || strings.TrimSpace(rules.Stdout) != "" {
		return err
	}
	return loadRedirect(listenPort)
}

type networkService struct {
	name   string
	device string
}

// listServices includes disabled services, which can be enabled while the Tun runs, and skips deviceless ones such as other VPNs.
func listServices() ([]networkService, error) {
	output, err := shell.Exec("/usr/sbin/networksetup", "-listnetworkserviceorder").ReadOutput()
	if err != nil {
		return nil, err
	}
	var services []networkService
	for _, match := range serviceOrderPattern.FindAllStringSubmatch(output, -1) {
		if match[3] != "" {
			services = append(services, networkService{name: match[2], device: match[3]})
		}
	}
	return services, nil
}

func dnsServers(service string) ([]string, error) {
	output, err := shell.Exec("/usr/sbin/networksetup", "-getdnsservers", service).ReadOutput()
	if err != nil {
		return nil, err
	}
	var servers []string
	for line := range strings.Lines(output) {
		line = strings.TrimSpace(line)
		if _, parseErr := netip.ParseAddr(line); parseErr == nil {
			servers = append(servers, line)
		} else if !strings.HasPrefix(line, "There aren't any DNS Servers") {
			return nil, E.New("read the DNS servers of ", service, ": ", output)
		}
	}
	return servers, nil
}

func setDNSServers(service string, servers []string) error {
	args := append([]string{"-setdnsservers", service}, servers...)
	if len(servers) == 0 {
		args = append(args, "Empty")
	}
	// networksetup commits through configd; only its plist backup needs root, so its "Permission denied" is noise.
	output, err := shell.Exec("/usr/sbin/networksetup", args...).Read()
	return withOutput(err, output)
}

func withOutput(err error, output string) error {
	if err == nil {
		return nil
	}
	if output = strings.TrimSpace(output); output != "" {
		return E.Cause(err, output)
	}
	return err
}
