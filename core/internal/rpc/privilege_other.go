//go:build !linux

package rpc

func hasTunCapabilities() bool { return false }
