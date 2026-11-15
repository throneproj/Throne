package boxmain

import (
	"context"
	"fmt"
	"io"
	"time"

	box "github.com/sagernet/sing-box"
)

// ctx is the one handed to box.New, which registers every service into its registry instead of deriving its own.
type Box struct {
	*box.Box
	ctx context.Context
}

func (s *Box) Context() context.Context {
	return s.ctx
}

func (s *Box) CloseWithTimeout(cancel context.CancelFunc, d time.Duration, logFunc func(v ...any), block bool) {
	start := time.Now()
	t := time.NewTimer(d)
	done := make(chan struct{})

	printCloseTime := func() {
		logFunc("[Info] sing-box closed in", fmt.Sprintf("%d ms", time.Since(start).Milliseconds()))
	}

	go func(cancel context.CancelFunc, closer io.Closer) {
		cancel()
		closer.Close()
		close(done)
		if !t.Stop() {
			printCloseTime()
		}
	}(cancel, s)

	select {
	case <-t.C:
		logFunc("[Warning] sing-box close takes longer than expected")
		if block {
			select {
			case <-done:
				printCloseTime()
			}
		}
	case <-done:
		printCloseTime()
	}
}
