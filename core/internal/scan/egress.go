package scan

import (
	"context"
	"errors"
	"fmt"
	"net"
	"net/netip"
	"os"

	"github.com/sagernet/sing/common/control"
)

// ErrNoEgress is a refusal to probe at all, not a verdict on the target: Probe leaves such positions unfinished.
var ErrNoEgress = errors.New("no default interface")

var ErrEgressLost = fmt.Errorf("%w; the scan was paused", ErrNoEgress)

// Re-read per probe, so default-route changes apply.
type Egress interface {
	// nil = unbound (the OS routes); ErrNoEgress = refuse rather than leak into the Tun.
	Control() (control.Func, error)
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

func contextOr(ctx context.Context, err error) error {
	if ctxErr := ctx.Err(); ctxErr != nil {
		return ctxErr
	}
	return err
}
