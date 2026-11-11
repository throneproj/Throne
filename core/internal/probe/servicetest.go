package probe

import (
	"context"
	"time"

	"github.com/sagernet/sing-box/adapter"

	"ThroneCore/internal/servicecheck"
)

var ServiceReporter resultBuffer[ServiceTestResult]

type ServiceTestResult struct {
	Result servicecheck.Result
	Tag    string
	Error  error
}

const (
	defaultServiceConcurrency = 4
	maxServiceConcurrency     = 8
	// Bounds one exit even when every request runs into its own timeout; the GUI allows 200 s a round.
	serviceTestBudget = 3 * time.Minute
)

// BatchServiceTest runs the checks through each tag. Each check gets its own session and an exit runs
// its checks side by side, about what one browser opening a few tabs would send.
func BatchServiceTest(ctx context.Context, i Box, outboundTags []string, maxConcurrency int, cold bool, defs *servicecheck.Definitions, timeout time.Duration, options servicecheck.Options) []*ServiceTestResult {
	if timeout <= 0 {
		timeout = servicecheck.DefaultRequestTimeout
	}
	if maxConcurrency <= 0 || maxConcurrency > maxServiceConcurrency {
		maxConcurrency = defaultServiceConcurrency
	}
	results := runBatch(ctx, i, outboundTags, maxConcurrency, batchProbe[ServiceTestResult]{
		run: func(ctx context.Context, tag string, outbound adapter.Outbound) *ServiceTestResult {
			if err := awaitTunnels(ctx, i, tag); err != nil {
				return &ServiceTestResult{Tag: tag, Error: err}
			}
			dial, cancelDials := outboundDial(ctx, i, tag, outbound)
			dialer := &probeDialer{dial: dial}
			defer func() {
				cancelDials()
				dialer.Close()
			}()
			runCtx, cancel := context.WithTimeout(ctx, serviceTestBudget)
			defer cancel()
			client := &servicecheck.Client{
				Dial:         dialer.DialContext,
				Timeout:      timeout,
				FirstTimeout: firstRequestTimeout(i, tag, cold, timeout),
				Context:      runCtx,
			}
			result, err := servicecheck.Run(runCtx, defs, client, options)
			if err != nil {
				return &ServiceTestResult{Tag: tag, Error: ErrTestAborted}
			}
			return &ServiceTestResult{Result: result, Tag: tag}
		},
		fail: func(tag string, err error) *ServiceTestResult {
			return &ServiceTestResult{Tag: tag, Error: err}
		},
		publish: ServiceReporter.AddResult,
	})
	ServiceReporter.Reclaim(results)
	return results
}
