package probe

import (
	"context"
	"errors"
	"io"
	"net/http"
	"time"

	"github.com/sagernet/sing-box/adapter"
	E "github.com/sagernet/sing/common/exceptions"
)

var URLReporter resultBuffer[URLTestResult]

const (
	URLTestTimeout    = 3 * time.Second
	urlTestDrainLimit = 64 * 1024
)

type URLTestResult struct {
	Duration time.Duration
	Tag      string
	Error    error
}

func BatchURLTest(ctx context.Context, i Box, outboundTags []string, url string, maxConcurrency int, twice bool, timeout time.Duration) []*URLTestResult {
	results := BatchURLTestTo(ctx, i, outboundTags, url, maxConcurrency, twice, timeout, URLReporter.AddResult)
	URLReporter.Reclaim(results)
	return results
}

// BatchURLTestTo hands every finished result to publish; aborted tags are returned but never published.
func BatchURLTestTo(ctx context.Context, i Box, outboundTags []string, url string, maxConcurrency int, twice bool, timeout time.Duration, publish func(*URLTestResult)) []*URLTestResult {
	if timeout <= 0 {
		timeout = URLTestTimeout
	}

	return runBatch(ctx, i, outboundTags, maxConcurrency, batchProbe[URLTestResult]{
		run: func(ctx context.Context, tag string, outbound adapter.Outbound) *URLTestResult {
			if err := awaitTunnels(ctx, i, tag); err != nil {
				return &URLTestResult{Tag: tag, Error: err}
			}
			client, closeClient := outboundHTTPClient(ctx, i, tag, outbound)
			defer closeClient()
			duration, err := urlTest(ctx, client, url, firstRequestTimeout(i, tag, twice, timeout))
			if err == nil && twice {
				duration, err = urlTest(ctx, client, url, timeout)
			}
			return &URLTestResult{Duration: duration, Tag: tag, Error: err}
		},
		fail: func(tag string, err error) *URLTestResult {
			return &URLTestResult{Tag: tag, Error: err}
		},
		publish: publish,
	})
}

// BatchDownloadTestTo is one cold GET per tag that must deliver bodyBytes: a tunnel MTU too large for the path
// passes a small request and stalls bulk data.
func BatchDownloadTestTo(ctx context.Context, i Box, outboundTags []string, url string, bodyBytes int64, maxConcurrency int, timeout time.Duration, publish func(*URLTestResult)) []*URLTestResult {
	if timeout <= 0 {
		timeout = URLTestTimeout
	}

	return runBatch(ctx, i, outboundTags, maxConcurrency, batchProbe[URLTestResult]{
		run: func(ctx context.Context, tag string, outbound adapter.Outbound) *URLTestResult {
			if err := awaitTunnels(ctx, i, tag); err != nil {
				return &URLTestResult{Tag: tag, Error: err}
			}
			client, closeClient := outboundHTTPClient(ctx, i, tag, outbound)
			defer closeClient()
			duration, err := downloadTest(ctx, client, url, bodyBytes, firstRequestTimeout(i, tag, true, timeout))
			return &URLTestResult{Duration: duration, Tag: tag, Error: err}
		},
		fail: func(tag string, err error) *URLTestResult {
			return &URLTestResult{Tag: tag, Error: err}
		},
		publish: publish,
	})
}

func downloadTest(ctx context.Context, client *http.Client, url string, bodyBytes int64, timeout time.Duration) (time.Duration, error) {
	ctx, cancel := context.WithTimeout(ctx, timeout)
	defer cancel()
	begin := time.Now()
	req, err := http.NewRequestWithContext(ctx, "GET", url, nil)
	if err != nil {
		return 0, err
	}
	resp, err := client.Do(req)
	if err != nil {
		return 0, err
	}
	defer resp.Body.Close()
	if resp.StatusCode < 200 || resp.StatusCode > 299 {
		return 0, E.New("download: HTTP ", resp.StatusCode)
	}
	read, err := io.CopyN(io.Discard, resp.Body, bodyBytes)
	if read < bodyBytes {
		if err == nil || errors.Is(err, io.EOF) {
			return 0, E.New("download: the body ended after ", read, " of ", bodyBytes, " bytes")
		}
		return 0, E.Cause(err, "download stalled after ", read, " of ", bodyBytes, " bytes")
	}
	return time.Since(begin), nil
}

func urlTest(ctx context.Context, client *http.Client, url string, timeout time.Duration) (time.Duration, error) {
	ctx, cancel := context.WithTimeout(ctx, timeout)
	defer cancel()
	begin := time.Now()
	req, err := http.NewRequestWithContext(ctx, "GET", url, nil)
	if err != nil {
		return 0, err
	}
	resp, err := client.Do(req)
	if err != nil {
		return 0, err
	}
	duration := time.Since(begin)
	// A body closed before EOF costs the connection, and the timed request would dial cold again.
	_, _ = io.CopyN(io.Discard, resp.Body, urlTestDrainLimit)
	_ = resp.Body.Close()
	return duration, nil
}
