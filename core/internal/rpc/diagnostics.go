package rpc

import (
	"archive/zip"
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"math"
	"os"
	"path"
	"runtime"
	runtimeDebug "runtime/debug"
	"runtime/metrics"
	"runtime/pprof"
	"runtime/trace"
	"strings"
	"sync"
	"time"

	"ThroneCore/gen"

	C "github.com/sagernet/sing-box/constant"
	"github.com/xtls/xray-core/core"
)

const (
	diagnosticsMaxWindow          = 1 * time.Minute
	diagnosticsMaxAttachmentBytes = 32 << 20
	diagnosticsTraceMaxBytes      = 32 << 20
	diagnosticsMutexFraction      = 10
	diagnosticsBlockRate          = 100 * time.Microsecond
)

const diagnosticsReadme = `Throne core diagnostics

cpu.pprof             go tool pprof -http=: cpu.pprof
heap-start.pprof      go tool pprof -http=: -diff_base heap-start.pprof heap-end.pprof
goroutines.pprof      go tool pprof -top goroutines.pprof (taken when the capture ends; full stacks in goroutines.txt)
goroutineleak.pprof   go tool pprof -top goroutineleak.pprof
mutex-start.pprof     go tool pprof -http=: -diff_base mutex-start.pprof mutex-end.pprof (block-*.pprof likewise)
trace.out             go tool trace trace.out (needs a Go toolchain at least as new as versions.go in meta.json)
metrics-*.txt         runtime/metrics before and after the capture
meta.json             versions, build and runtime details, what the capture did and anything that failed
app/                  files attached by Throne

Files the capture did not ask for, or that this runtime cannot produce, are absent.
`

var diagnosticsMu sync.Mutex

var (
	diagnosticsStopMu sync.Mutex
	diagnosticsStop   context.CancelFunc
)

var processStartedAt = time.Now()

type diagnosticsMeta struct {
	Created string `json:"created"`
	Capture struct {
		RequestedMs  int64    `json:"requested_ms"`
		ActualMs     int64    `json:"actual_ms"`
		StoppedEarly bool     `json:"stopped_early"`
		Contention   bool     `json:"contention"`
		Trace        bool     `json:"trace"`
		Errors       []string `json:"errors"`
	} `json:"capture"`
	Versions struct {
		SingBox string `json:"sing_box"`
		Xray    string `json:"xray"`
		Go      string `json:"go"`
	} `json:"versions"`
	Build struct {
		Path     string            `json:"path"`
		Version  string            `json:"version"`
		Settings map[string]string `json:"settings"`
	} `json:"build"`
	Runtime struct {
		GOOS            string  `json:"goos"`
		GOARCH          string  `json:"goarch"`
		NumCPU          int     `json:"num_cpu"`
		GOMAXPROCS      int     `json:"gomaxprocs"`
		GOMEMLIMIT      int64   `json:"gomemlimit"`
		GoroutinesStart int     `json:"goroutines_start"`
		GoroutinesEnd   int     `json:"goroutines_end"`
		PID             int     `json:"pid"`
		EUID            int     `json:"euid"`
		UptimeSeconds   float64 `json:"uptime_seconds"`
	} `json:"runtime"`
	Box struct {
		Running bool `json:"running"`
	} `json:"box"`
}

type diagnosticsArchive struct {
	zw       *zip.Writer
	failures []string
}

func (a *diagnosticsArchive) fail(name string, err error) {
	a.failures = append(a.failures, name+": "+err.Error())
}

func (a *diagnosticsArchive) add(name string, write func(io.Writer) error) {
	method := zip.Deflate
	if strings.HasSuffix(name, ".pprof") {
		method = zip.Store // already gzip
	}
	w, err := a.zw.CreateHeader(&zip.FileHeader{Name: name, Method: method, Modified: time.Now()})
	if err == nil {
		err = write(w)
	}
	if err != nil {
		a.fail(name, err)
	}
}

func (a *diagnosticsArchive) addBytes(name string, data []byte) {
	a.add(name, func(w io.Writer) error {
		_, err := w.Write(data)
		return err
	})
}

func (a *diagnosticsArchive) addProfile(name, profile string, debugLevel int) {
	a.add(name, func(w io.Writer) error {
		return pprof.Lookup(profile).WriteTo(w, debugLevel)
	})
}

// The heap profile is only as fresh as the last GC.
func (a *diagnosticsArchive) addHeap(name string) {
	runtime.GC()
	a.addProfile(name, "heap", 0)
}

func setDiagnosticsStop(stop context.CancelFunc) {
	diagnosticsStopMu.Lock()
	defer diagnosticsStopMu.Unlock()
	diagnosticsStop = stop
}

// Never takes lifecycleMu: a wedged Start holds it exactly when the dump is needed.
func (s *server) CaptureDiagnostics(ctx context.Context, in *gen.DiagnosticsRequest) (*gen.DiagnosticsResponse, error) {
	window := min(time.Duration(in.GetDurationMs())*time.Millisecond, diagnosticsMaxWindow)
	if window <= 0 {
		return &gen.DiagnosticsResponse{Error: To("duration_ms must be positive")}, nil
	}
	if !diagnosticsMu.TryLock() {
		return &gen.DiagnosticsResponse{Error: To("a diagnostics capture is already running")}, nil
	}
	defer diagnosticsMu.Unlock()

	var attachedBytes int
	for _, attachment := range in.GetAttachments() {
		attachedBytes += len(attachment.GetData())
	}
	if attachedBytes > diagnosticsMaxAttachmentBytes {
		return &gen.DiagnosticsResponse{Error: To(fmt.Sprintf("attachments exceed %d MiB", diagnosticsMaxAttachmentBytes>>20))}, nil
	}

	windowCtx, stop := context.WithCancel(context.Background())
	defer stop()
	setDiagnosticsStop(stop)
	defer setDiagnosticsStop(nil)

	meta := newDiagnosticsMeta()
	meta.Capture.RequestedMs = window.Milliseconds()
	meta.Capture.Contention = in.GetContention()
	meta.Capture.Trace = in.GetTrace()

	var buf bytes.Buffer
	archive := &diagnosticsArchive{zw: zip.NewWriter(&buf)}
	archive.addBytes("README.txt", []byte(diagnosticsReadme))
	archive.add("metrics-start.txt", writeRuntimeMetrics)
	meta.Runtime.GoroutinesStart = runtime.NumGoroutine()
	archive.addHeap("heap-start.pprof")

	elapsed, stoppedEarly := captureDiagnosticsWindow(windowCtx, archive, window, meta.Capture.Contention, meta.Capture.Trace)
	meta.Capture.ActualMs = elapsed.Milliseconds()
	meta.Capture.StoppedEarly = stoppedEarly

	archive.addProfile("goroutines.pprof", "goroutine", 0)
	archive.addProfile("goroutines.txt", "goroutine", 2)
	// Nil before Go 1.27 without GOEXPERIMENT=goroutineleakprofile; writing it runs a GC.
	if pprof.Lookup("goroutineleak") != nil {
		archive.addProfile("goroutineleak.pprof", "goroutineleak", 0)
	}
	archive.addHeap("heap-end.pprof")
	archive.add("metrics-end.txt", writeRuntimeMetrics)
	meta.Runtime.GoroutinesEnd = runtime.NumGoroutine()

	for _, attachment := range in.GetAttachments() {
		if name := attachmentName(attachment.GetName()); name != "" {
			archive.addBytes("app/"+name, attachment.GetData())
		}
	}

	meta.Capture.Errors = archive.failures
	encoded, err := json.MarshalIndent(meta, "", "  ")
	if err != nil {
		return nil, err
	}
	archive.addBytes("meta.json", encoded)
	if err = archive.zw.Close(); err != nil {
		return nil, err
	}
	return &gen.DiagnosticsResponse{Archive: buf.Bytes()}, nil
}

func (s *server) StopDiagnostics(ctx context.Context, in *gen.EmptyReq) (*gen.EmptyResp, error) {
	diagnosticsStopMu.Lock()
	defer diagnosticsStopMu.Unlock()
	if diagnosticsStop != nil {
		diagnosticsStop()
	}
	return &gen.EmptyResp{}, nil
}

// Stops are deferred: a panic recovered by dispatch must not leave a profiler running.
func captureDiagnosticsWindow(ctx context.Context, archive *diagnosticsArchive, window time.Duration,
	contention, withTrace bool) (elapsed time.Duration, stoppedEarly bool) {
	if contention {
		previousFraction := runtime.SetMutexProfileFraction(diagnosticsMutexFraction)
		runtime.SetBlockProfileRate(int(diagnosticsBlockRate))
		defer func() {
			runtime.SetMutexProfileFraction(previousFraction)
			runtime.SetBlockProfileRate(0)
		}()
		archive.addProfile("mutex-start.pprof", "mutex", 0)
		archive.addProfile("block-start.pprof", "block", 0)
	}

	var recorder *trace.FlightRecorder
	if withTrace {
		recorder = trace.NewFlightRecorder(trace.FlightRecorderConfig{MinAge: window, MaxBytes: diagnosticsTraceMaxBytes})
		if err := recorder.Start(); err != nil {
			archive.fail("trace.out", err)
			recorder = nil
		} else {
			defer recorder.Stop()
		}
	}

	var cpuProfile bytes.Buffer
	var stopCPU func()
	if err := pprof.StartCPUProfile(&cpuProfile); err != nil {
		archive.fail("cpu.pprof", err)
	} else {
		stopCPU = sync.OnceFunc(pprof.StopCPUProfile)
		defer stopCPU()
	}

	started := time.Now()
	select {
	case <-time.After(window):
	case <-ctx.Done():
		stoppedEarly = true
	}
	elapsed = time.Since(started)

	if stopCPU != nil {
		stopCPU()
		archive.addBytes("cpu.pprof", cpuProfile.Bytes())
	}
	if recorder != nil {
		// WriteTo refuses a stopped recorder.
		var traced bytes.Buffer
		if _, err := recorder.WriteTo(&traced); err != nil {
			archive.fail("trace.out", err)
		} else {
			archive.addBytes("trace.out", traced.Bytes())
		}
		recorder.Stop()
	}
	if contention {
		archive.addProfile("mutex-end.pprof", "mutex", 0)
		archive.addProfile("block-end.pprof", "block", 0)
	}
	return
}

func newDiagnosticsMeta() *diagnosticsMeta {
	meta := &diagnosticsMeta{Created: time.Now().Format(time.RFC3339)}
	meta.Versions.SingBox = C.Version
	meta.Versions.Xray = core.Version()
	meta.Versions.Go = runtime.Version()
	if info, ok := runtimeDebug.ReadBuildInfo(); ok {
		meta.Build.Path = info.Main.Path
		meta.Build.Version = info.Main.Version
		meta.Build.Settings = make(map[string]string, len(info.Settings))
		for _, setting := range info.Settings {
			meta.Build.Settings[setting.Key] = setting.Value
		}
	}
	meta.Runtime.GOOS = runtime.GOOS
	meta.Runtime.GOARCH = runtime.GOARCH
	meta.Runtime.NumCPU = runtime.NumCPU()
	meta.Runtime.GOMAXPROCS = runtime.GOMAXPROCS(0)
	meta.Runtime.GOMEMLIMIT = runtimeDebug.SetMemoryLimit(-1)
	meta.Runtime.PID = os.Getpid()
	meta.Runtime.EUID = os.Geteuid()
	meta.Runtime.UptimeSeconds = time.Since(processStartedAt).Seconds()
	meta.Box.Running = currentBox() != nil
	return meta
}

func writeRuntimeMetrics(w io.Writer) error {
	descriptions := metrics.All()
	samples := make([]metrics.Sample, len(descriptions))
	for i, description := range descriptions {
		samples[i].Name = description.Name
	}
	metrics.Read(samples)
	for _, sample := range samples {
		var err error
		switch sample.Value.Kind() {
		case metrics.KindUint64:
			_, err = fmt.Fprintf(w, "%s %d\n", sample.Name, sample.Value.Uint64())
		case metrics.KindFloat64:
			_, err = fmt.Fprintf(w, "%s %g\n", sample.Name, sample.Value.Float64())
		case metrics.KindFloat64Histogram:
			_, err = fmt.Fprintf(w, "%s %s\n", sample.Name, histogramSummary(sample.Value.Float64Histogram()))
		}
		if err != nil {
			return err
		}
	}
	return nil
}

func histogramSummary(histogram *metrics.Float64Histogram) string {
	var total uint64
	for _, count := range histogram.Counts {
		total += count
	}
	if total == 0 {
		return "count=0"
	}
	return fmt.Sprintf("count=%d p50=%g p99=%g max=%g", total, histogramQuantile(histogram, total, 0.5),
		histogramQuantile(histogram, total, 0.99), histogramQuantile(histogram, total, 1))
}

func histogramQuantile(histogram *metrics.Float64Histogram, total uint64, quantile float64) float64 {
	rank := uint64(math.Ceil(quantile * float64(total)))
	var seen uint64
	for i, count := range histogram.Counts {
		seen += count
		if seen < rank {
			continue
		}
		if upper := histogram.Buckets[i+1]; !math.IsInf(upper, 1) {
			return upper
		}
		return histogram.Buckets[i]
	}
	return math.NaN()
}

// Base name only, so a name carrying a path cannot escape app/.
func attachmentName(name string) string {
	base := path.Base(strings.ReplaceAll(name, `\`, "/"))
	switch base {
	case ".", "..", "/":
		return ""
	}
	return base
}
