package elevation

import (
	"bufio"
	"fmt"
	"io"
	"os"
	"strings"
	"time"
)

const (
	protocolVersion = 1
	replyTimeout    = 15 * time.Second
	requestTimeout  = 15 * time.Second
	maxRequest      = 1 << 20
	guiExitGrace    = 10 * time.Second
	stillActive     = 259
)

// request is the one JSON line the stub writes; handles are values in the stub's handle table, 0 for none.
type request struct {
	V      int               `json:"v"`
	Mode   Mode              `json:"mode"`
	GuiPID int               `json:"gui_pid"`
	Stdin  uint64            `json:"stdin"`
	Stdout uint64            `json:"stdout"`
	Stderr uint64            `json:"stderr"`
	Job    uint64            `json:"job"`
	Cwd    string            `json:"cwd"`
	Env    map[string]string `json:"env"`
}

// The child's environment is the user's own from Task Scheduler; only these keys come from Throne.
func envAllowed(key string) bool {
	key = strings.ToUpper(key)
	switch key {
	case "THRONE_CORE_SOCKET", "THRONE_CORE_DEBUG", "THRONE_GUARD", "GOTRACEBACK", "XRAY_LOCATION_ASSET":
		return true
	}
	return strings.HasPrefix(key, "XRAY_") || strings.HasPrefix(key, "ENABLE_DEPRECATED_")
}

func Requested() bool {
	return os.Getenv(EnvTask) != "" && !Elevated()
}

// scanLine waits for the next line; timeout 0 waits for as long as it takes.
func scanLine(scanner *bufio.Scanner, timeout time.Duration) (string, error) {
	result := make(chan error, 1)
	go func() {
		if scanner.Scan() {
			result <- nil
			return
		}
		err := scanner.Err()
		if err == nil {
			err = io.EOF
		}
		result <- err
	}()
	var expired <-chan time.Time
	if timeout > 0 {
		timer := time.NewTimer(timeout)
		defer timer.Stop()
		expired = timer.C
	}
	select {
	case err := <-result:
		if err != nil {
			return "", err
		}
		return scanner.Text(), nil
	case <-expired:
		return "", fmt.Errorf("nothing received within %v", timeout)
	}
}

func oneLine(text string) string {
	return strings.NewReplacer("\r", " ", "\n", " ").Replace(text)
}
