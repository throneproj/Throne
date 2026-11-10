package elevation

import (
	"bufio"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"net"
	"os"
	runtimeDebug "runtime/debug"
	"strconv"
	"strings"
	"syscall"
	"unsafe"

	"ThroneCore/internal/ipc"
	"ThroneCore/internal/parentcheck"

	"golang.org/x/sys/windows"
)

// grant is <B>: written by the elevated grant into the task's arguments, so it is trusted, unlike arg0.
type grant struct {
	Core string `json:"core"`
	Gui  string `json:"gui"`
}

type child struct {
	stub    windows.Handle
	stubPID uint32
	reply   *os.File
}

// RunAttach is the elevated copy the task starts as --attach <B> <arg0>: it takes over the stub's stdio, job and, in core
// mode, Throne's IPC pipe, then calls run, which must not return.
func RunAttach(args []string, run func(Attached)) {
	attached := attach(args)
	run(attached)
	os.Exit(0)
}

func attach(args []string) Attached {
	// The image is GUI-subsystem and starts without std handles: nothing can be reported until the reply pipe is ours.
	if !Elevated() || len(args) != 2 {
		os.Exit(1)
	}
	g, err := decodeGrant(args[0])
	if err != nil {
		os.Exit(1)
	}
	stubPID, requestHandle, replyHandle, err := parseArg0(args[1])
	if err != nil {
		os.Exit(1)
	}
	stub, err := windows.OpenProcess(
		windows.PROCESS_DUP_HANDLE|windows.PROCESS_QUERY_LIMITED_INFORMATION|windows.SYNCHRONIZE, false, stubPID)
	if err != nil {
		os.Exit(1)
	}
	c := &child{stub: stub, stubPID: stubPID}
	reply, err := c.dup(replyHandle)
	if err != nil {
		os.Exit(1)
	}
	c.reply = os.NewFile(uintptr(reply), "elevation-reply")

	if err = c.verifyStub(g.Core); err != nil {
		c.fail("%v", err)
	}
	requestPipe, err := c.dup(requestHandle)
	if err != nil {
		c.fail("duplicate the request pipe: %v", err)
	}
	requestFile := os.NewFile(uintptr(requestPipe), "elevation-request")
	scanner := bufio.NewScanner(requestFile)
	scanner.Buffer(make([]byte, 0, 4096), maxRequest)
	line, err := scanLine(scanner, requestTimeout)
	if err != nil {
		c.fail("read the request: %v", err)
	}
	var req request
	if err = json.Unmarshal([]byte(line), &req); err != nil {
		c.fail("invalid request: %v", err)
	}
	if req.V != protocolVersion || (req.Mode != ModeCore && req.Mode != ModeGuard) || req.GuiPID <= 0 || req.Job == 0 {
		c.fail("unsupported request (version %d, mode %q)", req.V, req.Mode)
	}

	// Held until exit, like the stub's handle: neither pid can be reused while the watchers wait on it.
	gui, err := windows.OpenProcess(windows.SYNCHRONIZE|windows.PROCESS_QUERY_LIMITED_INFORMATION, false, uint32(req.GuiPID))
	if err != nil {
		c.fail("open Throne's process: %v", err)
	}
	if err = c.verifyGUI(gui, uint32(req.GuiPID), g.Gui); err != nil {
		c.fail("%v", err)
	}

	stdio := c.dupStdio(&req)
	job, err := c.dup(windows.Handle(req.Job))
	if err != nil {
		c.fail("duplicate the job object: %v", err)
	}
	joinErr := windows.AssignProcessToJobObject(job, windows.CurrentProcess())
	// Only the stub may hold the job open, or KILL_ON_JOB_CLOSE never fires.
	windows.CloseHandle(job)

	c.setStdio(stdio)
	if joinErr != nil {
		log.Printf("elevated core: cannot join the stub's job, relying on watching it instead: %v", joinErr)
	}
	applyEnv(req.Env)
	if err = os.Chdir(req.Cwd); err != nil {
		c.fail("change to the working directory: %v", err)
	}
	if err = setOwnerToUser(); err != nil {
		log.Printf("elevated core: cannot make the user the default owner of new objects: %v", err)
	}
	parentcheck.ParentPID = req.GuiPID

	var self windows.Handle
	if err = windows.DuplicateHandle(windows.CurrentProcess(), windows.CurrentProcess(), stub, &self,
		windows.SYNCHRONIZE|windows.PROCESS_QUERY_LIMITED_INFORMATION, false, 0); err != nil {
		c.fail("share the process handle: %v", err)
	}
	if _, err = fmt.Fprintf(c.reply, "OK %d %d\n", os.Getpid(), self); err != nil {
		os.Exit(1)
	}

	attached := Attached{Mode: req.Mode, GuiPID: req.GuiPID, StubPID: int(stubPID), Core: g.Core}
	if req.Mode == ModeCore {
		attached.Conn = c.takePipe(scanner, req.GuiPID)
	}
	requestFile.Close()
	c.reply.Close()
	return attached
}

func (c *child) fail(format string, args ...any) {
	_, _ = fmt.Fprintf(c.reply, "ERR %s\n", oneLine(fmt.Sprintf(format, args...)))
	os.Exit(1)
}

func (c *child) dup(h windows.Handle) (windows.Handle, error) {
	var local windows.Handle
	err := windows.DuplicateHandle(c.stub, h, windows.CurrentProcess(), &local, 0, false, windows.DUPLICATE_SAME_ACCESS)
	return local, err
}

func decodeGrant(encoded string) (grant, error) {
	var g grant
	raw, err := base64.RawURLEncoding.DecodeString(encoded)
	if err != nil {
		return g, err
	}
	if err = json.Unmarshal(raw, &g); err != nil {
		return g, err
	}
	if g.Core == "" || g.Gui == "" {
		return g, errors.New("incomplete grant")
	}
	return g, nil
}

// parseArg0 takes "<stubPid>:<requestReadHandle>:<replyWriteHandle>".
func parseArg0(arg0 string) (uint32, windows.Handle, windows.Handle, error) {
	fields := strings.Split(arg0, ":")
	if len(fields) != 3 {
		return 0, 0, 0, errors.New("malformed arg0")
	}
	pid, err := strconv.ParseUint(fields[0], 10, 32)
	if err != nil {
		return 0, 0, 0, err
	}
	request, err := strconv.ParseUint(fields[1], 10, 64)
	if err != nil {
		return 0, 0, 0, err
	}
	reply, err := strconv.ParseUint(fields[2], 10, 64)
	if err != nil {
		return 0, 0, 0, err
	}
	return uint32(pid), windows.Handle(request), windows.Handle(reply), nil
}

func (c *child) verifyStub(core string) error {
	path, err := parentcheck.ImagePath(c.stub)
	if err != nil {
		return fmt.Errorf("read the stub's executable: %w", err)
	}
	if !strings.EqualFold(path, parentcheck.FinalPath(core)) {
		return fmt.Errorf("the stub is %q, not the granted %q", path, core)
	}
	var stubSession, ownSession uint32
	if err = windows.ProcessIdToSessionId(c.stubPID, &stubSession); err != nil {
		return fmt.Errorf("read the stub's session: %w", err)
	}
	if err = windows.ProcessIdToSessionId(windows.GetCurrentProcessId(), &ownSession); err != nil {
		return fmt.Errorf("read the session: %w", err)
	}
	if stubSession != ownSession {
		return fmt.Errorf("the stub runs in session %d, not %d", stubSession, ownSession)
	}
	var stubToken windows.Token
	if err = windows.OpenProcessToken(c.stub, windows.TOKEN_QUERY, &stubToken); err != nil {
		return fmt.Errorf("open the stub's token: %w", err)
	}
	defer stubToken.Close()
	stubUser, err := stubToken.GetTokenUser()
	if err != nil {
		return fmt.Errorf("read the stub's user: %w", err)
	}
	ownUser, err := ownTokenUser()
	if err != nil {
		return fmt.Errorf("read the user: %w", err)
	}
	if !windows.EqualSid(stubUser.User.Sid, ownUser.User.Sid) {
		return fmt.Errorf("the stub runs as %s, not %s", stubUser.User.Sid, ownUser.User.Sid)
	}
	return nil
}

func (c *child) verifyGUI(gui windows.Handle, guiPID uint32, want string) error {
	path, err := parentcheck.ImagePath(gui)
	if err != nil {
		return fmt.Errorf("read Throne's executable: %w", err)
	}
	if !strings.EqualFold(path, parentcheck.FinalPath(want)) {
		return fmt.Errorf("the stub's parent is %q, not the granted %q", path, want)
	}
	parent, err := parentPID(c.stubPID)
	if err != nil {
		return err
	}
	if parent != guiPID {
		return fmt.Errorf("pid %d is not the stub's parent", guiPID)
	}
	// The snapshot's parent pid outlives the parent: a reused pid shows up as a process younger than its "child".
	guiStart, err := creationTime(gui)
	if err != nil {
		return err
	}
	stubStart, err := creationTime(c.stub)
	if err != nil {
		return err
	}
	if guiStart > stubStart {
		return fmt.Errorf("pid %d was reused after the stub started", guiPID)
	}
	return nil
}

func parentPID(pid uint32) (uint32, error) {
	snapshot, err := windows.CreateToolhelp32Snapshot(windows.TH32CS_SNAPPROCESS, 0)
	if err != nil {
		return 0, fmt.Errorf("process snapshot: %w", err)
	}
	defer windows.CloseHandle(snapshot)
	var entry windows.ProcessEntry32
	entry.Size = uint32(unsafe.Sizeof(entry))
	for err = windows.Process32First(snapshot, &entry); err == nil; err = windows.Process32Next(snapshot, &entry) {
		if entry.ProcessID == pid {
			return entry.ParentProcessID, nil
		}
	}
	return 0, fmt.Errorf("process %d is not in the process snapshot", pid)
}

func creationTime(process windows.Handle) (int64, error) {
	var creation, exit, kernel, user windows.Filetime
	if err := windows.GetProcessTimes(process, &creation, &exit, &kernel, &user); err != nil {
		return 0, fmt.Errorf("GetProcessTimes: %w", err)
	}
	return creation.Nanoseconds(), nil
}

type stdStream struct {
	which  uint32
	handle windows.Handle
	name   string
	std    *syscall.Handle
	file   **os.File
}

func (c *child) dupStdio(req *request) []stdStream {
	streams := []stdStream{
		{which: windows.STD_INPUT_HANDLE, name: "/dev/stdin", std: &syscall.Stdin, file: &os.Stdin},
		{which: windows.STD_OUTPUT_HANDLE, name: "/dev/stdout", std: &syscall.Stdout, file: &os.Stdout},
		{which: windows.STD_ERROR_HANDLE, name: "/dev/stderr", std: &syscall.Stderr, file: &os.Stderr},
	}
	for i, value := range []uint64{req.Stdin, req.Stdout, req.Stderr} {
		var err error
		if value == 0 {
			streams[i].handle, err = openNul(i == 0)
		} else {
			streams[i].handle, err = c.dup(windows.Handle(value))
		}
		if err != nil {
			c.fail("take over %s: %v", streams[i].name, err)
		}
	}
	return streams
}

func openNul(read bool) (windows.Handle, error) {
	access := uint32(windows.GENERIC_WRITE)
	if read {
		access = windows.GENERIC_READ
	}
	name, _ := windows.UTF16PtrFromString("NUL")
	return windows.CreateFile(name, access, windows.FILE_SHARE_READ|windows.FILE_SHARE_WRITE, nil, windows.OPEN_EXISTING, 0, 0)
}

// setStdio makes the stub's stdio this process's own: the runtime's crash output reads GetStdHandle, Go code os.Std*.
func (c *child) setStdio(streams []stdStream) {
	for _, stream := range streams {
		if err := windows.SetStdHandle(stream.which, stream.handle); err != nil {
			c.fail("set %s: %v", stream.name, err)
		}
		// os.NewFile skips its blocking-mode probe for syscall.Stdin, which can hang on a pipe (go.dev/issue/75949).
		*stream.std = syscall.Handle(stream.handle)
		*stream.file = os.NewFile(uintptr(stream.handle), stream.name)
	}
	log.SetOutput(os.Stderr)
}

func applyEnv(env map[string]string) {
	for key, value := range env {
		if !envAllowed(key) {
			continue
		}
		if err := os.Setenv(key, value); err != nil {
			log.Printf("elevated core: cannot set %s: %v", key, err)
			continue
		}
		if strings.EqualFold(key, "GOTRACEBACK") {
			runtimeDebug.SetTraceback(value)
		}
	}
}

// An elevated token's default owner is Administrators; files the core creates in the user's folders should stay the user's.
func setOwnerToUser() error {
	var token windows.Token
	if err := windows.OpenProcessToken(windows.CurrentProcess(), windows.TOKEN_ADJUST_DEFAULT|windows.TOKEN_QUERY, &token); err != nil {
		return err
	}
	defer token.Close()
	user, err := token.GetTokenUser()
	if err != nil {
		return err
	}
	owner := struct{ Owner *windows.SID }{user.User.Sid}
	return windows.SetTokenInformation(token, windows.TokenOwner, (*byte)(unsafe.Pointer(&owner)), uint32(unsafe.Sizeof(owner)))
}

// takePipe adopts Throne's IPC pipe, which the stub dials only once this process is ready to serve it.
func (c *child) takePipe(scanner *bufio.Scanner, guiPID int) net.Conn {
	line, err := scanLine(scanner, 0)
	if err != nil {
		c.fail("read the IPC pipe hand-off: %v", err)
	}
	value, ok := strings.CutPrefix(line, "PIPE ")
	if !ok {
		c.fail("unexpected hand-off %q", line)
	}
	remote, err := strconv.ParseUint(value, 10, 64)
	if err != nil {
		c.fail("unexpected hand-off %q", line)
	}
	pipe, err := c.dup(windows.Handle(remote))
	if err != nil {
		c.fail("duplicate the IPC pipe: %v", err)
	}
	if kind, err := windows.GetFileType(pipe); err != nil || kind != windows.FILE_TYPE_PIPE {
		windows.CloseHandle(pipe)
		c.fail("the IPC handle is not a pipe")
	}
	if err = ipc.CheckPipeServer(pipe, guiPID); err != nil {
		windows.CloseHandle(pipe)
		c.fail("%v", err)
	}
	conn, err := ipc.NewPipeConn(pipe)
	if err != nil {
		windows.CloseHandle(pipe)
		c.fail("adopt the IPC pipe: %v", err)
	}
	if _, err = fmt.Fprint(c.reply, "PIPE-OK\n"); err != nil {
		os.Exit(1)
	}
	return conn
}
