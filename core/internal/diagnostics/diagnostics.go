package diagnostics

import (
	"archive/zip"
	"bytes"
	"context"
	"encoding/json"
	"errors"
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

	C "github.com/sagernet/sing-box/constant"
	"github.com/xtls/xray-core/core"
)

const (
	MaxWindow          = 1 * time.Minute
	maxAttachmentBytes = 32 << 20
	traceMaxBytes      = 32 << 20
	mutexFraction      = 10
	blockRate          = 100 * time.Microsecond
)

const readme = `Throne core diagnostics

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

var ErrBusy = errors.New("a diagnostics capture is already running")

// Profiling is process-wide, so one capture at a time per process.
var captureMu sync.Mutex

var (
	stopMu sync.Mutex
	stop   context.CancelFunc
)

var processStartedAt = time.Now()

type Attachment struct {
	Name string
	Data []byte
}

type Request struct {
	Window     time.Duration
	Contention bool
	Trace      bool
	// Zipped under app/ by base name.
	Attachments []Attachment
	// Recorded in meta.json; nil records false.
	BoxRunning func() bool
}

type meta struct {
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

type archive struct {
	zw       *zip.Writer
	failures []string
}

func (a *archive) fail(name string, err error) {
	a.failures = append(a.failures, name+": "+err.Error())
}

func (a *archive) add(name string, write func(io.Writer) error) {
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

func (a *archive) addBytes(name string, data []byte) {
	a.add(name, func(w io.Writer) error {
		_, err := w.Write(data)
		return err
	})
}

func (a *archive) addProfile(name, profile string, debugLevel int) {
	a.add(name, func(w io.Writer) error {
		return pprof.Lookup(profile).WriteTo(w, debugLevel)
	})
}

// The heap profile is only as fresh as the last GC.
func (a *archive) addHeap(name string) {
	runtime.GC()
	a.addProfile(name, "heap", 0)
}

func setStop(cancel context.CancelFunc) {
	stopMu.Lock()
	defer stopMu.Unlock()
	stop = cancel
}

// Stop ends a running capture's window early; the archive is still written.
func Stop() {
	stopMu.Lock()
	defer stopMu.Unlock()
	if stop != nil {
		stop()
	}
}

// Capture profiles the process for the window (capped at MaxWindow) or until Stop, and writes the zip to w.
func Capture(w io.Writer, request Request) error {
	window := min(request.Window, MaxWindow)
	if window <= 0 {
		return errors.New("the capture window must be positive")
	}
	if !captureMu.TryLock() {
		return ErrBusy
	}
	defer captureMu.Unlock()

	var attachedBytes int
	for _, attachment := range request.Attachments {
		attachedBytes += len(attachment.Data)
	}
	if attachedBytes > maxAttachmentBytes {
		return fmt.Errorf("attachments exceed %d MiB", maxAttachmentBytes>>20)
	}

	windowCtx, cancel := context.WithCancel(context.Background())
	defer cancel()
	setStop(cancel)
	defer setStop(nil)

	info := newMeta(request.BoxRunning)
	info.Capture.RequestedMs = window.Milliseconds()
	info.Capture.Contention = request.Contention
	info.Capture.Trace = request.Trace

	out := &archive{zw: zip.NewWriter(w)}
	out.addBytes("README.txt", []byte(readme))
	out.add("metrics-start.txt", writeRuntimeMetrics)
	info.Runtime.GoroutinesStart = runtime.NumGoroutine()
	out.addHeap("heap-start.pprof")

	elapsed, stoppedEarly := captureWindow(windowCtx, out, window, request.Contention, request.Trace)
	info.Capture.ActualMs = elapsed.Milliseconds()
	info.Capture.StoppedEarly = stoppedEarly

	out.addProfile("goroutines.pprof", "goroutine", 0)
	out.addProfile("goroutines.txt", "goroutine", 2)
	// Nil before Go 1.27 without GOEXPERIMENT=goroutineleakprofile; writing it runs a GC.
	if pprof.Lookup("goroutineleak") != nil {
		out.addProfile("goroutineleak.pprof", "goroutineleak", 0)
	}
	out.addHeap("heap-end.pprof")
	out.add("metrics-end.txt", writeRuntimeMetrics)
	info.Runtime.GoroutinesEnd = runtime.NumGoroutine()

	for _, attachment := range request.Attachments {
		if name := attachmentName(attachment.Name); name != "" {
			out.addBytes("app/"+name, attachment.Data)
		}
	}

	info.Capture.Errors = out.failures
	encoded, err := json.MarshalIndent(info, "", "  ")
	if err != nil {
		return err
	}
	out.addBytes("meta.json", encoded)
	return out.zw.Close()
}

// Stops are deferred: a recovered panic must not leave a profiler running.
func captureWindow(ctx context.Context, out *archive, window time.Duration,
	contention, withTrace bool) (elapsed time.Duration, stoppedEarly bool) {
	if contention {
		previousFraction := runtime.SetMutexProfileFraction(mutexFraction)
		runtime.SetBlockProfileRate(int(blockRate))
		defer func() {
			runtime.SetMutexProfileFraction(previousFraction)
			runtime.SetBlockProfileRate(0)
		}()
		out.addProfile("mutex-start.pprof", "mutex", 0)
		out.addProfile("block-start.pprof", "block", 0)
	}

	var recorder *trace.FlightRecorder
	if withTrace {
		recorder = trace.NewFlightRecorder(trace.FlightRecorderConfig{MinAge: window, MaxBytes: traceMaxBytes})
		if err := recorder.Start(); err != nil {
			out.fail("trace.out", err)
			recorder = nil
		} else {
			defer recorder.Stop()
		}
	}

	var cpuProfile bytes.Buffer
	var stopCPU func()
	if err := pprof.StartCPUProfile(&cpuProfile); err != nil {
		out.fail("cpu.pprof", err)
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
		out.addBytes("cpu.pprof", cpuProfile.Bytes())
	}
	if recorder != nil {
		// WriteTo refuses a stopped recorder.
		var traced bytes.Buffer
		if _, err := recorder.WriteTo(&traced); err != nil {
			out.fail("trace.out", err)
		} else {
			out.addBytes("trace.out", traced.Bytes())
		}
		recorder.Stop()
	}
	if contention {
		out.addProfile("mutex-end.pprof", "mutex", 0)
		out.addProfile("block-end.pprof", "block", 0)
	}
	return
}

func newMeta(boxRunning func() bool) *meta {
	info := &meta{Created: time.Now().Format(time.RFC3339)}
	info.Versions.SingBox = C.Version
	info.Versions.Xray = core.Version()
	info.Versions.Go = runtime.Version()
	if build, ok := runtimeDebug.ReadBuildInfo(); ok {
		info.Build.Path = build.Main.Path
		info.Build.Version = build.Main.Version
		info.Build.Settings = make(map[string]string, len(build.Settings))
		for _, setting := range build.Settings {
			info.Build.Settings[setting.Key] = setting.Value
		}
	}
	info.Runtime.GOOS = runtime.GOOS
	info.Runtime.GOARCH = runtime.GOARCH
	info.Runtime.NumCPU = runtime.NumCPU()
	info.Runtime.GOMAXPROCS = runtime.GOMAXPROCS(0)
	info.Runtime.GOMEMLIMIT = runtimeDebug.SetMemoryLimit(-1)
	info.Runtime.PID = os.Getpid()
	info.Runtime.EUID = os.Geteuid()
	info.Runtime.UptimeSeconds = time.Since(processStartedAt).Seconds()
	info.Box.Running = boxRunning != nil && boxRunning()
	return info
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
