//go:build !linux && !darwin && !windows

package invoker

import "errors"

type identity struct{}

func boundary() bool { return false }

func invokingUser() (identity, bool) { return identity{}, false }

func (identity) assume() error { return errors.New("not supported on this platform") }

func (identity) revert() error { return nil }
