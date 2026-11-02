//go:build !darwin

package sysdns

import "errors"

func Apply(string) error {
	return errors.ErrUnsupported
}

func Release() {}

func RestoreLegacy() {}
