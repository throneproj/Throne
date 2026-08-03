package main

import (
	"ThroneCore/internal/boxmain"
	"ThroneCore/ipc"
	"ThroneCore/parentcheck"
	"ThroneCore/test_utils"
	"context"
	"fmt"
	"github.com/xtls/xray-core/core"
	"log"
	"net"
	"os"
	"runtime"
	runtimeDebug "runtime/debug"
	"runtime/pprof"
	"syscall"
	"time"

	_ "ThroneCore/internal/distro/all"
	C "github.com/sagernet/sing-box/constant"
)

const (
	// Threshold is live heap measured after a forced GC: hitting it below the
	// soft limit means the GC is thrashing and cannot win
	memoryLimit           = 2 * 1024 * 1024 * 1024 // 2GB
	memoryPanicThreshold  = 1536 * 1024 * 1024     // 1.5GB
	memoryCheckInterval   = 2 * time.Second
	memoryForcedGCBackoff = 30 * time.Second
)

// watchMemory takes the core down when the live heap runs away
//
// HeapAlloc counts unswept garbage and sawtooths up to the GC target (live
// set x GOGC, capped by memoryLimit), so a bare threshold on it fires on
// a healthy heap - only a heap still oversized after a forced GC means a leak
func watchMemory() {
	var memStats runtime.MemStats
	for {
		time.Sleep(memoryCheckInterval)

		runtime.ReadMemStats(&memStats)
		if memStats.HeapAlloc < memoryPanicThreshold {
			continue
		}
		allocated := memStats.HeapAlloc

		runtimeDebug.FreeOSMemory()
		runtime.ReadMemStats(&memStats)
		if memStats.HeapAlloc < memoryPanicThreshold {
			// FreeOSMemory is stop-the-world, do not repeat it every tick
			// while a busy core legitimately sits near the threshold
			time.Sleep(memoryForcedGCBackoff)
			continue
		}

		log.Printf("memory watchdog: %d MiB live after a forced GC (%d MiB before), %d goroutines",
			memStats.HeapAlloc>>20, allocated>>20, runtime.NumGoroutine())
		if path, err := writeHeapProfile(); err != nil {
			log.Printf("memory watchdog: could not write heap profile: %v", err)
		} else {
			log.Printf("memory watchdog: heap profile written to %s", path)
		}
		panic(fmt.Sprintf("Live heap reached %d MiB after a forced GC, this is not normal",
			memStats.HeapAlloc>>20))
	}
}

func writeHeapProfile() (string, error) {
	// Core runs privileged: a clock-derived name is guessable, and a symlink
	// planted at that path turns this into a root-owned write anywhere
	f, err := os.CreateTemp("", "throne-core-heap-*.pprof")
	if err != nil {
		return "", err
	}
	defer f.Close()
	if err = pprof.WriteHeapProfile(f); err != nil {
		return "", err
	}
	return f.Name(), f.Sync()
}

func RunCore() {
	socketName := os.Getenv("THRONE_CORE_SOCKET")
	if socketName == "" {
		log.Fatal("THRONE_CORE_SOCKET not set")
	}
	debug = os.Getenv("THRONE_CORE_DEBUG") == "1"

	parentcheck.CheckParentProcess()

	// Exit when parent dies
	go func() {
		parent, err := os.FindProcess(parentcheck.ParentPID)
		if err != nil {
			log.Fatalln("find parent:", err)
		}
		if runtime.GOOS == "windows" {
			state, err := parent.Wait()
			log.Fatalln("parent exited:", state, err)
		} else {
			for {
				time.Sleep(time.Second * 10)
				err = parent.Signal(syscall.Signal(0))
				if err != nil {
					log.Fatalln("parent exited:", err)
				}
			}
		}
	}()

	boxmain.DisableColor()

	// Connect to GUI IPC socket, retry up to 10 times
	var conn net.Conn
	var err error
	for i := 0; i < 10; i++ {
		conn, err = ipc.ConnectIPC(socketName, parentcheck.ParentPID)
		if err == nil {
			break
		}
		log.Printf("IPC connect attempt %d/10 failed: %v", i+1, err)
		time.Sleep(500 * time.Millisecond)
	}
	if err != nil {
		log.Fatalf("failed to connect to GUI socket after 10 attempts: %v", err)
	}

	fmt.Println("Core Has Successfully Connected to Throne!")
	runDispatch(conn)
}

func main() {
	defer func() {
		if err := recover(); err != nil {
			fmt.Println("Core panicked:")
			fmt.Println(err)
			os.Exit(0)
		}
	}()
	fmt.Println("sing-box:", C.Version)
	fmt.Println("Xray-core:", core.Version())
	fmt.Println()
	runtimeDebug.SetMemoryLimit(memoryLimit)
	go watchMemory()

	test_utils.TestCtx, test_utils.CancelTests = context.WithCancel(context.Background())
	RunCore()
	return
}
