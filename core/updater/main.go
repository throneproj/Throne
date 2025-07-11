package main

import (
	"io"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"time"
)

func main() {
	// update & launcher
	exe, err := os.Executable()
	if err != nil {
		panic(err.Error())
	}

	wd := filepath.Dir(exe)
	os.Chdir(wd)
	exe = filepath.Base(os.Args[0])
	log.Println("exe:", exe, "exe dir:", wd)

	if strings.HasPrefix(strings.ToLower(exe), "updater") {
		if runtime.GOOS == "windows" {
			if strings.HasPrefix(strings.ToLower(exe), "updater.old") {
				// 2. "updater.old" update files
				time.Sleep(time.Second)
				Updater()
				// 3. start
				exec.Command("./Throne.exe").Start()
			} else {
				// 1. Throne stop itself and run "updater.exe"
				Copy("./updater.exe", "./updater.old")
				exec.Command("./updater.old", os.Args[1:]...).Start()
			}
		} else {
			// 1. update files
			Updater()
			// 2. start
			exec.Command("./Throne").Start()
		}
		return
	}
	log.Fatalf("wrong name")
}

func Copy(src string, dst string) {
	srcFile, err := os.Open(src)
	if err != nil {
		log.Println(err)
		return
	}
	defer srcFile.Close()
	dstFile, err := os.OpenFile(dst, os.O_CREATE|os.O_TRUNC|os.O_RDWR, 0644)
	if err != nil {
		log.Println(err)
		return
	}
	defer dstFile.Close()
	_, err = io.Copy(dstFile, srcFile)
	if err != nil {
		log.Println(err)
	}
}
