package probe

import (
	"context"
	"time"

	"github.com/sagernet/sing-box/adapter"

	"ThroneCore/internal/customtest"
)

var CustomReporter resultBuffer[CustomTestResult]

type CustomTestResult struct {
	Result customtest.Outcome
	Tag    string
	Error  error
}

func BatchCustomTest(ctx context.Context, i Box, outboundTags []string, maxConcurrency int, cold bool, script string, timeout time.Duration) ([]*CustomTestResult, error) {
	prog, err := customtest.Compile(script)
	if err != nil {
		return nil, err
	}
	if timeout <= 0 {
		timeout = customtest.DefaultTimeout
	}

	results := runBatch(ctx, i, outboundTags, maxConcurrency, batchProbe[CustomTestResult]{
		run: func(ctx context.Context, tag string, outbound adapter.Outbound) *CustomTestResult {
			if err := awaitTunnels(ctx, i, tag); err != nil {
				return &CustomTestResult{Tag: tag, Error: err}
			}
			dial, cancelDials := outboundDial(ctx, i, tag, outbound)
			dialer := &probeDialer{dial: dial}
			defer func() {
				cancelDials()
				dialer.Close()
			}()
			firstTimeout := firstRequestTimeout(i, tag, cold, timeout)
			outcome, err := customtest.Run(ctx, prog, dialer.DialContext, firstTimeout, timeout)
			return &CustomTestResult{Result: outcome, Tag: tag, Error: err}
		},
		fail: func(tag string, err error) *CustomTestResult {
			return &CustomTestResult{Tag: tag, Error: err}
		},
		publish: CustomReporter.AddResult,
	})
	CustomReporter.Reclaim(results)
	return results, nil
}
