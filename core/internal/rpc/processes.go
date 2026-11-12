package rpc

import (
	"context"

	"ThroneCore/gen"
	"ThroneCore/internal/proclist"
)

func (s *server) ListProcesses(ctx context.Context, _ *gen.EmptyReq) (*gen.ListProcessesResponse, error) {
	entries, err := proclist.List()
	response := &gen.ListProcessesResponse{Processes: make([]*gen.ProcessEntry, 0, len(entries))}
	for _, entry := range entries {
		response.Processes = append(response.Processes, &gen.ProcessEntry{
			Pid:     To(entry.PID),
			Name:    To(entry.Name),
			Path:    To(entry.Path),
			Service: To(entry.Service),
		})
	}
	if err != nil {
		response.Error = To(err.Error())
	}
	return response, nil
}
