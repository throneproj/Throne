//go:build windows

package process

import (
	"errors"
	"fmt"
	"os"
	"os/exec"
	"strings"
	"syscall"
	"unsafe"

	"golang.org/x/sys/windows"
)

// applyPrivilegeDrop makes the child run as the unprivileged interactive user.
//
// Windows has no setuid: a UAC-elevated parent spawns children with its own
// elevated admin token. We obtain a non-elevated primary token for the real
// user and hand it to CreateProcessAsUser via SysProcAttr.Token. If no
// unprivileged token can be obtained, we refuse to launch an arbitrary binary
// with admin rights.
func applyPrivilegeDrop(cmd *exec.Cmd) error {
	self, err := selfToken()
	if err != nil {
		return fmt.Errorf("cannot open process token: %w", err)
	}
	defer self.Close()

	if !self.IsElevated() {
		return nil // not elevated, run as-is
	}

	tok, err := unprivilegedToken(self)
	if err != nil {
		return fmt.Errorf("refusing to start extra process: cannot obtain an unprivileged user token: %w", err)
	}

	if cmd.SysProcAttr == nil {
		cmd.SysProcAttr = &syscall.SysProcAttr{}
	}
	cmd.SysProcAttr.Token = syscall.Token(tok)
	cmd.SysProcAttr.HideWindow = true
	return nil
}

func selfToken() (windows.Token, error) {
	var tok windows.Token
	err := windows.OpenProcessToken(windows.CurrentProcess(),
		windows.TOKEN_QUERY|windows.TOKEN_DUPLICATE, &tok)
	return tok, err
}

// unprivilegedToken returns a primary token for the real, non-elevated user,
// trying in order: the UAC linked token of the same user (the common
// elevated-admin case), the active console session user, then the interactive
// shell (explorer.exe) user (covers a SYSTEM service).
func unprivilegedToken(self windows.Token) (windows.Token, error) {
	if t, err := linkedToken(self); err == nil {
		return t, nil
	}
	if t, err := consoleSessionToken(); err == nil {
		return t, nil
	}
	if t, err := shellToken(); err == nil {
		return t, nil
	}
	return 0, errors.New("no linked, console-session, or shell token available")
}

func linkedToken(self windows.Token) (windows.Token, error) {
	linked, err := self.GetLinkedToken()
	if err != nil {
		return 0, err
	}
	defer linked.Close()
	if linked.IsElevated() {
		return 0, errors.New("linked token is still elevated")
	}
	return primaryToken(linked)
}

func consoleSessionToken() (windows.Token, error) {
	session := windows.WTSGetActiveConsoleSessionId()
	if session == 0xFFFFFFFF {
		return 0, errors.New("no active console session")
	}
	var tok windows.Token
	if err := windows.WTSQueryUserToken(session, &tok); err != nil {
		return 0, err
	}
	defer tok.Close()
	return primaryToken(tok)
}

func shellToken() (windows.Token, error) {
	pid, err := findProcess("explorer.exe")
	if err != nil {
		return 0, err
	}
	proc, err := windows.OpenProcess(windows.PROCESS_QUERY_LIMITED_INFORMATION, false, pid)
	if err != nil {
		return 0, err
	}
	defer windows.CloseHandle(proc)

	var tok windows.Token
	if err := windows.OpenProcessToken(proc, windows.TOKEN_DUPLICATE|windows.TOKEN_QUERY, &tok); err != nil {
		return 0, err
	}
	defer tok.Close()
	return primaryToken(tok)
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

func findProcess(name string) (uint32, error) {
	snap, err := windows.CreateToolhelp32Snapshot(windows.TH32CS_SNAPPROCESS, 0)
	if err != nil {
		return 0, err
	}
	defer windows.CloseHandle(snap)

	var entry windows.ProcessEntry32
	entry.Size = uint32(unsafe.Sizeof(entry))
	for err = windows.Process32First(snap, &entry); err == nil; err = windows.Process32Next(snap, &entry) {
		if strings.EqualFold(windows.UTF16ToString(entry.ExeFile[:]), name) {
			return entry.ProcessID, nil
		}
	}
	return 0, fmt.Errorf("process %q not found", name)
}

// createSecureConfigFile creates the extra-process config file. On Windows
// %TEMP% is a per-user directory and creating a symlink needs a privilege
// unprivileged users lack, so an ordinary O_CREATE|O_EXCL temp file (the
// default of os.CreateTemp) already creates a clean, un-hijackable file.
func createSecureConfigFile() (*os.File, string, error) {
	f, err := os.CreateTemp("", "throne-extra-*.conf")
	if err != nil {
		return nil, "", err
	}
	return f, f.Name(), nil
}

// makeConfigReadable best-effort grants the local Users group read access so a
// de-privileged child can read the config. To avoid a path swap between
// creation and the ACL change, it reopens the file WITHOUT following a reparse
// point and verifies (by volume + file id) that it is the very object we
// created before touching the DACL through that handle. A failure is
// non-fatal: with the linked-token path the child is the same user at lower
// integrity and can already read it.
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

// reopenSameObject opens f's path for DACL editing (WRITE_DAC|READ_CONTROL)
// without following a final reparse point, then confirms it is the same
// filesystem object as f (same volume + file id, not a reparse point). This
// defeats a symlink/path swap performed after the file was created.
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
