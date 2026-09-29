package mobile

import (
	stdlog "log"
	"os"
	"strings"
	"sync"

	"github.com/sagernet/sing-box/log"
)

const (
	LogLevelPanic = int32(log.LevelPanic)
	LogLevelFatal = int32(log.LevelFatal)
	LogLevelError = int32(log.LevelError)
	LogLevelWarn  = int32(log.LevelWarn)
	LogLevelInfo  = int32(log.LevelInfo)
	LogLevelDebug = int32(log.LevelDebug)
	LogLevelTrace = int32(log.LevelTrace)
)

// LogSink receives sing-box lines (already formatted by the box's platform formatter) with their
// log.Level, and Go std log lines (Xray gates, probes, close timing) as LogLevelInfo.
type LogSink interface {
	Write(level int32, message string)
}

const defaultLogQueueLines = 1024

type logEntry struct {
	level   int32
	message string
}

// Lines are queued and delivered by one goroutine so a slow Kotlin sink never stalls sing-box's
// logging path; past the limit the oldest queued line is dropped.
type logPump struct {
	access  sync.Mutex
	sink    LogSink
	limit   int
	pending []logEntry
	notify  chan struct{}
	once    sync.Once
}

var pump = &logPump{notify: make(chan struct{}, 1)}

func (p *logPump) setLimit(limit int) {
	p.access.Lock()
	p.limit = limit
	p.access.Unlock()
}

func (p *logPump) setSink(sink LogSink) {
	p.access.Lock()
	p.sink = sink
	p.access.Unlock()
	if sink != nil {
		p.once.Do(func() { go p.run() })
		p.wake()
	}
}

func (p *logPump) push(level int32, message string) {
	p.access.Lock()
	if p.sink == nil {
		p.access.Unlock()
		return
	}
	limit := p.limit
	if limit <= 0 {
		limit = defaultLogQueueLines
	}
	if len(p.pending) >= limit {
		p.pending = p.pending[1:]
	}
	p.pending = append(p.pending, logEntry{level: level, message: message})
	p.access.Unlock()
	p.wake()
}

func (p *logPump) wake() {
	select {
	case p.notify <- struct{}{}:
	default:
	}
}

func (p *logPump) run() {
	for range p.notify {
		for {
			p.access.Lock()
			sink := p.sink
			batch := p.pending
			p.pending = nil
			p.access.Unlock()
			if sink == nil || len(batch) == 0 {
				break
			}
			for _, entry := range batch {
				sink.Write(entry.level, entry.message)
			}
		}
	}
}

func SetLogSink(sink LogSink) {
	pump.setSink(sink)
	if sink == nil {
		stdlog.SetFlags(stdlog.LstdFlags)
		stdlog.SetOutput(os.Stderr)
		return
	}
	stdlog.SetFlags(0)
	stdlog.SetOutput(stdLogWriter{})
}

type stdLogWriter struct{}

func (stdLogWriter) Write(p []byte) (int, error) {
	if message := strings.TrimRight(string(p), "\r\n"); message != "" {
		pump.push(LogLevelInfo, message)
	}
	return len(p), nil
}

var _ log.PlatformWriter = platformLogWriter{}

// Always handed to box.New for the main instance, sink or not: a non-nil PlatformLogWriter is what
// switches on trafficcontrol, clash mode and the cache file, which Status()/Groups()/QueryOutboundStats()
// read. Probe boxes pass nil so they never share cache.db or pay for accounting.
type platformLogWriter struct{}

func (platformLogWriter) WriteMessage(level log.Level, message string) {
	pump.push(int32(level), message)
}
