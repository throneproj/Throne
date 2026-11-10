package elevation

import (
	"bufio"
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"os"
	"strconv"
	"strings"
	"time"
	"unsafe"

	"ThroneCore/internal/ipc"
	"ThroneCore/internal/parentcheck"

	"golang.org/x/sys/windows"
)

type stub struct {
	mode    Mode
	gui     windows.Handle
	job     windows.Handle
	child   windows.Handle
	request *os.File
	reply   *os.File
	replies <-chan string
}

// RunStub returns only when the elevated child could not be started, before it touched stdout; the caller then carries on
// unelevated. Otherwise it exits with the child's exit code.
func RunStub(mode Mode) {
	taskPath := os.Getenv(EnvTask)
	// An unelevated fallback must not hand the variable to anything it starts.
	_ = os.Unsetenv(EnvTask)
	s, err := startStub(mode, taskPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "elevated core unavailable: %s; running without administrator rights\n", oneLine(err.Error()))
		return
	}
	if mode == ModeCore {
		s.handOverPipe()
	}
	s.request.Close()
	s.reply.Close()
	os.Exit(s.wait())
}

func startStub(mode Mode, taskPath string) (_ *stub, err error) {
	s := &stub{mode: mode}
	var requestRead, replyWrite windows.Handle
	defer func() {
		if err == nil {
			return
		}
		// The child joins the job before it answers, so closing the job also kills one that answered too late.
		for _, h := range []windows.Handle{s.job, s.gui, requestRead, replyWrite} {
			if h != 0 {
				windows.CloseHandle(h)
			}
		}
		if s.request != nil {
			s.request.Close()
		}
		if s.reply != nil {
			s.reply.Close()
		}
	}()

	if s.gui, err = windows.OpenProcess(windows.SYNCHRONIZE, false, uint32(parentcheck.ParentPID)); err != nil {
		return nil, fmt.Errorf("open Throne's process: %w", err)
	}
	if s.job, err = newKillOnCloseJob(); err != nil {
		return nil, fmt.Errorf("create a job object: %w", err)
	}
	var requestWrite, replyRead windows.Handle
	if err = windows.CreatePipe(&requestRead, &requestWrite, nil, 0); err != nil {
		return nil, fmt.Errorf("create a pipe: %w", err)
	}
	s.request = os.NewFile(uintptr(requestWrite), "elevation-request")
	if err = windows.CreatePipe(&replyRead, &replyWrite, nil, 0); err != nil {
		return nil, fmt.Errorf("create a pipe: %w", err)
	}
	s.reply = os.NewFile(uintptr(replyRead), "elevation-reply")

	line, err := newRequest(mode, s.job)
	if err != nil {
		return nil, err
	}
	var session uint32
	if err = windows.ProcessIdToSessionId(windows.GetCurrentProcessId(), &session); err != nil {
		return nil, fmt.Errorf("read the session id: %w", err)
	}
	arg0 := fmt.Sprintf("%d:%d:%d", os.Getpid(), requestRead, replyWrite)
	if err = runTask(taskPath, arg0, session); err != nil {
		return nil, err
	}

	s.replies = readLines(s.reply)
	// Written off this goroutine: a child that never starts never drains the pipe, and the reply timeout must still fire.
	go func() { _, _ = s.request.Write(line) }()

	answer, err := s.next(replyTimeout)
	if err != nil {
		return nil, err
	}
	if s.child, err = parseOK(answer); err != nil {
		return nil, err
	}
	// The child holds its own duplicates now; ours would keep its pipe ends from ever reporting a broken pipe.
	windows.CloseHandle(requestRead)
	windows.CloseHandle(replyWrite)
	return s, nil
}

func newKillOnCloseJob() (windows.Handle, error) {
	job, err := windows.CreateJobObject(nil, nil)
	if err != nil {
		return 0, err
	}
	info := windows.JOBOBJECT_EXTENDED_LIMIT_INFORMATION{
		BasicLimitInformation: windows.JOBOBJECT_BASIC_LIMIT_INFORMATION{
			LimitFlags: windows.JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE,
		},
	}
	if _, err = windows.SetInformationJobObject(job, windows.JobObjectExtendedLimitInformation,
		uintptr(unsafe.Pointer(&info)), uint32(unsafe.Sizeof(info))); err != nil {
		windows.CloseHandle(job)
		return 0, err
	}
	return job, nil
}

func newRequest(mode Mode, job windows.Handle) ([]byte, error) {
	cwd, err := os.Getwd()
	if err != nil {
		return nil, fmt.Errorf("read the working directory: %w", err)
	}
	env := make(map[string]string)
	for _, entry := range os.Environ() {
		key, value, ok := strings.Cut(entry, "=")
		if ok && key != "" && envAllowed(key) {
			env[key] = value
		}
	}
	line, err := json.Marshal(request{
		V:      protocolVersion,
		Mode:   mode,
		GuiPID: parentcheck.ParentPID,
		Stdin:  stdHandle(windows.STD_INPUT_HANDLE),
		Stdout: stdHandle(windows.STD_OUTPUT_HANDLE),
		Stderr: stdHandle(windows.STD_ERROR_HANDLE),
		Job:    uint64(job),
		Cwd:    cwd,
		Env:    env,
	})
	if err != nil {
		return nil, err
	}
	return append(line, '\n'), nil
}

func stdHandle(which uint32) uint64 {
	h, err := windows.GetStdHandle(which)
	if err != nil || h == windows.InvalidHandle {
		return 0
	}
	return uint64(h)
}

func readLines(file *os.File) <-chan string {
	lines := make(chan string, 4)
	go func() {
		defer close(lines)
		scanner := bufio.NewScanner(file)
		for scanner.Scan() {
			lines <- scanner.Text()
		}
	}()
	return lines
}

func (s *stub) next(timeout time.Duration) (string, error) {
	timer := time.NewTimer(timeout)
	defer timer.Stop()
	select {
	case line, ok := <-s.replies:
		if !ok {
			return "", errors.New("the elevated core closed its reply pipe")
		}
		if message, failed := strings.CutPrefix(line, "ERR "); failed {
			return "", errors.New(message)
		}
		return line, nil
	case <-timer.C:
		return "", fmt.Errorf("no answer from the elevated core within %v", timeout)
	}
}

// parseOK takes "OK <childPid> <handle>", the handle being the child's own process handle duplicated into this process.
func parseOK(line string) (windows.Handle, error) {
	fields := strings.Fields(line)
	if len(fields) != 3 || fields[0] != "OK" {
		return 0, fmt.Errorf("unexpected answer from the elevated core: %q", line)
	}
	pid, err := strconv.ParseUint(fields[1], 10, 32)
	if err != nil {
		return 0, fmt.Errorf("unexpected answer from the elevated core: %q", line)
	}
	value, err := strconv.ParseUint(fields[2], 10, 64)
	if err != nil {
		return 0, fmt.Errorf("unexpected answer from the elevated core: %q", line)
	}
	child := windows.Handle(value)
	if actual, err := windows.GetProcessId(child); err != nil || actual != uint32(pid) {
		return 0, fmt.Errorf("the elevated core's process handle does not refer to pid %d", pid)
	}
	return child, nil
}

// handOverPipe dials Throne only now that the child is up: Throne probes the core's privilege as soon as the pipe connects.
func (s *stub) handOverPipe() {
	socketName := os.Getenv("THRONE_CORE_SOCKET")
	if socketName == "" {
		log.Fatal("THRONE_CORE_SOCKET not set")
	}
	var pipe windows.Handle
	var err error
	for i := 0; i < 10; i++ {
		pipe, err = ipc.DialPipeHandle(socketName, parentcheck.ParentPID)
		if err == nil {
			break
		}
		log.Printf("IPC connect attempt %d/10 failed: %v", i+1, err)
		time.Sleep(500 * time.Millisecond)
	}
	if err != nil {
		log.Fatalf("failed to connect to GUI socket after 10 attempts: %v", err)
	}
	if _, err = fmt.Fprintf(s.request, "PIPE %d\n", pipe); err != nil {
		log.Fatalf("hand the IPC pipe to the elevated core: %v", err)
	}
	answer, err := s.next(replyTimeout)
	if err != nil {
		log.Fatalf("hand the IPC pipe to the elevated core: %v", err)
	}
	if answer != "PIPE-OK" {
		log.Fatalf("hand the IPC pipe to the elevated core: unexpected answer %q", answer)
	}
	// Our copy would keep the pipe open after the child exits, and Throne would never see the core disconnect.
	windows.CloseHandle(pipe)
}

func (s *stub) wait() int {
	event, err := windows.WaitForMultipleObjects([]windows.Handle{s.child, s.gui}, false, windows.INFINITE)
	if err != nil {
		log.Fatalf("wait for the elevated core: %v", err)
	}
	if event == windows.WAIT_OBJECT_0+1 {
		// The child watches Throne too: let it shut down on its own before exiting closes the job and kills it.
		_, _ = windows.WaitForSingleObject(s.child, uint32(guiExitGrace/time.Millisecond))
	}
	var code uint32
	if err = windows.GetExitCodeProcess(s.child, &code); err != nil {
		log.Fatalf("read the elevated core's exit code: %v", err)
	}
	if code == stillActive {
		log.Fatalln("parent exited")
	}
	return int(code)
}
