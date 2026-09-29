package rpc

import "golang.org/x/sys/unix"

// sing-box's service unit set: tun/nftables/sysctl, raw sockets, ports < 1024 and find_process on other users' sockets.
var tunCapabilities = []int{
	unix.CAP_NET_ADMIN,
	unix.CAP_NET_RAW,
	unix.CAP_NET_BIND_SERVICE,
	unix.CAP_SYS_PTRACE,
	unix.CAP_DAC_READ_SEARCH,
}

// A subset (say NET_ADMIN alone) opens the tun and then fails part-way, so only the whole set counts as privileged.
func hasTunCapabilities() bool {
	header := unix.CapUserHeader{Version: unix.LINUX_CAPABILITY_VERSION_3}
	var data [2]unix.CapUserData
	if err := unix.Capget(&header, &data[0]); err != nil {
		return false
	}
	for _, capability := range tunCapabilities {
		if data[capability/32].Effective&(1<<uint(capability%32)) == 0 {
			return false
		}
	}
	return true
}
