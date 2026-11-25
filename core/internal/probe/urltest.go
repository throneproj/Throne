package probe

import (
	"context"
	"fmt"
	"io"
	"net/http"
	"time"

	"github.com/sagernet/sing-box/adapter"
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
	if resp.StatusCode >= http.StatusBadRequest {
		return 0, fmt.Errorf("unexpected status %s", resp.Status)
	}
	return duration, nil
}
