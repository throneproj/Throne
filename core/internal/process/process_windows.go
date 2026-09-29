//go:build windows

package process

import (
	"errors"
	"fmt"
	"io"
	"log"
	"os"
	"os/exec"
	"runtime"
	"strings"
	"sync"
	"syscall"
	"unsafe"

	"golang.org/x/sys/windows"
)

// exec.Cmd+SysProcAttr.Token routes through CreateProcessAsUser, needing SeAssignPrimaryTokenPrivilege that elevated admins lack; CreateProcessWithTokenW needs only SeImpersonatePrivilege (#1482).
func startChild(path string, args []string, noOut bool) (running, error) {
	self, err := selfToken()
	if err != nil {
		return nil, fmt.Errorf("cannot open process token: %w", err)
	}
	defer self.Close()

	if !self.IsElevated() {
		return startCmd(newCmd(path, args, noOut))
	}

	tok, err := unprivilegedToken(self)
	if err != nil {
		return nil, fmt.Errorf("refusing to start extra process: cannot obtain an unprivileged user token: %w", err)
	}
	defer tok.Close()

	return startWithToken(path, args, noOut, tok)
}

// CreateProcessWithTokenW is served by the Secondary Logon service, which some systems disable; CreateProcessAsUser then still works for a Core running as SYSTEM.
func startWithToken(path string, args []string, noOut bool, tok windows.Token) (running, error) {
	run, err := startWithTokenW(path, args, noOut, tok)
	if err == nil {
		return run, nil
	}
	cmd := newCmd(path, args, noOut)
	cmd.SysProcAttr = &syscall.SysProcAttr{
		Token:         syscall.Token(tok),
		HideWindow:    true,
		CreationFlags: windows.CREATE_NO_WINDOW,
	}
	run, asUserErr := startCmd(cmd)
	if asUserErr != nil {
		return nil, errors.Join(err, fmt.Errorf("CreateProcessAsUser: %w", asUserErr))
	}
	return run, nil
}

var procCreateProcessWithTokenW = windows.NewLazySystemDLL("advapi32.dll").NewProc("CreateProcessWithTokenW")

// CreateProcessWithTokenW inherits no arbitrary handles, but the secondary-logon service still duplicates the three std handles into a 64-bit child.
func startWithTokenW(path string, args []string, noOut bool, tok windows.Token) (running, error) {
	exe, err := exec.LookPath(path)
	if err != nil {
		return nil, err
	}

	outR, outW, err := os.Pipe()
	if err != nil {
		return nil, err
	}
	errR, errW, err := os.Pipe()
	if err != nil {
		closeAll(outR, outW)
		return nil, err
	}
	nul, err := os.OpenFile("NUL", os.O_RDONLY, 0)
	if err != nil {
		closeAll(outR, outW, errR, errW)
		return nil, err
	}
	for _, f := range []*os.File{outW, errW, nul} {
		if err = makeInheritable(f); err != nil {
			closeAll(outR, outW, errR, errW, nul)
			return nil, err
		}
	}

	si := &windows.StartupInfo{}
	si.Cb = uint32(unsafe.Sizeof(*si))
	si.Flags = windows.STARTF_USESTDHANDLES | windows.STARTF_USESHOWWINDOW
	si.ShowWindow = windows.SW_HIDE
	si.StdInput = windows.Handle(nul.Fd())
	si.StdOutput = windows.Handle(outW.Fd())
	si.StdErr = windows.Handle(errW.Fd())

	appName, err := windows.UTF16PtrFromString(exe)
	if err != nil {
		closeAll(outR, outW, errR, errW, nul)
		return nil, err
	}
	cmdLine, err := windows.UTF16PtrFromString(windows.ComposeCommandLine(append([]string{exe}, args...)))
	if err != nil {
		closeAll(outR, outW, errR, errW, nul)
		return nil, err
	}
	envBlock, err := makeEnvBlock(childEnv())
	if err != nil {
		closeAll(outR, outW, errR, errW, nul)
		return nil, err
	}

	// dwLogonFlags 0: don't load the user profile.
	const createFlags = windows.CREATE_UNICODE_ENVIRONMENT | windows.CREATE_NO_WINDOW
	var pi windows.ProcessInformation
	r1, _, e1 := procCreateProcessWithTokenW.Call(
		uintptr(tok),
		0,
		uintptr(unsafe.Pointer(appName)),
		uintptr(unsafe.Pointer(cmdLine)),
		uintptr(createFlags),
		uintptr(unsafe.Pointer(&envBlock[0])),
		0,
		uintptr(unsafe.Pointer(si)),
		uintptr(unsafe.Pointer(&pi)),
	)
	runtime.KeepAlive(si)
	runtime.KeepAlive(appName)
	runtime.KeepAlive(cmdLine)
	runtime.KeepAlive(envBlock)
	// Closing our write ends is what lets the read ends see EOF when the child exits.
	closeAll(outW, errW, nul)
	if r1 == 0 {
		closeAll(outR, errR)
		return nil, fmt.Errorf("CreateProcessWithTokenW %s: %w", exe, e1)
	}
	_ = windows.CloseHandle(pi.Thread)

	done := make(chan struct{}, 2)
	pump := func(r *os.File) {
		_, _ = io.Copy(&pipeLogger{prefix: extraCorePrefix, noOut: noOut}, r)
		_ = r.Close()
		done <- struct{}{}
	}
	go pump(outR)
	go pump(errR)

	return &tokenRunner{hProcess: pi.Process, done: done}, nil
}

// done receives once per output pump; Wait must drain both before closing the handle.
type tokenRunner struct {
	mu       sync.Mutex
	hProcess windows.Handle
	done     chan struct{}
}

func (t *tokenRunner) Wait() error {
	t.mu.Lock()
	h := t.hProcess
	t.mu.Unlock()
	if h == 0 {
		return nil
	}
	_, err := windows.WaitForSingleObject(h, windows.INFINITE)
	<-t.done
	<-t.done
	t.mu.Lock()
	if t.hProcess != 0 {
		_ = windows.CloseHandle(t.hProcess)
		t.hProcess = 0
	}
	t.mu.Unlock()
	return err
}

// No-op once Wait has reaped the process, so the handle Wait closed is never reused.
func (t *tokenRunner) Kill() error {
	t.mu.Lock()
	defer t.mu.Unlock()
	if t.hProcess == 0 {
		return nil
	}
	return windows.TerminateProcess(t.hProcess, 1)
}

func makeInheritable(f *os.File) error {
	return windows.SetHandleInformation(windows.Handle(f.Fd()),
		windows.HANDLE_FLAG_INHERIT, windows.HANDLE_FLAG_INHERIT)
}

func closeAll(files ...*os.File) {
	for _, f := range files {
		_ = f.Close()
	}
}

func makeEnvBlock(env []string) ([]uint16, error) {
	var block []uint16
	for _, e := range env {
		if e == "" {
			continue
		}
		u, err := windows.UTF16FromString(e)
		if err != nil {
			return nil, err
		}
		block = append(block, u...) // u already ends in NUL
	}
	block = append(block, 0)
	if len(block) == 1 {
		block = append(block, 0) // an empty environment still needs a double NUL
	}
	return block, nil
}

func selfToken() (windows.Token, error) {
	var tok windows.Token
	err := windows.OpenProcessToken(windows.CurrentProcess(),
		windows.TOKEN_QUERY|windows.TOKEN_DUPLICATE, &tok)
	return tok, err
}

type tokenSource struct {
	name string
	get  func(windows.Token) (windows.Token, error)
}

// Ordered best-first: the linked token is exactly what the user would run with without UAC, the session/shell tokens cover a Core running as SYSTEM, and the restricted self token always works because it needs nothing outside this process.
var tokenSources = []tokenSource{
	{"linked token", linkedToken},
	{"session token", sessionToken},
	{"shell token", shellToken},
	{"restricted self token", restrictedSelfToken},
}

func unprivilegedToken(self windows.Token) (windows.Token, error) {
	var errs []error
	for _, src := range tokenSources {
		tok, err := src.get(self)
		if err == nil {
			log.Printf("%s: dropping privileges via %s", extraCorePrefix, src.name)
			return tok, nil
		}
		errs = append(errs, fmt.Errorf("%s: %w", src.name, err))
	}
	return 0, errors.Join(errs...)
}

func linkedToken(self windows.Token) (windows.Token, error) {
	return unelevated(self)
}

// Primary duplicate of src, or of its linked token when src itself is elevated.
func unelevated(src windows.Token) (windows.Token, error) {
	if !src.IsElevated() {
		return primaryToken(src)
	}
	linked, err := src.GetLinkedToken()
	if err != nil {
		return 0, fmt.Errorf("elevated token has no linked token: %w", err)
	}
	defer linked.Close()
	if linked.IsElevated() {
		return 0, errors.New("linked token is still elevated")
	}
	return primaryToken(linked)
}

// WTSQueryUserToken needs SeTcbPrivilege, so this only ever succeeds when the Core runs as SYSTEM.
func sessionToken(windows.Token) (windows.Token, error) {
	sessions := candidateSessions()
	if len(sessions) == 0 {
		return 0, errors.New("no active session")
	}
	var errs []error
	for _, id := range sessions {
		var raw windows.Token
		if err := windows.WTSQueryUserToken(id, &raw); err != nil {
			errs = append(errs, fmt.Errorf("session %d: %w", id, err))
			continue
		}
		tok, err := unelevated(raw)
		raw.Close()
		if err == nil {
			return tok, nil
		}
		errs = append(errs, fmt.Errorf("session %d: %w", id, err))
	}
	return 0, errors.Join(errs...)
}

// Every explorer.exe is tried, nearest session first: a second signed-in user or a restarted shell otherwise makes the single candidate we used to pick arbitrary (#1794).
func shellToken(windows.Token) (windows.Token, error) {
	pids := shellCandidates(findProcesses("explorer.exe"), candidateSessions())
	if len(pids) == 0 {
		return 0, errors.New("no explorer.exe in a usable session")
	}
	var errs []error
	for _, pid := range pids {
		raw, err := openProcessToken(pid)
		if err != nil {
			errs = append(errs, fmt.Errorf("pid %d: %w", pid, err))
			continue
		}
		tok, err := unelevated(raw)
		raw.Close()
		if err == nil {
			return tok, nil
		}
		errs = append(errs, fmt.Errorf("pid %d: %w", pid, err))
	}
	return 0, errors.Join(errs...)
}

// Last resort, built the way UAC builds its filtered token: no privileges, admin groups deny-only, medium integrity. Same user and session as us, so the child keeps a working profile and %TEMP%.
func restrictedSelfToken(self windows.Token) (windows.Token, error) {
	primary, err := primaryToken(self)
	if err != nil {
		return 0, err
	}
	defer primary.Close()

	groups, err := primary.GetTokenGroups()
	if err != nil {
		return 0, err
	}
	restricted, err := createRestrictedToken(primary, adminGroups(groups))
	runtime.KeepAlive(groups)
	if err != nil {
		return 0, err
	}
	// Duplication preserves the disabled SIDs and deleted privileges, and pins the access mask the integrity change below needs.
	tok, err := primaryToken(restricted)
	restricted.Close()
	if err != nil {
		return 0, err
	}
	if err = setMediumIntegrity(tok); err != nil {
		tok.Close()
		return 0, fmt.Errorf("cannot lower integrity level: %w", err)
	}
	if tok.IsElevated() {
		tok.Close()
		return 0, errors.New("restricted token is still elevated")
	}
	return tok, nil
}

// Only groups actually present in the token: CreateRestrictedToken rejects a SID the token does not carry.
func adminGroups(groups *windows.Tokengroups) []windows.SIDAndAttributes {
	var out []windows.SIDAndAttributes
	for _, g := range groups.AllGroups() {
		if isAdminSid(g.Sid) {
			out = append(out, windows.SIDAndAttributes{Sid: g.Sid})
		}
	}
	return out
}

// The domain-relative admin RIDs need the manual check; IsWellKnown cannot match them without a domain SID.
func isAdminSid(sid *windows.SID) bool {
	if sid == nil || !sid.IsValid() {
		return false
	}
	if sid.IsWellKnown(windows.WinBuiltinAdministratorsSid) {
		return true
	}
	n := sid.SubAuthorityCount()
	if n < 2 || sid.SubAuthority(0) != 21 {
		return false
	}
	switch sid.SubAuthority(uint32(n) - 1) {
	case 512, 518, 519, 520: // Domain, Schema, Enterprise Admins, Group Policy Creator Owners
		return true
	}
	return false
}

var procCreateRestrictedToken = windows.NewLazySystemDLL("advapi32.dll").NewProc("CreateRestrictedToken")

const (
	disableMaxPrivilege = 0x1
	luaToken            = 0x4
)

func createRestrictedToken(src windows.Token, disable []windows.SIDAndAttributes) (windows.Token, error) {
	call := func(flags uintptr) (windows.Token, error) {
		var sids *windows.SIDAndAttributes
		if len(disable) > 0 {
			sids = &disable[0]
		}
		var tok windows.Token
		r1, _, e1 := procCreateRestrictedToken.Call(
			uintptr(src),
			flags,
			uintptr(len(disable)),
			uintptr(unsafe.Pointer(sids)),
			0, 0, 0, 0,
			uintptr(unsafe.Pointer(&tok)),
		)
		runtime.KeepAlive(disable)
		if r1 == 0 {
			return 0, e1
		}
		return tok, nil
	}
	if tok, err := call(disableMaxPrivilege | luaToken); err == nil {
		return tok, nil
	}
	return call(disableMaxPrivilege)
}

func setMediumIntegrity(tok windows.Token) error {
	sid, err := windows.CreateWellKnownSid(windows.WinMediumLabelSid)
	if err != nil {
		return err
	}
	label := windows.Tokenmandatorylabel{
		Label: windows.SIDAndAttributes{Sid: sid, Attributes: windows.SE_GROUP_INTEGRITY},
	}
	return windows.SetTokenInformation(tok, windows.TokenIntegrityLevel,
		(*byte)(unsafe.Pointer(&label)), label.Size())
}

// Our own session first, then the physical console, then any other active one; deduplicated, best first.
func candidateSessions() []uint32 {
	var out []uint32
	add := func(id uint32) {
		if id == 0 || id == 0xFFFFFFFF {
			return
		}
		for _, seen := range out {
			if seen == id {
				return
			}
		}
		out = append(out, id)
	}
	if id, ok := processSession(windows.GetCurrentProcessId()); ok {
		add(id)
	}
	add(windows.WTSGetActiveConsoleSessionId())
	for _, id := range activeSessions() {
		add(id)
	}
	return out
}

func activeSessions() []uint32 {
	var info *windows.WTS_SESSION_INFO
	var count uint32
	if err := windows.WTSEnumerateSessions(0, 0, 1, &info, &count); err != nil {
		return nil
	}
	defer windows.WTSFreeMemory(uintptr(unsafe.Pointer(info)))

	var out []uint32
	for _, s := range unsafe.Slice(info, count) {
		if s.State == windows.WTSActive {
			out = append(out, s.SessionID)
		}
	}
	return out
}

func processSession(pid uint32) (uint32, bool) {
	var id uint32
	if err := windows.ProcessIdToSessionId(pid, &id); err != nil {
		return 0, false
	}
	return id, true
}

// Keeps only shells in a session we care about, nearest first: another signed-in user's token would put the child in their session, where our config file is unreachable.
func shellCandidates(pids []uint32, prefer []uint32) []uint32 {
	if len(prefer) == 0 {
		return pids
	}
	out := make([]uint32, 0, len(pids))
	for _, want := range prefer {
		for _, pid := range pids {
			if id, ok := processSession(pid); ok && id == want {
				out = append(out, pid)
			}
		}
	}
	return out
}

// OpenProcessToken documents PROCESS_QUERY_INFORMATION; the limited right also works on modern Windows and is all a hardened process will hand out.
func openProcessToken(pid uint32) (windows.Token, error) {
	var errs []error
	for _, access := range []uint32{windows.PROCESS_QUERY_INFORMATION, windows.PROCESS_QUERY_LIMITED_INFORMATION} {
		proc, err := windows.OpenProcess(access, false, pid)
		if err != nil {
			errs = append(errs, err)
			continue
		}
		var tok windows.Token
		err = windows.OpenProcessToken(proc, windows.TOKEN_DUPLICATE|windows.TOKEN_QUERY, &tok)
		_ = windows.CloseHandle(proc)
		if err == nil {
			return tok, nil
		}
		errs = append(errs, err)
	}
	return 0, errors.Join(errs...)
}

func primaryToken(src windows.Token) (windows.Token, error) {
	var dup windows.Token
	err := windows.DuplicateTokenEx(
		src,
		windows.TOKEN_ASSIGN_PRIMARY|windows.TOKEN_DUPLICATE|windows.TOKEN_QUERY|
			windows.TOKEN_ADJUST_DEFAULT|windows.TOKEN_ADJUST_SESSIONID,
		nil,
		windows.SecurityImpersonation,
		windows.TokenPrimary,
		&dup,
	)
	if err != nil {
		return 0, err
	}
	return dup, nil
}

func findProcesses(name string) []uint32 {
	snap, err := windows.CreateToolhelp32Snapshot(windows.TH32CS_SNAPPROCESS, 0)
	if err != nil {
		return nil
	}
	defer windows.CloseHandle(snap)

	var out []uint32
	var entry windows.ProcessEntry32
	entry.Size = uint32(unsafe.Sizeof(entry))
	for err = windows.Process32First(snap, &entry); err == nil; err = windows.Process32Next(snap, &entry) {
		if strings.EqualFold(windows.UTF16ToString(entry.ExeFile[:]), name) {
			out = append(out, entry.ProcessID)
		}
	}
	return out
}

// %TEMP% is per-user and symlink creation needs a privilege, so os.CreateTemp's O_CREATE|O_EXCL file is already un-hijackable.
func createSecureConfigFile() (*os.File, string, error) {
	f, err := os.CreateTemp("", "throne-extra-*.conf")
	if err != nil {
		return nil, "", err
	}
	return f, f.Name(), nil
}

// Best-effort: every failure returns nil, since on the linked-token path the child is the same user and can already read the file.
func makeConfigReadable(f *os.File) error {
	usersSid, err := windows.CreateWellKnownSid(windows.WinBuiltinUsersSid)
	if err != nil {
		return nil
	}
	h, err := reopenSameObject(f)
	if err != nil {
		return nil
	}
	defer windows.CloseHandle(h)

	sd, err := windows.GetSecurityInfo(h, windows.SE_FILE_OBJECT, windows.DACL_SECURITY_INFORMATION)
	if err != nil {
		return nil
	}
	dacl, _, err := sd.DACL()
	if err != nil {
		return nil
	}
	entries := []windows.EXPLICIT_ACCESS{{
		AccessPermissions: windows.GENERIC_READ,
		AccessMode:        windows.GRANT_ACCESS,
		Inheritance:       windows.NO_INHERITANCE,
		Trustee: windows.TRUSTEE{
			TrusteeForm:  windows.TRUSTEE_IS_SID,
			TrusteeType:  windows.TRUSTEE_IS_GROUP,
			TrusteeValue: windows.TrusteeValueFromSID(usersSid),
		},
	}}
	merged, err := windows.ACLFromEntries(entries, dacl)
	if err != nil {
		return nil
	}
	_ = windows.SetSecurityInfo(h, windows.SE_FILE_OBJECT,
		windows.DACL_SECURITY_INFORMATION, nil, nil, merged, nil)
	return nil
}

// Reopens without following a final reparse point and re-checks volume+file id, defeating a path swap made after creation.
func reopenSameObject(f *os.File) (windows.Handle, error) {
	namep, err := windows.UTF16PtrFromString(f.Name())
	if err != nil {
		return 0, err
	}
	h, err := windows.CreateFile(
		namep,
		windows.WRITE_DAC|windows.READ_CONTROL,
		windows.FILE_SHARE_READ|windows.FILE_SHARE_WRITE|windows.FILE_SHARE_DELETE,
		nil,
		windows.OPEN_EXISTING,
		windows.FILE_FLAG_OPEN_REPARSE_POINT|windows.FILE_FLAG_BACKUP_SEMANTICS,
		0,
	)
	if err != nil {
		return 0, err
	}
	var reopened, original windows.ByHandleFileInformation
	if err = windows.GetFileInformationByHandle(h, &reopened); err != nil {
		_ = windows.CloseHandle(h)
		return 0, err
	}
	if err = windows.GetFileInformationByHandle(windows.Handle(f.Fd()), &original); err != nil {
		_ = windows.CloseHandle(h)
		return 0, err
	}
	if reopened.VolumeSerialNumber != original.VolumeSerialNumber ||
		reopened.FileIndexHigh != original.FileIndexHigh ||
		reopened.FileIndexLow != original.FileIndexLow ||
		reopened.FileAttributes&windows.FILE_ATTRIBUTE_REPARSE_POINT != 0 {
		_ = windows.CloseHandle(h)
		return 0, errors.New("config file identity mismatch (possible path swap)")
	}
	return h, nil
}
