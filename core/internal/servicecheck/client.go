package servicecheck

import (
	"bufio"
	"bytes"
	"compress/flate"
	"compress/gzip"
	"compress/zlib"
	"context"
	"errors"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/http/cookiejar"
	"net/url"
	"strings"
	"sync/atomic"
	"time"

	"github.com/andybalholm/brotli"
	"github.com/klauspost/compress/zstd"
	boxtls "github.com/sagernet/sing-box/common/tls"
	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing/common/logger"
	"golang.org/x/net/http2"
)

type Dialer func(ctx context.Context, network, address string) (net.Conn, error)

// Client fetches the way desktop Chrome would look on the wire as far as a server can tell from
// one request: Chrome's TLS hello, h2 when offered, and a navigation's headers.
type Client struct {
	Dial    Dialer
	Timeout time.Duration
	// The first request may also be starting the tunnel; 0 = Timeout.
	FirstTimeout time.Duration
	// Context for the TLS configs; nil = context.Background().
	Context context.Context

	warm atomic.Bool
}

const (
	DefaultRequestTimeout = 10 * time.Second
	maxRedirects          = 5
	chromeUserAgent       = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/141.0.0.0 Safari/537.36"
)

func (c *Client) hopTimeout() time.Duration {
	timeout := c.Timeout
	if timeout <= 0 {
		timeout = DefaultRequestTimeout
	}
	if !c.warm.Swap(true) && c.FirstTimeout > timeout {
		return c.FirstTimeout
	}
	return timeout
}

// session is one check's view of the web: its cookies and h2 connections end with it.
type session struct {
	client    *Client
	jar       http.CookieJar
	transport *http2.Transport
	h2        map[string]*http2.ClientConn
	conns     []net.Conn
}

func (c *Client) newSession() *session {
	jar, _ := cookiejar.New(nil)
	return &session{
		client:    c,
		jar:       jar,
		transport: &http2.Transport{DisableCompression: true},
		h2:        make(map[string]*http2.ClientConn),
	}
}

func (s *session) close() {
	for _, cc := range s.h2 {
		_ = cc.Close()
	}
	for _, conn := range s.conns {
		_ = conn.Close()
	}
	s.h2 = nil
	s.conns = nil
}

func (s *session) fetch(ctx context.Context, request *Request) *Response {
	out := &Response{}
	target := *request.URL
	method := request.Method
	for hop := 0; ; hop++ {
		resp, body, err := s.roundTrip(ctx, method, &target, request.Headers, request.MaxBody)
		if resp != nil {
			out.Status, out.URL, out.Header, out.Body = resp.StatusCode, target.String(), resp.Header, body
		}
		if err != nil {
			out.Err = err
			return out
		}
		location := resp.Header.Get("Location")
		if !request.Follow || !isRedirect(resp.StatusCode) || location == "" {
			return out
		}
		if hop == maxRedirects {
			out.Err = errors.New("too many redirects")
			return out
		}
		next, err := target.Parse(location)
		if err != nil {
			out.Err = fmt.Errorf("invalid redirect: %w", err)
			return out
		}
		next.Fragment, next.RawFragment = "", ""
		if err := checkTarget(next); err != nil {
			out.Err = fmt.Errorf("redirect refused: %w", err)
			return out
		}
		target = *next
		if resp.StatusCode == http.StatusSeeOther && method != http.MethodHead {
			method = http.MethodGet
		}
	}
}

func isRedirect(status int) bool {
	switch status {
	case http.StatusMovedPermanently, http.StatusFound, http.StatusSeeOther,
		http.StatusTemporaryRedirect, http.StatusPermanentRedirect:
		return true
	}
	return false
}

// A response with a body that could not be read in full comes back with the error: rules must not
// judge a page by a cut-off copy, but region sources may still read it.
func (s *session) roundTrip(ctx context.Context, method string, target *url.URL, headers map[string]string, maxBody int64) (*http.Response, []byte, error) {
	ctx, cancel := context.WithTimeout(ctx, s.client.hopTimeout())
	defer cancel()
	req, err := http.NewRequestWithContext(ctx, method, target.String(), nil)
	if err != nil {
		return nil, nil, err
	}
	setBrowserHeaders(req.Header)
	for name, value := range headers {
		req.Header.Set(name, value)
	}
	for _, cookie := range s.jar.Cookies(target) {
		req.AddCookie(cookie)
	}
	resp, err := s.send(ctx, req)
	if err != nil {
		return nil, nil, err
	}
	defer resp.Body.Close()
	s.jar.SetCookies(target, resp.Cookies())
	if method == http.MethodHead {
		return resp, nil, nil
	}
	body, err := readBody(resp, maxBody)
	if err != nil {
		return resp, body, fmt.Errorf("reading body: %w", err)
	}
	return resp, body, nil
}

func (s *session) send(ctx context.Context, req *http.Request) (*http.Response, error) {
	host := req.URL.Hostname()
	if cc := s.h2[host]; cc != nil {
		if cc.CanTakeNewRequest() {
			if resp, err := cc.RoundTrip(req); err == nil {
				return resp, nil
			}
		}
		_ = cc.Close()
		delete(s.h2, host)
	}
	conn, err := s.client.Dial(ctx, "tcp", net.JoinHostPort(host, "443"))
	if err != nil {
		return nil, err
	}
	s.conns = append(s.conns, conn)
	tlsContext := s.client.Context
	if tlsContext == nil {
		tlsContext = context.Background()
	}
	config, err := boxtls.NewClientWithOptions(boxtls.ClientOptions{
		Context: tlsContext,
		Logger:  logger.NOP(),
		Options: option.OutboundTLSOptions{
			Enabled:    true,
			ServerName: host,
			ALPN:       []string{"h2", "http/1.1"},
			UTLS:       &option.OutboundUTLSOptions{Enabled: true, Fingerprint: "chrome"},
		},
	})
	if err != nil {
		return nil, err
	}
	tlsConn, err := boxtls.ClientHandshake(ctx, conn, config)
	if err != nil {
		return nil, err
	}
	if tlsConn.ConnectionState().NegotiatedProtocol == "h2" {
		cc, err := s.transport.NewClientConn(tlsConn)
		if err != nil {
			return nil, err
		}
		s.h2[host] = cc
		return cc.RoundTrip(req)
	}
	if deadline, ok := ctx.Deadline(); ok {
		_ = tlsConn.SetDeadline(deadline)
	}
	req.Close = true
	if err := req.Write(tlsConn); err != nil {
		return nil, err
	}
	return http.ReadResponse(bufio.NewReader(tlsConn), req)
}

func setBrowserHeaders(h http.Header) {
	h.Set("User-Agent", chromeUserAgent)
	h.Set("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8")
	h.Set("Accept-Language", "en-US,en;q=0.9")
	h.Set("Accept-Encoding", "gzip, deflate, br, zstd")
	h.Set("Sec-Ch-Ua", `"Google Chrome";v="141", "Not?A_Brand";v="8", "Chromium";v="141"`)
	h.Set("Sec-Ch-Ua-Mobile", "?0")
	h.Set("Sec-Ch-Ua-Platform", `"Windows"`)
	h.Set("Sec-Fetch-Dest", "document")
	h.Set("Sec-Fetch-Mode", "navigate")
	h.Set("Sec-Fetch-Site", "none")
	h.Set("Sec-Fetch-User", "?1")
	h.Set("Upgrade-Insecure-Requests", "1")
}

// The cap applies to decoded bytes, so a compression bomb costs at most maxBody of memory.
func readBody(resp *http.Response, maxBody int64) ([]byte, error) {
	reader, closeReader, err := decoder(resp.Header.Get("Content-Encoding"), resp.Body)
	if err != nil {
		return nil, err
	}
	defer closeReader()
	return io.ReadAll(io.LimitReader(reader, maxBody))
}

func decoder(encoding string, body io.Reader) (io.Reader, func(), error) {
	noop := func() {}
	switch strings.ToLower(strings.TrimSpace(encoding)) {
	case "", "identity":
		return body, noop, nil
	case "gzip", "x-gzip":
		reader, err := gzip.NewReader(body)
		if err != nil {
			return nil, noop, err
		}
		return reader, func() { _ = reader.Close() }, nil
	case "br":
		return brotli.NewReader(body), noop, nil
	case "zstd":
		reader, err := zstd.NewReader(body, zstd.WithDecoderConcurrency(1), zstd.WithDecoderMaxWindow(8<<20))
		if err != nil {
			return nil, noop, err
		}
		return reader, reader.Close, nil
	case "deflate":
		// HTTP's deflate is meant to be zlib-wrapped, but some servers send raw deflate.
		buffered := bufio.NewReader(body)
		header, _ := buffered.Peek(2)
		if len(header) == 2 && header[0]&0x0f == 8 && (uint16(header[0])<<8|uint16(header[1]))%31 == 0 {
			reader, err := zlib.NewReader(buffered)
			if err != nil {
				return nil, noop, err
			}
			return reader, func() { _ = reader.Close() }, nil
		}
		reader := flate.NewReader(buffered)
		return reader, func() { _ = reader.Close() }, nil
	}
	return nil, noop, fmt.Errorf("unsupported content encoding %q", encoding)
}

func shortError(err error) string {
	if errors.Is(err, context.DeadlineExceeded) {
		return "timeout"
	}
	text := strings.TrimSpace(err.Error())
	if len(text) > 160 {
		text = text[:157] + "..."
	}
	return string(bytes.ToValidUTF8([]byte(text), nil))
}
