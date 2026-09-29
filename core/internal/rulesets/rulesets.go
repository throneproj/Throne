package rulesets

import (
	"context"
	"slices"
	"strings"
	"sync"
	"time"

	"github.com/sagernet/sing-box/adapter"
)

const (
	updateTimeout     = 60 * time.Second
	updateConcurrency = 5
)

func UpdateAll(ctx context.Context, router adapter.Router) (updated int32, errorText string) {
	ctx, cancel := context.WithTimeout(ctx, updateTimeout)
	defer cancel()

	var resultMu sync.Mutex
	var failures []string
	slots := make(chan struct{}, updateConcurrency)
	var wg sync.WaitGroup
	for _, ruleSet := range router.RuleSets() {
		updatable, ok := ruleSet.(adapter.UpdatableRuleSet)
		if !ok {
			continue
		}
		wg.Add(1)
		go func() {
			defer wg.Done()
			slots <- struct{}{}
			defer func() { <-slots }()
			err := updatable.Update(ctx)
			resultMu.Lock()
			defer resultMu.Unlock()
			if err != nil {
				failures = append(failures, updatable.Name()+": "+err.Error())
				return
			}
			updated++
		}()
	}
	wg.Wait()
	slices.Sort(failures)
	return updated, strings.Join(failures, "\n")
}
