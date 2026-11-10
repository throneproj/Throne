package servicecheck

import (
	"context"
	"errors"
	"time"
)

type Options struct {
	// Check ids to run; nil runs them all.
	Only       map[string]bool
	SkipIPInfo bool
	// Pause between consecutive requests; 0 = DefaultGap.
	Gap time.Duration
}

type Result struct {
	Checks []Outcome
	IPInfo []IPInfo
}

const (
	DefaultGap = 250 * time.Millisecond
	// After this many lookups or checks in a row got no answer at all, the exit is taken for dead
	// and the remaining checks fail without sending anything.
	deadAfter = 3
)

// Run checks one exit: the IP lookups, then every check, one request at a time. Cancelling ctx
// aborts it with no result; ctx's deadline instead ends it early, failing the checks not done yet.
func Run(ctx context.Context, defs *Definitions, client *Client, opts Options) (Result, error) {
	var res Result
	stop := func(err error, remaining []*Check) (Result, error) {
		if !errors.Is(err, context.DeadlineExceeded) {
			return Result{}, err
		}
		for _, check := range remaining {
			if opts.Only == nil || opts.Only[check.ID] {
				res.Checks = append(res.Checks, Outcome{ID: check.ID, Name: check.Name, Category: check.Category, Status: StatusFailed, Note: "timeout"})
			}
		}
		return res, nil
	}
	gap := opts.Gap
	if gap <= 0 {
		gap = DefaultGap
	}
	sent := false
	pause := func() error {
		if !sent {
			sent = true
			return nil
		}
		timer := time.NewTimer(gap)
		defer timer.Stop()
		select {
		case <-ctx.Done():
			return ctx.Err()
		case <-timer.C:
			return nil
		}
	}

	silent := 0
	var lastError error
	if !opts.SkipIPInfo {
		for _, provider := range defs.IPInfo {
			if err := pause(); err != nil {
				return stop(err, defs.Checks)
			}
			info, answered := provider.lookup(ctx, client)
			res.IPInfo = append(res.IPInfo, info)
			if err := ctx.Err(); err != nil {
				return stop(err, defs.Checks)
			}
			if answered {
				silent = 0
			} else {
				silent++
				lastError = errorText(info.Error)
			}
		}
	}

	for checkIndex, check := range defs.Checks {
		if opts.Only != nil && !opts.Only[check.ID] {
			continue
		}
		if silent >= deadAfter {
			out := Outcome{ID: check.ID, Name: check.Name, Category: check.Category, Status: StatusFailed, Note: "skipped: the exit did not answer"}
			if lastError != nil {
				out.Note = "skipped: " + shortError(lastError)
			}
			res.Checks = append(res.Checks, out)
			continue
		}
		responses := make([]*Response, len(check.Requests))
		s := client.newSession()
		answered := false
		for idx, request := range check.Requests {
			if err := pause(); err != nil {
				s.close()
				return stop(err, defs.Checks[checkIndex:])
			}
			responses[idx] = s.fetch(ctx, request)
			if err := ctx.Err(); err != nil {
				s.close()
				return stop(err, defs.Checks[checkIndex:])
			}
			if responses[idx].answered() {
				answered = true
			} else if responses[idx].Err != nil {
				lastError = responses[idx].Err
			}
		}
		s.close()
		if answered {
			silent = 0
		} else {
			silent++
		}
		res.Checks = append(res.Checks, check.Evaluate(responses))
	}
	return res, nil
}

type errorText string

func (e errorText) Error() string { return string(e) }
