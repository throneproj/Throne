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
	"syscall"
	"time"

	_ "ThroneCore/internal/distro/all"
	C "github.com/sagernet/sing-box/constant"
)

func RunCore() {
	socketName := os.Getenv("THRONE_CORE_SOCKET")
	if socketName == "" {
		log.Fatal("THRONE_CORE_SOCKET not set")
	}
	debug = os.Getenv("THRONE_CORE_DEBUG") == "1"

	parentcheck.CheckParentProcess()

	// Exit when parent dies
	go func() {
		parent, err := os.FindProcess(os.Getppid())
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
		conn, err = ipc.ConnectIPC(socketName)
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
	runtimeDebug.SetMemoryLimit(2 * 1024 * 1024 * 1024) // 2GB
	go func() {
		var memStats runtime.MemStats
		for {
			time.Sleep(2 * time.Second)
			runtime.ReadMemStats(&memStats)
			if memStats.HeapAlloc > 1.5*1024*1024*1024 {
				panic("Memory has reached 1.5 GB, this is not normal")
			}
		}
	}()

	test_utils.TestCtx, test_utils.CancelTests = context.WithCancel(context.Background())
	RunCore()
	return
}
