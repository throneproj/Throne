package torbridges

import (
	"bytes"
	"context"
	"encoding/json"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/url"
	"slices"
	"strconv"
	"strings"
	"sync"
	"time"
	"unicode"

	pt "gitlab.torproject.org/tpo/anti-censorship/pluggable-transports/goptlib"
	"gitlab.torproject.org/tpo/anti-censorship/pluggable-transports/lyrebird/transports/meeklite"
)

const (
	maxMoatAnswer   = 1 << 20
	maxMoatSettings = 16
	moatContentType = "application/vnd.api+json"
	// Moat answers HTTP 200 with this code when it cannot place the request's address.
	moatCannotLocate = 406
	moatLinger       = 2 * time.Second
)

type DialFunc func(ctx context.Context, network, address string) (net.Conn, error)

type MoatRequest struct {
	// Lowercase alpha-2, or empty to let the service locate the request.
	Country    string
	Transports []string
}

type MoatAnswer struct {
	Country  string
	Settings []Setting
}

// ServiceError is the service's own refusal: it was reached, so asking it another way would not help.
type ServiceError struct {
	Code   int
	Detail string
}

func (e *ServiceError) Error() string {
	return fmt.Sprintf("%s (%d)", e.Detail, e.Code)
}

type settingsBody struct {
	Transports []string `json:"transports"`
	Country    string   `json:"country,omitempty"`
}

type defaultsBody struct {
	Transports []string `json:"transports"`
}

type moatResponse struct {
	Settings []struct {
		Bridges struct {
			Type          string   `json:"type"`
			Source        string   `json:"source"`
			BridgeStrings []string `json:"bridge_strings"`
		} `json:"bridges"`
	} `json:"settings"`
	Country string `json:"country"`
	Errors  []struct {
		Code   json.RawMessage `json:"code"`
		Detail string          `json:"detail"`
	} `json:"errors"`
}

// AskMoat asks the bridge service through its domain-fronted meek tunnel; dial reaches the fronts.
// Errors other than *ServiceError mean the service gave no usable answer.
func (d *Document) AskMoat(ctx context.Context, request MoatRequest, dial DialFunc) (*MoatAnswer, error) {
	session, err := newMoatSession(ctx, d.Moat, dial)
	if err != nil {
		return nil, err
	}
	defer session.close()

	answer := &MoatAnswer{Country: request.Country}
	response, err := session.post(ctx, "settings", settingsBody{Transports: request.Transports, Country: request.Country})
	if err != nil {
		return nil, err
	}
	if country, err := NormalizeCountry(response.Country); err == nil && country != "" {
		answer.Country = country
	}
	if refusal := response.refusal(); refusal != nil {
		if refusal.Code != moatCannotLocate {
			return nil, refusal
		}
	} else if answer.Settings = d.convert(response, request.Transports); len(answer.Settings) > 0 {
		return answer, nil
	}
	// Like Tor Browser: a country that needs nothing special, or no country at all, gets the general defaults.
	if response, err = session.post(ctx, "defaults", defaultsBody{Transports: request.Transports}); err != nil {
		return nil, err
	}
	if refusal := response.refusal(); refusal != nil {
		return nil, refusal
	}
	if answer.Settings = d.convert(response, request.Transports); len(answer.Settings) == 0 {
		answer.Settings = d.fallback(request.Transports)
	}
	return answer, nil
}

// Built-in entries get this document's lines: the service's copies of them have been seen shuffled and repeated.
func (d *Document) convert(response *moatResponse, transports []string) []Setting {
	var settings []Setting
	for _, entry := range response.Settings[:min(len(response.Settings), maxMoatSettings)] {
		bridges := entry.Bridges
		if !slices.Contains(transports, bridges.Type) {
			continue
		}
		var lines []string
		switch bridges.Source {
		case SourceBuiltin:
			if lines = slices.Clone(d.builtinLines(bridges.Type)); len(lines) == 0 {
				lines = usableLines(bridges.Type, bridges.BridgeStrings)
			}
		case SourceBridgeDB:
			lines = usableLines(bridges.Type, bridges.BridgeStrings)
		}
		if len(lines) > 0 {
			settings = append(settings, Setting{Type: bridges.Type, Source: bridges.Source, Bridges: lines})
		}
	}
	return dedupe(settings)
}

func usableLines(transportType string, lines []string) []string {
	var usable []string
	for _, line := range lines[:min(len(lines), maxLinesPerSetting)] {
		line = strings.Join(strings.Fields(line), " ")
		if transport, err := checkLine(line); err == nil && transport == lineTransport[transportType] && !refusedLine(transportType, line) {
			usable = append(usable, line)
		}
	}
	return usable
}

func (m *moatResponse) refusal() *ServiceError {
	if len(m.Errors) == 0 {
		return nil
	}
	code, _ := strconv.Atoi(strings.Trim(string(m.Errors[0].Code), `"`))
	detail := strings.Map(func(r rune) rune {
		if unicode.IsControl(r) {
			return -1
		}
		return r
	}, m.Errors[0].Detail)
	if runes := []rune(detail); len(runes) > 200 {
		detail = string(runes[:200])
	}
	if detail == "" {
		detail = "the bridge service refused the request"
	}
	return &ServiceError{Code: code, Detail: detail}
}

type moatSession struct {
	endpoint  string
	client    *http.Client
	transport *http.Transport
	fronts    *connTracker
	stopWatch func() bool
}

// The service's own TLS runs inside the tunnel, so its certificate is checked against the system roots as usual.
func newMoatSession(ctx context.Context, moat Moat, dial DialFunc) (*moatSession, error) {
	endpoint, err := url.Parse(moat.URL)
	if err != nil {
		return nil, err
	}
	factory, err := (&meeklite.Transport{}).ClientFactory("")
	if err != nil {
		return nil, err
	}
	fronts := &connTracker{conns: make(map[net.Conn]struct{})}
	dialFront := func(network, address string) (net.Conn, error) {
		// meek_lite retries a failed poll up to ten times, also after the session ended.
		if fronts.isRefusing() {
			return nil, net.ErrClosed
		}
		conn, err := dial(ctx, network, address)
		if err != nil {
			fronts.failed(err)
			return nil, err
		}
		if !fronts.add(conn) {
			conn.Close()
			return nil, net.ErrClosed
		}
		return conn, nil
	}
	transport := &http.Transport{
		DialContext: func(_ context.Context, network, _ string) (net.Conn, error) {
			// Per tunnel: meek_lite rotates through the fronts inside its parsed arguments.
			args, err := factory.ParseArgs(&pt.Args{"targets": []string{moat.Targets}})
			if err != nil {
				return nil, err
			}
			// meek_lite ignores the address; the tunnel always ends at the service.
			return factory.Dial(network, net.JoinHostPort(endpoint.Hostname(), "443"), dialFront, args)
		},
		MaxIdleConns:        1,
		IdleConnTimeout:     time.Minute,
		TLSHandshakeTimeout: 30 * time.Second,
	}
	return &moatSession{
		endpoint:  moat.URL,
		transport: transport,
		client: &http.Client{
			Transport: transport,
			CheckRedirect: func(*http.Request, []*http.Request) error {
				return http.ErrUseLastResponse
			},
		},
		fronts: fronts,
		// meek_lite's TLS handshakes to the fronts take no deadline; closing their sockets is what stops them.
		stopWatch: context.AfterFunc(ctx, fronts.closeAll),
	}, nil
}

// meek_lite's worker finishes a poll in flight before it sees the close; cutting its sockets under it makes it retry ten times.
func (s *moatSession) close() {
	s.transport.CloseIdleConnections()
	s.stopWatch()
	s.fronts.refuse()
	time.AfterFunc(moatLinger, s.fronts.closeAll)
}

func (s *moatSession) post(ctx context.Context, method string, body any) (*moatResponse, error) {
	payload, err := json.Marshal(body)
	if err != nil {
		return nil, err
	}
	request, err := http.NewRequestWithContext(ctx, http.MethodPost, s.endpoint+"/circumvention/"+method, bytes.NewReader(payload))
	if err != nil {
		return nil, err
	}
	request.Header.Set("Content-Type", moatContentType)
	response, err := s.client.Do(request)
	if err != nil {
		// meek_lite reports a dead tunnel as a closed pipe; the reason is in how the fronts failed.
		if dialErr := s.fronts.lastFailure(); dialErr != nil {
			return nil, fmt.Errorf("%w (fronts: %w)", err, dialErr)
		}
		return nil, err
	}
	defer response.Body.Close()
	if response.StatusCode != http.StatusOK {
		return nil, fmt.Errorf("%s: HTTP %d", method, response.StatusCode)
	}
	data, err := io.ReadAll(io.LimitReader(response.Body, maxMoatAnswer+1))
	if err != nil {
		return nil, fmt.Errorf("%s: %w", method, err)
	}
	if len(data) > maxMoatAnswer {
		return nil, fmt.Errorf("%s: answer larger than %d bytes", method, maxMoatAnswer)
	}
	var answer moatResponse
	if err = json.Unmarshal(data, &answer); err != nil {
		return nil, fmt.Errorf("%s: invalid answer: %w", method, err)
	}
	return &answer, nil
}

type connTracker struct {
	mu        sync.Mutex
	conns     map[net.Conn]struct{}
	refusing  bool
	dialError error
}

func (t *connTracker) failed(err error) {
	t.mu.Lock()
	defer t.mu.Unlock()
	t.dialError = err
}

func (t *connTracker) lastFailure() error {
	t.mu.Lock()
	defer t.mu.Unlock()
	return t.dialError
}

func (t *connTracker) add(conn net.Conn) bool {
	t.mu.Lock()
	defer t.mu.Unlock()
	if t.refusing {
		return false
	}
	t.conns[conn] = struct{}{}
	return true
}

func (t *connTracker) refuse() {
	t.mu.Lock()
	defer t.mu.Unlock()
	t.refusing = true
}

func (t *connTracker) isRefusing() bool {
	t.mu.Lock()
	defer t.mu.Unlock()
	return t.refusing
}

func (t *connTracker) closeAll() {
	t.mu.Lock()
	conns := t.conns
	t.conns, t.refusing = nil, true
	t.mu.Unlock()
	for conn := range conns {
		conn.Close()
	}
}
