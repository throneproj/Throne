package rpc

import (
	"context"
	"os"
	"runtime"

	"ThroneCore/gen"
	"ThroneCore/internal/capcheck"
)

func (s *server) IsPrivileged(ctx context.Context, _ *gen.EmptyReq) (*gen.IsPrivilegedResponse, error) {
	report := capcheck.Report()
	hasPrivilege := os.Geteuid() == 0 || hasTunCapabilities()
	killSwitchPrivilege := report.KillSwitchPrivilege
	detail := report.Detail
	if runtime.GOOS == "windows" {
		hasPrivilege = isElevated()
		killSwitchPrivilege = To(hasPrivilege)
		if !hasPrivilege {
			detail = "the core is not running as administrator"
		}
	}
	response := &gen.IsPrivilegedResponse{
		HasPrivilege:        To(hasPrivilege),
		TunDevice:           report.TunDevice,
		NetAdmin:            report.NetAdmin,
		KillSwitchPrivilege: killSwitchPrivilege,
		CorePid:             To(int32(os.Getpid())),
	}
	if detail != "" {
		response.Detail = To(detail)
	}
	return response, nil
}
