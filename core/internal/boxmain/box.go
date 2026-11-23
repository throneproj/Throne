package boxmain

import (
	"context"
	"fmt"
	"io"
	"io/fs"
	"os"
	"time"

	box "github.com/sagernet/sing-box"
)

// ctx is the one handed to box.New, which registers every service into its registry instead of deriving its own.
type Box struct {
	*box.Box
	ctx     context.Context
	psiphon bool
}

func (s *Box) Context() context.Context {
	return s.ctx
}

func (s *Box) Close() error {
	err := s.Box.Close()
	if s.psiphon {
		chownPsiphonDirectory()
	}
	return err
}

// A root core writes Psiphon's datastore as root; Lchown within an os.Root so planted symlinks cannot redirect it.
func chownPsiphonDirectory() {
	uid, gid, known := fileOwner()
	if !known || os.Geteuid() != 0 {
		return
	}
	config, err := os.OpenRoot(".")
	if err != nil {
		return
	}
	defer config.Close()
	if info, err := config.Lstat(psiphonFolder); err != nil || !info.IsDir() {
		return
	}
	_ = fs.WalkDir(config.FS(), psiphonFolder, func(name string, _ fs.DirEntry, err error) error {
		if err == nil {
			_ = config.Lchown(name, uid, gid)
		}
		return nil
	})
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
