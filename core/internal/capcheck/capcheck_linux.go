package capcheck

import (
	"errors"
	"fmt"
	"os"

	"github.com/sagernet/nftables"
	"golang.org/x/sys/unix"
)

const tunPath = "/dev/net/tun"

func Report() Result {
	var result Result
	fail := func(format string, args ...any) {
		if result.Detail == "" {
			result.Detail = fmt.Sprintf(format, args...)
		}
	}

	if err := openTun(); err != nil {
		result.TunDevice = flag(false)
		fail("cannot open %s: %v", tunPath, err)
	} else {
		result.TunDevice = flag(true)
	}

	if netAdmin, err := hasNetAdmin(); err != nil {
		fail("cannot read the core's capabilities: %v", err)
	} else {
		result.NetAdmin = flag(netAdmin)
		if !netAdmin {
			fail("the core does not have CAP_NET_ADMIN")
		}
	}

	switch {
	case os.Geteuid() != 0:
		result.KillSwitchPrivilege = flag(false)
		fail("the kill switch needs the core to run as root (a setuid-root core); capabilities alone are not enough")
	case result.NetAdmin != nil:
		result.KillSwitchPrivilege = flag(*result.NetAdmin)
	}
	return result
}

// Opening without TUNSETIFF creates nothing: it only proves the device exists and the cgroup lets the core use it.
func openTun() error {
	for {
		fd, err := unix.Open(tunPath, unix.O_RDWR|unix.O_CLOEXEC, 0)
		if errors.Is(err, unix.EINTR) {
			continue
		}
		if err != nil {
			return err
		}
		_ = unix.Close(fd)
		return nil
	}
}

func hasNetAdmin() (bool, error) {
	header := unix.CapUserHeader{Version: unix.LINUX_CAPABILITY_VERSION_3}
	var data [2]unix.CapUserData
	if err := unix.Capget(&header, &data[0]); err != nil {
		return false, err
	}
	return data[unix.CAP_NET_ADMIN/32].Effective&(1<<(unix.CAP_NET_ADMIN%32)) != 0, nil
}

// nfnetlink refuses every request without CAP_NET_ADMIN, so a permission error says nothing about the kernel.
func nftablesUsable() (*bool, string) {
	conn, err := nftables.New()
	if err == nil {
		_, err = conn.ListTablesOfFamily(nftables.TableFamilyINet)
	}
	switch {
	case err == nil:
		return flag(true), ""
	case errors.Is(err, unix.EPERM), errors.Is(err, unix.EACCES):
		return nil, fmt.Sprintf("list the nftables tables: %v", err)
	}
	return flag(false), fmt.Sprintf("list the nftables tables: %v", err)
}
