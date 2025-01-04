package main

import (
	"context"
	"fmt"
	"runtime"
	runtimeDebug "runtime/debug"
	"time"
	_ "unsafe"

	"nekobox_core/server"

	C "github.com/sagernet/sing-box/constant"
	_ "nekobox_core/internal/distro/all"
)

func main() {
	fmt.Println("sing-box:", C.Version)
	fmt.Println()
	runtimeDebug.SetMemoryLimit(2 * 1024 * 1024 * 1024) // 2GB
	go func() {
		var memStats runtime.MemStats
		for {
			time.Sleep(2 * time.Second)
			runtime.ReadMemStats(&memStats)
			if memStats.HeapAlloc > 1.5*1024*1024*1024 {
				// too much memory for sing-box, crash
				panic("Memory has reached 1.5 GB, this is not normal")
			}
		}
	}()

	testCtx, cancelTests = context.WithCancel(context.Background())
	grpc_server.RunCore(setupCore, &server{})
	return
}
