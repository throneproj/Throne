//go:build !windows

package rpc

func isElevated() bool { return false }
