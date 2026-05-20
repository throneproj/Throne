//go:build unix

package process

import (
	"errors"
	"os"
	"os/exec"
	"os/user"
	"strconv"
	"strings"
	"syscall"
)

// applyPrivilegeDrop makes the child run as the unprivileged real user.
//
// The Core is a setuid-root binary, so when an unprivileged user launches it
// the effective uid is 0 but the real uid still identifies that user. We hand
// that real uid/gid to the kernel via SysProcAttr.Credential, which performs
// the setuid/setgid in the child before exec — race-free.
//
// If the Core is genuinely running as root (real uid 0, e.g. a root login or
// `sudo`), there is no unprivileged user to recover, so we refuse to launch an
// arbitrary binary with root privileges.
func applyPrivilegeDrop(cmd *exec.Cmd) error {
	if os.Geteuid() != 0 {
		return nil // not elevated, run as-is
	}
	ruid := os.Getuid()
	rgid := os.Getgid()
	if ruid == 0 {
		return errors.New("refusing to start extra process as root: no unprivileged user to drop to")
	}

	cmd.SysProcAttr = &syscall.SysProcAttr{
		Credential: &syscall.Credential{
			Uid:    uint32(ruid),
			Gid:    uint32(rgid),
			Groups: supplementaryGroups(ruid, rgid),
		},
	}
	cmd.Env = userEnv(cmd.Env, ruid)
	return nil
}

// supplementaryGroups resolves the real user's groups so the child keeps the
// access the user normally has. If the user database can't be read, fall back
// to just the primary gid (strictly fewer privileges, never more).
func supplementaryGroups(ruid, rgid int) []uint32 {
	fallback := []uint32{uint32(rgid)}
	u, err := user.LookupId(strconv.Itoa(ruid))
	if err != nil {
		return fallback
	}
	ids, err := u.GroupIds()
	if err != nil {
		return fallback
	}
	groups := make([]uint32, 0, len(ids))
	for _, id := range ids {
		if n, err := strconv.Atoi(id); err == nil {
			groups = append(groups, uint32(n))
		}
	}
	if len(groups) == 0 {
		return fallback
	}
	return groups
}

// userEnv replaces HOME/USER/LOGNAME with the dropped user's values so the
// child doesn't see root's identity.
func userEnv(env []string, ruid int) []string {
	u, err := user.LookupId(strconv.Itoa(ruid))
	if err != nil {
		return env
	}
	out := make([]string, 0, len(env)+3)
	for _, kv := range env {
		if name, _, ok := strings.Cut(kv, "="); ok {
			switch name {
			case "HOME", "USER", "LOGNAME":
				continue
			}
		}
		out = append(out, kv)
	}
	return append(out, "HOME="+u.HomeDir, "USER="+u.Username, "LOGNAME="+u.Username)
}

// makeConfigReadable ensures the de-privileged extra process can read the
// config the (possibly elevated) Core just wrote. Every change is made on the
// open file descriptor (fchown/fchmod), never by re-resolving the path, so a
// symlink swapped in at the path after creation cannot redirect a privileged
// chown/chmod onto an attacker-chosen target. The exact target uid/gid is
// known, so the mode stays tight (0600) instead of world-exposing a config
// that may carry secrets.
func makeConfigReadable(f *os.File) error {
	if os.Geteuid() != 0 {
		return f.Chmod(0o600) // fchmod(fd)
	}
	ruid := os.Getuid()
	rgid := os.Getgid()
	if ruid == 0 {
		// Start() refuses to launch in this case anyway.
		return nil
	}
	if err := f.Chown(ruid, rgid); err != nil { // fchown(fd): symlink-proof
		return err
	}
	return f.Chmod(0o600) // fchmod(fd)
}

// createSecureConfigFile creates the extra-process config file with a
// race-free, symlink-proof scheme and returns the open file plus the path to
// remove on cleanup.
//
// When not elevated there is no privilege boundary, so an ordinary temp file
// is fine. When elevated (setuid-root) the invoking user controls $TMPDIR, so
// os.TempDir() is untrusted: we instead create a fresh private directory in
// the standard, root-owned, sticky /tmp. Its random name can't be predicted
// and, being a root-owned entry in a sticky directory, it can't be unlinked or
// swapped by another user. The directory is left root-owned and chmod'd to
// 0711 (search-only): the child can traverse to the known config path but no
// unprivileged user can write into it or list it, while the config file itself
// (fchown'd to the user, 0600) stays secret. cleanup removes the directory.
func createSecureConfigFile() (*os.File, string, error) {
	if os.Geteuid() != 0 {
		f, err := os.CreateTemp("", "throne-extra-*.conf")
		if err != nil {
			return nil, "", err
		}
		return f, f.Name(), nil
	}

	dir, err := os.MkdirTemp("/tmp", "throne-extra-")
	if err != nil {
		return nil, "", err
	}
	// fchmod the directory via its own descriptor (no path re-resolution).
	if d, derr := os.Open(dir); derr == nil {
		_ = d.Chmod(0o711)
		_ = d.Close()
	}
	f, err := os.CreateTemp(dir, "extra-*.conf")
	if err != nil {
		_ = os.RemoveAll(dir)
		return nil, "", err
	}
	return f, dir, nil
}
