package scan

import (
	"context"
	"errors"
	"sync"
	"sync/atomic"
	"time"
)

const (
	eventRingSize  = 256
	eventPageSize  = 64
	sessionIdleTTL = 10 * time.Minute
	pruneInterval  = time.Minute
)

// The GUI matches on this text.
var ErrStopped = errors.New("scan stopped")

const (
	EventStart = "start"
	EventOK    = "ok"
	EventFail  = "fail"

	PhaseICMP = "icmp"
	PhaseTCP  = "tcp"
	PhaseTLS  = "tls"
	PhaseHTTP = "http"
	PhaseDNS  = "dns"
	PhaseURL  = "url"
)

type Event struct {
	Seq       int64
	Kind      string
	Phase     string
	Target    string
	LatencyMs int32
	Error     string
}

type Counters struct {
	Probed      int64
	ProbePassed int64
	URLTested   int64
	URLPassed   int64
}

type Session struct {
	ctx    context.Context
	cancel context.CancelFunc

	probed      atomic.Int64
	probePassed atomic.Int64
	urlTested   atomic.Int64
	urlPassed   atomic.Int64

	inflight atomic.Int32
	lastUsed atomic.Int64

	mu      sync.Mutex
	ring    [eventRingSize]Event
	lastSeq int64
}

type tombstone struct {
	session *Session
	at      time.Time
}

var registry = struct {
	mu        sync.Mutex
	sessions  map[string]*Session
	stopped   map[string]tombstone
	lastPrune time.Time
}{
	sessions: make(map[string]*Session),
	stopped:  make(map[string]tombstone),
}

func newSession() *Session {
	s := &Session{}
	s.ctx, s.cancel = context.WithCancel(context.Background())
	s.touch()
	return s
}

func Acquire(id string) (*Session, func(), error) {
	if id == "" {
		return nil, nil, errors.New("missing scan session id")
	}
	registry.mu.Lock()
	defer registry.mu.Unlock()
	pruneLocked(time.Now())
	if _, stopped := registry.stopped[id]; stopped {
		return nil, nil, ErrStopped
	}
	s := registry.sessions[id]
	if s == nil {
		s = newSession()
		registry.sessions[id] = s
	}
	s.inflight.Add(1)
	s.touch()
	return s, func() {
		s.touch()
		s.inflight.Add(-1)
	}, nil
}

// Read-only polling: a stopped session still reports its final events and counters.
func Lookup(id string) *Session {
	if id == "" {
		return nil
	}
	registry.mu.Lock()
	defer registry.mu.Unlock()
	pruneLocked(time.Now())
	if stone, stopped := registry.stopped[id]; stopped {
		return stone.session
	}
	s := registry.sessions[id]
	if s == nil {
		s = newSession()
		registry.sessions[id] = s
	}
	s.touch()
	return s
}

// Every later Acquire of the same id is refused.
func Stop(id string) {
	if id == "" {
		return
	}
	registry.mu.Lock()
	defer registry.mu.Unlock()
	now := time.Now()
	pruneLocked(now)
	s := registry.sessions[id]
	delete(registry.sessions, id)
	if s != nil {
		s.cancel()
	} else if stone, stopped := registry.stopped[id]; stopped {
		s = stone.session
	}
	registry.stopped[id] = tombstone{session: s, at: now}
}

func pruneLocked(now time.Time) {
	if now.Sub(registry.lastPrune) < pruneInterval {
		return
	}
	registry.lastPrune = now
	for id, s := range registry.sessions {
		if s.inflight.Load() == 0 && now.Sub(time.Unix(0, s.lastUsed.Load())) > sessionIdleTTL {
			s.cancel()
			delete(registry.sessions, id)
		}
	}
	for id, stone := range registry.stopped {
		if now.Sub(stone.at) > sessionIdleTTL {
			delete(registry.stopped, id)
		}
	}
}

func (s *Session) touch() {
	s.lastUsed.Store(time.Now().UnixNano())
}

func (s *Session) Context() context.Context {
	return s.ctx
}

// emit and recordProbe are nil-safe: Probe callers without live reporting pass a nil session.
func (s *Session) emit(kind, phase, target string, latency time.Duration, errText string) {
	if s == nil {
		return
	}
	s.mu.Lock()
	defer s.mu.Unlock()
	s.lastSeq++
	s.ring[s.lastSeq%eventRingSize] = Event{
		Seq:       s.lastSeq,
		Kind:      kind,
		Phase:     phase,
		Target:    target,
		LatencyMs: int32(latency.Milliseconds()),
		Error:     errText,
	}
}

func (s *Session) recordProbe(passed bool) {
	if s == nil {
		return
	}
	s.probed.Add(1)
	if passed {
		s.probePassed.Add(1)
	}
}

func (s *Session) RecordURL(target string, latency time.Duration, err error) {
	s.urlTested.Add(1)
	if err != nil {
		s.emit(EventFail, PhaseURL, target, 0, errorText(err))
		return
	}
	s.urlPassed.Add(1)
	s.emit(EventOK, PhaseURL, target, latency, "")
}

func (s *Session) Events(after int64) ([]Event, int64) {
	s.mu.Lock()
	defer s.mu.Unlock()
	// Checked before after+1, which overflows for the GUI's counters-only query (after = MaxInt64).
	if after >= s.lastSeq {
		return nil, s.lastSeq
	}
	first := max(after+1, s.lastSeq-eventPageSize+1, 1)
	events := make([]Event, 0, s.lastSeq-first+1)
	for seq := first; seq <= s.lastSeq; seq++ {
		events = append(events, s.ring[seq%eventRingSize])
	}
	return events, s.lastSeq
}

func (s *Session) Counters() Counters {
	return Counters{
		Probed:      s.probed.Load(),
		ProbePassed: s.probePassed.Load(),
		URLTested:   s.urlTested.Load(),
		URLPassed:   s.urlPassed.Load(),
	}
}
