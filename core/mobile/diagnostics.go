package mobile

import (
	"bufio"
	"bytes"
	"os"
	"path/filepath"
	"time"

	"ThroneCore/internal/diagnostics"

	E "github.com/sagernet/sing/common/exceptions"
)

type DiagnosticsRequest struct {
	DurationMs int32
	Contention bool
	Trace      bool

	attachments []diagnostics.Attachment
}

// Zipped under app/ by base name. data is copied: gomobile only lends it for the call.
func (r *DiagnosticsRequest) AddAttachment(name string, data []byte) {
	r.attachments = append(r.attachments, diagnostics.Attachment{Name: name, Data: bytes.Clone(data)})
}

// rpc.CaptureDiagnostics, but the zip goes to outputPath: an archive can be far larger than a binder
// transaction or a comfortable Java byte[]. Blocks for the window (at most 1 min) or until StopDiagnostics.
// Profiles only this process, so it must run in the one that hosts the box.
func CaptureDiagnostics(request *DiagnosticsRequest, outputPath string) error {
	if request == nil {
		return E.New("nil request")
	}
	if outputPath == "" {
		return E.New("missing output path")
	}
	if err := os.MkdirAll(filepath.Dir(outputPath), 0o755); err != nil {
		return err
	}
	file, err := os.CreateTemp(filepath.Dir(outputPath), filepath.Base(outputPath)+".*.partial")
	if err != nil {
		return err
	}
	partial := file.Name()
	writer := bufio.NewWriterSize(file, 1<<20)
	err = diagnostics.Capture(writer, diagnostics.Request{
		Window:      time.Duration(request.DurationMs) * time.Millisecond,
		Contention:  request.Contention,
		Trace:       request.Trace,
		Attachments: request.attachments,
		BoxRunning:  func() bool { return runningInstances.Load() > 0 },
	})
	if err == nil {
		err = writer.Flush()
	}
	if closeErr := file.Close(); err == nil {
		err = closeErr
	}
	if err == nil {
		err = os.Rename(partial, outputPath)
	}
	if err != nil {
		_ = os.Remove(partial)
	}
	return err
}

func StopDiagnostics() {
	diagnostics.Stop()
}
