// Package guard is the kill switch: a privileged process the GUI spawns with --guard that blocks every
// packet not sent by Throne or routed into its tun, for exactly as long as the GUI keeps its stdin open.
package guard

import (
	"bufio"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log"
	"net/netip"
	"os"
	"os/signal"
	"path/filepath"
	"regexp"
	"runtime"
	"runtime/debug"
	"strings"
	"sync"
	"syscall"
	"time"

	tun "github.com/sagernet/sing-tun"
	"github.com/sagernet/sing/common/logger"
)

// GID marks Throne's sockets on Linux and macOS; it sits outside systemd's nspawn range (524288–1879048191).
const GID = 0x7448524F

// EnvIdentity makes a root core adopt GID before it opens any socket.
const EnvIdentity = "THRONE_GUARD"

// IdentityExitCode is the core's exit code when ApplyIdentity fails; it must match Sys::KillSwitch::kCoreIdentityExitCode.
const IdentityExitCode = 86

const (
	CodePrivilege   = "privilege"
	CodeUnsupported = "unsupported"
	CodeConfig      = "config"
	CodeTampered    = "tampered"
	CodeInternal    = "internal"
)

// Config is the single JSON line the GUI writes to the guard's stdin.
type Config struct {
	AllowLAN   bool     `json:"allow_lan"`
	TunName    string   `json:"tun_name"`
	TunCIDRs   []string `json:"tun_cidrs"`
	ExtraPaths []string `json:"extra_paths"`
	BridgeName string   `json:"bridge_name"`
	// APIPort is the Throne API's TCP port, 0 when it does not listen; replies on connections peers open to it pass.
	APIPort uint16 `json:"api_port"`
	// KeepStates skips macOS's global pf state flush; set only when replacing a READY guard whose anchor stays loaded meanwhile.
	KeepStates bool `json:"keep_states"`
}

type config struct {
	AllowLAN    bool
	TunName     string
	TunPrefixes []netip.Prefix
	ExtraPaths  []string
	BridgeName  string
	APIPort     uint16
	KeepStates  bool
}

// Event is a runtime report from a backend; a Fatal one makes the guard disarm and exit.
type Event struct {
	Fatal   bool
	Code    string
	Message string
}

type backend interface {
	// Arm installs the whole ruleset atomically and returns only once it is enforced; on error nothing stays installed.
	Arm(cfg *config, events chan<- Event) error
	// Disarm removes everything Arm installed; it must be safe to call more than once and after a failed Arm.
	Disarm()
}

type Error struct {
	Code string
	Err  error
}

func (e *Error) Error() string { return e.Err.Error() }
func (e *Error) Unwrap() error { return e.Err }

func newError(code string, format string, args ...any) error {
	return &Error{Code: code, Err: fmt.Errorf(format, args...)}
}

func errorCode(err error) string {
	var guardErr *Error
	if errors.As(err, &guardErr) {
		return guardErr.Code
	}
	return CodeInternal
}

var (
	lanPrefixes4 = []netip.Prefix{
		netip.MustParsePrefix("10.0.0.0/8"),
		netip.MustParsePrefix("172.16.0.0/12"),
		netip.MustParsePrefix("192.168.0.0/16"),
		netip.MustParsePrefix("169.254.0.0/16"),
		netip.MustParsePrefix("224.0.0.0/4"),
		netip.MustParsePrefix("255.255.255.255/32"),
	}
	lanPrefixes6 = []netip.Prefix{
		netip.MustParsePrefix("fe80::/10"),
		netip.MustParsePrefix("fc00::/7"),
		netip.MustParsePrefix("ff00::/8"),
	}
	// Blocked towards LAN even when LAN is allowed: a LAN resolver forwards upstream, outside the tunnel.
	lanDNSPorts = []uint16{53, 853}
)

var interfaceNamePattern = regexp.MustCompile(`^[A-Za-z0-9._-]*$`)

func parseConfig(line string) (*config, error) {
	var raw Config
	if err := json.Unmarshal([]byte(line), &raw); err != nil {
		return nil, newError(CodeConfig, "invalid configuration: %v", err)
	}
	if len(raw.TunName) > 15 || !interfaceNamePattern.MatchString(raw.TunName) {
		return nil, newError(CodeConfig, "invalid tun name %q", raw.TunName)
	}
	if len(raw.BridgeName) > 12 || !interfaceNamePattern.MatchString(raw.BridgeName) {
		return nil, newError(CodeConfig, "invalid bridge name %q", raw.BridgeName)
	}
	cfg := &config{AllowLAN: raw.AllowLAN, TunName: raw.TunName, BridgeName: raw.BridgeName, APIPort: raw.APIPort, KeepStates: raw.KeepStates}
	for _, cidr := range raw.TunCIDRs {
		if strings.TrimSpace(cidr) == "" {
			continue
		}
		prefix, err := netip.ParsePrefix(strings.TrimSpace(cidr))
		if err != nil {
			return nil, newError(CodeConfig, "invalid tun address %q", cidr)
		}
		cfg.TunPrefixes = append(cfg.TunPrefixes, prefix)
	}
	seen := make(map[string]bool)
	for _, path := range raw.ExtraPaths {
		path = filepath.Clean(strings.TrimSpace(path))
		if path == "." || !filepath.IsAbs(path) {
			continue
		}
		key := path
		if runtime.GOOS == "windows" {
			key = strings.ToLower(path)
		}
		if !seen[key] {
			seen[key] = true
			cfg.ExtraPaths = append(cfg.ExtraPaths, path)
		}
	}
	return cfg, nil
}

type reporter struct {
	access sync.Mutex
}

func (r *reporter) line(fields ...string) {
	r.access.Lock()
	defer r.access.Unlock()
	text := strings.Join(fields, " ")
	text = strings.NewReplacer("\r", " ", "\n", " ").Replace(text)
	_, _ = fmt.Fprintln(os.Stdout, text)
}

// Run never returns. Stdout carries READY, ERROR <code> <message>, WARN <message> and FAIL <code> <message> lines.
func Run(parentPID int) {
	signal.Ignore(syscall.SIGPIPE)
	log.SetOutput(os.Stderr)
	out := &reporter{}
	// Registered before Arm: a default-action SIGTERM mid-Arm would leave macOS pf rules behind.
	signals := make(chan os.Signal, 1)
	signal.Notify(signals, os.Interrupt, syscall.SIGTERM, syscall.SIGHUP)

	var active backend
	defer func() {
		if r := recover(); r != nil {
			if active != nil {
				active.Disarm()
			}
			fmt.Fprintf(os.Stderr, "guard panicked: %v\n%s\n", r, debug.Stack())
			os.Exit(2)
		}
	}()

	reader := bufio.NewReader(os.Stdin)
	cfg, err := readConfig(reader, 10*time.Second)
	if err != nil {
		out.line("ERROR", errorCode(err), err.Error())
		os.Exit(1)
	}

	events := make(chan Event, 16)
	active, err = newBackend()
	if err == nil {
		err = active.Arm(cfg, events)
	}
	if err != nil {
		if active != nil {
			active.Disarm()
		}
		out.line("ERROR", errorCode(err), err.Error())
		os.Exit(1)
	}
	out.line("READY")

	done := make(chan string, 4)
	go func() {
		_, _ = io.Copy(io.Discard, reader)
		done <- "stdin closed"
	}()
	go watchParent(parentPID, done)

	for {
		select {
		case reason := <-done:
			log.Printf("guard: %s, disarming", reason)
			active.Disarm()
			os.Exit(0)
		case received := <-signals:
			log.Printf("guard: %v, disarming", received)
			active.Disarm()
			os.Exit(0)
		case event := <-events:
			if !event.Fatal {
				out.line("WARN", event.Message)
				continue
			}
			out.line("FAIL", event.Code, event.Message)
			active.Disarm()
			os.Exit(3)
		}
	}
}

func readConfig(reader *bufio.Reader, timeout time.Duration) (*config, error) {
	type result struct {
		line string
		err  error
	}
	received := make(chan result, 1)
	go func() {
		line, err := reader.ReadString('\n')
		received <- result{line, err}
	}()
	select {
	case r := <-received:
		if r.err != nil && strings.TrimSpace(r.line) == "" {
			return nil, newError(CodeConfig, "no configuration received: %v", r.err)
		}
		return parseConfig(r.line)
	case <-time.After(timeout):
		return nil, newError(CodeConfig, "no configuration received within %v", timeout)
	}
}

// A reparented guard means the GUI died without its pipe closing, e.g. while a descendant still held it.
func watchParent(parentPID int, done chan<- string) {
	if parentPID <= 1 {
		return
	}
	if runtime.GOOS == "windows" {
		parent, err := os.FindProcess(parentPID)
		if err != nil {
			done <- "parent not found"
			return
		}
		_, _ = parent.Wait()
		done <- "parent exited"
		return
	}
	for range time.Tick(time.Second) {
		if os.Getppid() != parentPID {
			done <- "parent exited"
			return
		}
	}
}

// goSafe runs fn on its own goroutine; a panic there must disarm through Run, not kill the process with rules installed.
func goSafe(events chan<- Event, fn func()) {
	go func() {
		defer func() {
			if r := recover(); r != nil {
				log.Printf("guard: panic: %v\n%s", r, debug.Stack())
				events <- Event{Fatal: true, Code: CodeInternal, Message: fmt.Sprint("internal error: ", r)}
			}
		}()
		fn()
	}()
}

// watchInterfaces calls onChange after every interface or route change, and every interval as a fallback.
func watchInterfaces(stop <-chan struct{}, interval time.Duration, onChange func()) {
	trigger := make(chan struct{}, 1)
	notify := func() {
		select {
		case trigger <- struct{}{}:
		default:
		}
	}
	monitor, err := tun.NewNetworkUpdateMonitor(logger.NOP())
	if err == nil {
		monitor.RegisterCallback(notify)
		if err = monitor.Start(); err != nil {
			log.Printf("guard: network monitor unavailable, polling only: %v", err)
		} else {
			defer monitor.Close()
		}
	}
	ticker := time.NewTicker(interval)
	defer ticker.Stop()
	for {
		select {
		case <-stop:
			return
		case <-trigger:
			onChange()
		case <-ticker.C:
			onChange()
		}
	}
}
