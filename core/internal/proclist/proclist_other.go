//go:build !windows && !linux && !darwin

package proclist

import "errors"

func List() ([]Entry, error) {
	return nil, errors.New("not supported")
}
