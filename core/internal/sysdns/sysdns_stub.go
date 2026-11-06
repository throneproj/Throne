//go:build !darwin

package sysdns

import (
	"errors"

	"github.com/sagernet/sing-box/option"
	tun "github.com/sagernet/sing-tun"
)

func Prepare() error {
	return errors.ErrUnsupported
}

func AdjustInbound(*option.Options) {}

func Apply(tun.DefaultInterfaceMonitor) error {
	return errors.ErrUnsupported
}

func Release() {}

func RestoreStale() {}
