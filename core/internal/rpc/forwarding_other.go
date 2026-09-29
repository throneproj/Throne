//go:build !windows

package rpc

import "github.com/sagernet/sing-box/adapter"

func watchEgressForwarding(adapter.NetworkManager) func() { return nil }
