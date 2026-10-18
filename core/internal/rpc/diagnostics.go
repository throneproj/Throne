package rpc

import (
	"bytes"
	"context"
	"time"

	"ThroneCore/gen"
	"ThroneCore/internal/diagnostics"
)

// Never takes lifecycleMu: a wedged Start holds it exactly when the dump is needed.
func (s *server) CaptureDiagnostics(ctx context.Context, in *gen.DiagnosticsRequest) (*gen.DiagnosticsResponse, error) {
	request := diagnostics.Request{
		Window:     time.Duration(in.GetDurationMs()) * time.Millisecond,
		Contention: in.GetContention(),
		Trace:      in.GetTrace(),
		BoxRunning: func() bool { return currentBox() != nil },
	}
	for _, attachment := range in.GetAttachments() {
		request.Attachments = append(request.Attachments, diagnostics.Attachment{
			Name: attachment.GetName(),
			Data: attachment.GetData(),
		})
	}
	var archive bytes.Buffer
	if err := diagnostics.Capture(&archive, request); err != nil {
		return &gen.DiagnosticsResponse{Error: To(err.Error())}, nil
	}
	return &gen.DiagnosticsResponse{Archive: archive.Bytes()}, nil
}

func (s *server) StopDiagnostics(ctx context.Context, in *gen.EmptyReq) (*gen.EmptyResp, error) {
	diagnostics.Stop()
	return &gen.EmptyResp{}, nil
}
