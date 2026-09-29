package process

import (
	"fmt"
	"os"
	"os/exec"
	"strings"

	"github.com/sagernet/sing/common/atomic"
)

const extraCorePrefix = "Extra Core"

type Process struct {
	path        string
	args        []string
	noOut       bool
	cleanupPath string
	run         running
	stopped     atomic.Bool
}

type running interface {
	Wait() error
	Kill() error
}

func NewProcess(path string, args []string, noOut bool) *Process {
	return &Process{path: path, args: args, noOut: noOut}
}

// The path is removed recursively when the process stops, exits, or fails to start; "" means nothing to clean up.
func (p *Process) SetCleanupPath(path string) {
	p.cleanupPath = path
}

func (p *Process) Start() error {
	run, err := startChild(p.path, p.args, p.noOut)
	if err != nil {
		p.cleanup()
		return err
	}
	p.run = run
	p.stopped.Store(false)

	go func() {
		fmt.Println(p.path, ":", "process started, waiting for it to end")
		_ = p.run.Wait()
		if !p.stopped.Load() {
			fmt.Println("Extra process exited unexpectedly")
		}
		p.cleanup()
	}()
	return nil
}

func (p *Process) Stop() {
	p.stopped.Store(true)
	if p.run != nil {
		_ = p.run.Kill()
	}
	p.cleanup()
}

func newCmd(path string, args []string, noOut bool) *exec.Cmd {
	cmd := exec.Command(path, args...)
	cmd.Stdout = &pipeLogger{prefix: extraCorePrefix, noOut: noOut}
	cmd.Stderr = &pipeLogger{prefix: extraCorePrefix, noOut: noOut}
	cmd.Env = childEnv()
	return cmd
}

func startCmd(cmd *exec.Cmd) (running, error) {
	if err := cmd.Start(); err != nil {
		return nil, err
	}
	return &cmdRunner{cmd: cmd}, nil
}

type cmdRunner struct{ cmd *exec.Cmd }

func (c *cmdRunner) Wait() error { return c.cmd.Wait() }
func (c *cmdRunner) Kill() error { return c.cmd.Process.Kill() }

// Safe to call repeatedly and from multiple goroutines; unlink does not traverse a final symlink.
func (p *Process) cleanup() {
	if p.cleanupPath != "" {
		_ = os.RemoveAll(p.cleanupPath)
	}
}

// Returns (configPath, cleanupPath): what the extra process reads, and what Process must remove afterwards.
func CreateExtraConfig(content string) (string, string, error) {
	f, cleanupPath, err := createSecureConfigFile()
	if err != nil {
		return "", "", err
	}
	configPath := f.Name()

	fail := func(e error) (string, string, error) {
		_ = f.Close()
		_ = os.RemoveAll(cleanupPath)
		return "", "", e
	}

	if _, err = f.WriteString(content); err != nil {
		return fail(err)
	}
	// Done on the fd, before Close, so a path swap cannot redirect this privileged chmod (TOCTOU).
	if err = makeConfigReadable(f); err != nil {
		return fail(err)
	}
	if err = f.Close(); err != nil {
		_ = os.RemoveAll(cleanupPath)
		return "", "", err
	}
	return configPath, cleanupPath, nil
}

func childEnv() []string {
	parent := os.Environ()
	out := make([]string, 0, len(parent))
	for _, kv := range parent {
		if name, _, ok := strings.Cut(kv, "="); ok && strings.HasPrefix(strings.ToUpper(name), "THRONE") {
			continue
		}
		out = append(out, kv)
	}
	return out
}
