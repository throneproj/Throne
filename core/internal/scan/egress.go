package scan

import (
	"context"
	"errors"
	"fmt"
	"net"
	"net/netip"
	"os"
	"time"

	"github.com/sagernet/sing/common/control"
)

// ErrNoEgress is a refusal to probe at all, not a verdict on the target: Probe leaves such positions unfinished.
var ErrNoEgress = errors.New("no default interface")

var ErrEgressLost = fmt.Errorf("%w; the scan was paused", ErrNoEgress)

// Re-read per probe, so default-route changes apply.
type Egress interface {
	// nil = unbound (the OS routes); ErrNoEgress = refuse rather than leak into the Tun.
	Control() (control.Func, error)
	// The interface Control binds a socket for destination to; nil when unbound.
	InterfaceFor(destination netip.Addr) *control.Interface
}

func dialTCP(ctx context.Context, egress Egress, addr netip.Addr, port uint16, timeout time.Duration) (net.Conn, time.Duration, error) {
	controlFunc, err := egress.Control()
	if err != nil {
		return nil, 0, err
	}
	dialer := net.Dialer{Timeout: timeout, Control: controlFunc}
	begin := time.Now()
	conn, err := dialer.DialContext(ctx, "tcp", netip.AddrPortFrom(addr, port).String())
	if err != nil {
		return nil, 0, err
	}
	return conn, time.Since(begin), nil
}

func DisplayTarget(addr netip.Addr, port uint16) string {
	if port != 0 {
		return netip.AddrPortFrom(addr, port).String()
	}
	if addr.Is6() {
		return "[" + addr.String() + "]"
	}
	return addr.String()
}

func errorText(err error) string {
	if err == nil {
		return ""
	}
	if staged, ok := err.(*dnsStageError); ok {
		return staged.stage + ": " + errorText(staged.err)
	}
	if errors.Is(err, context.DeadlineExceeded) || errors.Is(err, os.ErrDeadlineExceeded) {
		return "timeout"
	}
	var netErr net.Error
	if errors.As(err, &netErr) && netErr.Timeout() {
		return "timeout"
	}
	var opErr *net.OpError
	if errors.As(err, &opErr) && opErr.Err != nil {
		err = opErr.Err
	}
	var syscallErr *os.SyscallError
	if errors.As(err, &syscallErr) && syscallErr.Err != nil {
		err = syscallErr.Err
	}
	return err.Error()
}
