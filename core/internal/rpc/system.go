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
	response := &gen.IsPrivilegedResponse{
		HasPrivilege:        To(runtime.GOOS != "windows" && (os.Geteuid() == 0 || hasTunCapabilities())),
		TunDevice:           report.TunDevice,
		NetAdmin:            report.NetAdmin,
		KillSwitchPrivilege: report.KillSwitchPrivilege,
	}
	if report.Detail != "" {
		response.Detail = To(report.Detail)
	}
	return response, nil
}
