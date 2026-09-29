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

func startChild(path string, args []string, noOut bool) (running, error) {
	cmd := newCmd(path, args, noOut)
	if err := applyPrivilegeDrop(cmd); err != nil {
		return nil, err
	}
	return startCmd(cmd)
}

// Under setuid-root the real uid still identifies the launching user; SysProcAttr.Credential setuids in the child before exec, race-free.
func applyPrivilegeDrop(cmd *exec.Cmd) error {
	if os.Geteuid() != 0 {
		return nil
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

// os.File.Chown/Chmod are fchown/fchmod on the fd, never a path re-resolution, so a swapped symlink cannot redirect them.
func makeConfigReadable(f *os.File) error {
	if os.Geteuid() != 0 {
		return f.Chmod(0o600)
	}
	ruid := os.Getuid()
	rgid := os.Getgid()
	if ruid == 0 {
		// Start() refuses to launch in this case anyway.
		return nil
	}
	if err := f.Chown(ruid, rgid); err != nil {
		return err
	}
	return f.Chmod(0o600)
}

// When elevated, $TMPDIR is attacker-controlled: use a root-owned 0711 directory in sticky /tmp, which no unprivileged user can swap or list.
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
