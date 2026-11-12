package pfctl

import (
	"bytes"
	"context"
	"errors"
	"fmt"
	"os/exec"
	"regexp"
	"slices"
	"strconv"
	"strings"
	"sync"
	"time"
)

const (
	path    = "/sbin/pfctl"
	timeout = 5 * time.Second
	// RootAnchor is the anchor /etc/pf.conf evaluates the children of, for filter and rdr rules alike.
	RootAnchor = "com.apple"
)

var (
	access       sync.Mutex
	tokenPattern = regexp.MustCompile(`(?m)^Token\s*:\s*(\d+)\s*$`)
	// ErrIncomplete means pfctl never reported a result.
	ErrIncomplete = errors.New("did not finish")
)

type Output struct {
	Stdout string
	Stderr string
	PID    int
}

// Run returns the output even on failure.
func Run(stdin []byte, args ...string) (Output, error) {
	access.Lock()
	defer access.Unlock()
	ctx, cancel := context.WithTimeout(context.Background(), timeout)
	defer cancel()
	cmd := exec.CommandContext(ctx, path, args...)
	cmd.Env = []string{"PATH=/usr/bin:/bin:/usr/sbin:/sbin"}
	if stdin != nil {
		cmd.Stdin = bytes.NewReader(stdin)
	}
	var stdout, stderr bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Stderr = &stderr
	err := cmd.Run()
	var output Output
	if cmd.Process != nil {
		output.PID = cmd.Process.Pid
	}
	output.Stdout, output.Stderr = stdout.String(), stderr.String()
	if err != nil {
		command := "pfctl " + strings.Join(args, " ")
		var exitErr *exec.ExitError
		if ctx.Err() != nil || !errors.As(err, &exitErr) {
			err = fmt.Errorf("%s: %w (%v)", command, ErrIncomplete, err)
		} else if detail := diagnostic(output.Stderr); detail != "" {
			err = fmt.Errorf("%s: %v: %s", command, err, detail)
		} else {
			err = fmt.Errorf("%s: %v", command, err)
		}
	}
	return output, err
}

// diagnostic drops the ALTQ warnings pfctl prints on every run.
func diagnostic(stderr string) string {
	var lines []string
	for line := range strings.Lines(stderr) {
		line = strings.TrimSpace(line)
		if line != "" && !strings.Contains(line, "ALTQ") {
			lines = append(lines, line)
		}
	}
	return strings.Join(lines, "; ")
}

// Enable takes a reference-counted enable (-E); -e and -d would switch pf for its other users too.
// The token comes back even with an error, so the caller can still release it.
func Enable() (token string, pid string, err error) {
	output, err := Run(nil, "-E")
	if match := tokenPattern.FindStringSubmatch(output.Stderr + "\n" + output.Stdout); match != nil {
		token, pid = match[1], strconv.Itoa(output.PID)
	}
	if err == nil && token == "" {
		err = errors.New("pfctl printed no reference token")
	}
	return
}

func FlushAnchor(anchor string) error {
	_, err := Run(nil, "-a", anchor, "-F", "all")
	return err
}

// ReleaseToken: pfctl -d frees every token and the kernel reissues freed values, so a token is ours only while listed under our enabler's pid.
func ReleaseToken(token string, pid string) error {
	references, err := Run(nil, "-s", "References")
	if err != nil {
		return err
	}
	for line := range strings.Lines(references.Stdout) {
		fields := strings.Fields(line)
		if len(fields) >= 3 && fields[0] == pid && slices.Contains(fields[1:], token) {
			_, err = Run(nil, "-X", token)
			return err
		}
	}
	return fmt.Errorf("pf reference %s of pid %s is no longer held", token, pid)
}
