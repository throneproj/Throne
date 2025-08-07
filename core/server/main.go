package main

import (
	"Core/gen"
	"Core/internal/boxmain"
	"context"
	"flag"
	"fmt"
	"log"
	"net"
	"os"
	"runtime"
	runtimeDebug "runtime/debug"
	"strconv"
	"syscall"
	"time"

	_ "Core/internal/distro/all"
	C "github.com/sagernet/sing-box/constant"
)

func RunCore() {
	_port := flag.Int("port", 19810, "")
	_debug := flag.Bool("debug", false, "")
	flag.CommandLine.Parse(os.Args[1:])
	debug = *_debug

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

	// RPC
	go func() {
		for {
			time.Sleep(100 * time.Millisecond)
			conn, err := net.Dial("tcp", "127.0.0.1:"+strconv.Itoa(*_port))
			if err == nil {
				conn.Close()
				fmt.Printf("Core listening at %v\n", "127.0.0.1:"+strconv.Itoa(*_port))
				return
			}
		}
	}()
	err := gen.ListenAndServeLibcoreService("tcp", "127.0.0.1:"+strconv.Itoa(*_port), new(server))
	if err != nil {
		log.Fatalf("failed to listen: %v", err)
	}
}

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
	RunCore()
	return
}
