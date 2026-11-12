// Package invoker runs file work at client-named locations with the client's own identity when the core holds rights
// its client lacks, so the operating system applies the client's permissions.
package invoker

import (
	"errors"
	"fmt"
	"log"
	"runtime"
)

// Boundary reports whether the core holds privileges its client lacks.
func Boundary() bool { return boundary() }

// Do runs fn with the invoking user's filesystem identity; fn must do its file work synchronously on the calling goroutine.
func Do(fn func() error) error {
	id, ok := invokingUser()
	if !ok {
		return fn()
	}
	done := make(chan outcome, 1)
	go run(id, fn, done)
	out := <-done
	if out.panicked {
		panic(out.value)
	}
	return out.err
}

// Call is Do for functions that return a value.
func Call[T any](fn func() (T, error)) (T, error) {
	var result T
	err := Do(func() (err error) {
		result, err = fn()
		return err
	})
	return result, err
}

type outcome struct {
	err      error
	panicked bool
	value    any
}

func run(id identity, fn func() error, done chan<- outcome) {
	runtime.LockOSThread()
	out := outcome{err: errors.New("invoker: the function exited without returning")}
	defer func() { done <- out }()
	defer func() {
		if p := recover(); p != nil {
			out = outcome{panicked: true, value: p}
		}
		if err := id.revert(); err != nil {
			// Exiting while still locked makes the runtime retire the thread instead of reusing it.
			log.Printf("invoker: cannot restore the thread's identity, retiring the thread: %v", err)
			return
		}
		runtime.UnlockOSThread()
	}()
	if err := id.assume(); err != nil {
		out.err = fmt.Errorf("assume the invoking user's identity: %w", err)
		return
	}
	out.err = fn()
}
