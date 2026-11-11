package servicecheck

import (
	"context"
	"errors"
	"sync"
	"time"
)

type Options struct {
	// Check ids to run; nil runs them all.
	Only       map[string]bool
	SkipIPInfo bool
	// Pause between a check's consecutive requests; 0 = DefaultGap.
	Gap time.Duration
	// Lookups and checks in flight at once; 0 = DefaultConcurrency.
	Concurrency int
}

type Result struct {
	Checks []Outcome
	IPInfo []IPInfo
}

const (
	DefaultGap         = 250 * time.Millisecond
	DefaultConcurrency = 8
	// Once this many lookups or checks got no answer at all and none has had one yet, the exit is
	// taken for dead and the checks not started yet fail without sending anything.
	deadAfter = 3
)

// Run checks one exit: the IP lookups and the checks run side by side, each check's own requests one
// at a time. Cancelling ctx aborts it with no result; ctx's deadline instead ends it early, failing
// the checks not done yet.
func Run(ctx context.Context, defs *Definitions, client *Client, opts Options) (Result, error) {
	gap := opts.Gap
	if gap <= 0 {
		gap = DefaultGap
	}
	concurrency := opts.Concurrency
	if concurrency <= 0 {
		concurrency = DefaultConcurrency
	}
	var providers []*IPInfoProvider
	if !opts.SkipIPInfo {
		providers = defs.IPInfo
	}
	var checks []*Check
	for _, check := range defs.Checks {
		if opts.Only == nil || opts.Only[check.ID] {
			checks = append(checks, check)
		}
	}
	infos := make([]*IPInfo, len(providers))
	outcomes := make([]*Outcome, len(checks))

	var (
		access    sync.Mutex
		alive     bool
		silent    int
		lastError error
		aborted   error
	)
	settle := func(answered bool, err error) {
		access.Lock()
		defer access.Unlock()
		if answered {
			alive = true
			return
		}
		silent++
		if err != nil {
			lastError = err
		}
	}
	interrupted := func(err error) {
		access.Lock()
		defer access.Unlock()
		if aborted == nil {
			aborted = err
		}
	}
	deadNote := func() (string, bool) {
		access.Lock()
		defer access.Unlock()
		if alive || silent < deadAfter {
			return "", false
		}
		if lastError != nil {
			return "skipped: " + shortError(lastError), true
		}
		return "skipped: the exit did not answer", true
	}

	lookup := func(idx int) {
		info, answered := providers[idx].lookup(ctx, client)
		infos[idx] = &info
		if err := ctx.Err(); err != nil {
			interrupted(err)
			return
		}
		settle(answered, errorText(info.Error))
	}
	runCheck := func(idx int) {
		check := checks[idx]
		if note, dead := deadNote(); dead {
			outcomes[idx] = &Outcome{ID: check.ID, Name: check.Name, Category: check.Category, Status: StatusFailed, Note: note}
			return
		}
		responses := make([]*Response, len(check.Requests))
		s := client.newSession()
		defer s.close()
		answered := false
		var checkError error
		for i, request := range check.Requests {
			if i > 0 {
				if err := sleep(ctx, gap); err != nil {
					interrupted(err)
					return
				}
			}
			responses[i] = s.fetch(ctx, request)
			if err := ctx.Err(); err != nil {
				interrupted(err)
				return
			}
			if responses[i].answered() {
				answered = true
			} else if responses[i].Err != nil {
				checkError = responses[i].Err
			}
		}
		settle(answered, checkError)
		outcome := check.Evaluate(responses)
		outcomes[idx] = &outcome
	}

	limiter := make(chan struct{}, concurrency)
	var wg sync.WaitGroup
	for job := 0; job < len(providers)+len(checks); job++ {
		select {
		case limiter <- struct{}{}:
		case <-ctx.Done():
		}
		if err := ctx.Err(); err != nil {
			interrupted(err)
			break
		}
		wg.Add(1)
		go func(job int) {
			defer wg.Done()
			defer func() { <-limiter }()
			if job < len(providers) {
				lookup(job)
			} else {
				runCheck(job - len(providers))
			}
		}(job)
	}
	wg.Wait()

	if aborted != nil && !errors.Is(aborted, context.DeadlineExceeded) {
		return Result{}, aborted
	}
	var res Result
	for _, info := range infos {
		if info != nil {
			res.IPInfo = append(res.IPInfo, *info)
		}
	}
	for idx, outcome := range outcomes {
		if outcome == nil {
			check := checks[idx]
			outcome = &Outcome{ID: check.ID, Name: check.Name, Category: check.Category, Status: StatusFailed, Note: "timeout"}
		}
		res.Checks = append(res.Checks, *outcome)
	}
	return res, nil
}

func sleep(ctx context.Context, d time.Duration) error {
	timer := time.NewTimer(d)
	defer timer.Stop()
	select {
	case <-ctx.Done():
		return ctx.Err()
	case <-timer.C:
		return nil
	}
}

type errorText string

func (e errorText) Error() string { return string(e) }
