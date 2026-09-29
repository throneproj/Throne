package warp

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"io"
	"net"
	"net/http"
	"net/http/httptrace"
	"net/url"
	"slices"
	"strings"
	"sync/atomic"
	"time"

	E "github.com/sagernet/sing/common/exceptions"
)

const (
	defaultAPIHost = "api.cloudflareclient.com"
	apiVersion     = "v0a4471"
	clientVersion  = "a-6.35-4471"
	userAgent      = "WARP for Android"
	maxBodySize    = 1 << 20
	dialTimeout    = 10 * time.Second
	// Some ISPs stall the handshake for 5-10 s before letting it complete.
	tlsHandshakeTimeout = 10 * time.Second
)

type registerRequest struct {
	Key          string `json:"key"`
	InstallID    string `json:"install_id"`
	FCMToken     string `json:"fcm_token"`
	TOS          string `json:"tos"`
	Model        string `json:"model"`
	SerialNumber string `json:"serial_number"`
	KeyType      string `json:"key_type"`
	TunnelType   string `json:"tunnel_type"`
	Locale       string `json:"locale"`
}

type updateKeyRequest struct {
	Key        string `json:"key"`
	KeyType    string `json:"key_type"`
	TunnelType string `json:"tunnel_type"`
}

type device struct {
	ID      string `json:"id"`
	Token   string `json:"token"`
	Account struct {
		License string `json:"license"`
	} `json:"account"`
	Config struct {
		ClientID  string `json:"client_id"`
		Interface struct {
			Addresses struct {
				V4 string `json:"v4"`
				V6 string `json:"v6"`
			} `json:"addresses"`
		} `json:"interface"`
		Peers []struct {
			PublicKey string `json:"public_key"`
			Endpoint  struct {
				Host string `json:"host"`
				V4   string `json:"v4"`
				V6   string `json:"v6"`
			} `json:"endpoint"`
		} `json:"peers"`
	} `json:"config"`
}

type apiError struct {
	Errors []struct {
		Code    int    `json:"code"`
		Message string `json:"message"`
	} `json:"errors"`
}

type statusError struct {
	statusCode int
	message    string
}

func (e *statusError) Error() string {
	return e.message
}

type client struct {
	httpClient *http.Client
}

func newClient(proxy string) (*client, error) {
	transport := http.DefaultTransport.(*http.Transport).Clone()
	transport.Proxy = nil
	transport.DialContext = (&net.Dialer{Timeout: dialTimeout, KeepAlive: 30 * time.Second}).DialContext
	transport.TLSHandshakeTimeout = tlsHandshakeTimeout
	if proxy != "" {
		proxyURL, err := url.Parse(proxy)
		if err != nil {
			return nil, E.Cause(err, "parse proxy")
		}
		transport.Proxy = http.ProxyURL(proxyURL)
	}
	return &client{httpClient: &http.Client{Transport: transport}}, nil
}

func (c *client) Close() {
	c.httpClient.CloseIdleConnections()
}

func (c *client) register(ctx context.Context, hosts []string, body registerRequest) (string, *device, error) {
	var failures []string
	for _, host := range hosts {
		var connected atomic.Bool
		traceCtx := httptrace.WithClientTrace(ctx, &httptrace.ClientTrace{
			GotConn: func(httptrace.GotConnInfo) { connected.Store(true) },
		})
		registered, err := c.call(traceCtx, host, http.MethodPost, "/reg", "", body)
		if err == nil {
			return host, registered, nil
		}
		var urlErr *url.Error
		if errors.As(err, &urlErr) {
			err = urlErr.Err
		}
		failures = append(failures, host+": "+err.Error())
		// POST /reg is not idempotent: once a server may have seen it, another host could register a second device.
		if (connected.Load() && !isRejected(err)) || ctx.Err() != nil {
			break
		}
	}
	return "", nil, E.New(strings.Join(failures, "; "))
}

func (c *client) call(ctx context.Context, host string, method string, path string, token string, body any) (*device, error) {
	content, err := json.Marshal(body)
	if err != nil {
		return nil, err
	}
	request, err := http.NewRequestWithContext(ctx, method, "https://"+host+"/"+apiVersion+path, bytes.NewReader(content))
	if err != nil {
		return nil, err
	}
	request.Header.Set("User-Agent", userAgent)
	request.Header.Set("CF-Client-Version", clientVersion)
	request.Header.Set("Content-Type", "application/json; charset=UTF-8")
	if token != "" {
		request.Header.Set("Authorization", "Bearer "+token)
	}
	response, err := c.httpClient.Do(request)
	if err != nil {
		return nil, err
	}
	defer response.Body.Close()
	content, err = io.ReadAll(io.LimitReader(response.Body, maxBodySize))
	if err != nil {
		return nil, E.Cause(err, "read response")
	}
	if response.StatusCode < 200 || response.StatusCode >= 300 {
		return nil, responseError(response, content)
	}
	var result device
	err = json.Unmarshal(content, &result)
	if err != nil {
		return nil, E.Cause(err, "decode response")
	}
	return &result, nil
}

func responseError(response *http.Response, content []byte) error {
	err := &statusError{statusCode: response.StatusCode, message: response.Status}
	if response.StatusCode == http.StatusTooManyRequests {
		err.message = "rate limited by Cloudflare, try again later"
		return err
	}
	var apiErr apiError
	if json.Unmarshal(content, &apiErr) == nil {
		var messages []string
		for _, item := range apiErr.Errors {
			if item.Message != "" {
				messages = append(messages, item.Message)
			}
		}
		if len(messages) > 0 {
			err.message = response.Status + ": " + strings.Join(messages, "; ")
		}
	}
	return err
}

func isRejected(err error) bool {
	var statusErr *statusError
	return errors.As(err, &statusErr) && statusErr.statusCode >= 400 && statusErr.statusCode < 500 &&
		statusErr.statusCode != http.StatusTooManyRequests
}

func normalizeHosts(hosts []string) []string {
	var result []string
	for _, host := range hosts {
		host = strings.ToLower(strings.TrimSpace(host))
		host = strings.TrimPrefix(host, "https://")
		host = strings.TrimPrefix(host, "http://")
		if index := strings.IndexAny(host, "/?#"); index >= 0 {
			host = host[:index]
		}
		if host != "" && !slices.Contains(result, host) {
			result = append(result, host)
		}
	}
	if len(result) == 0 {
		return []string{defaultAPIHost}
	}
	return result
}
