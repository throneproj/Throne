// Package elevation runs the Windows core and guard elevated through the per-user scheduled task a one-time UAC grant
// registers: the unelevated ThroneCore Throne starts becomes a stub that hands its stdio, IPC pipe and a job object to
// an elevated copy of itself, then mirrors its exit code.
package elevation

import "net"

// EnvTask is the task path Throne sets when the core should start elevated.
const EnvTask = "THRONE_ELEVATION_TASK"

type Mode string

const (
	ModeCore  Mode = "core"
	ModeGuard Mode = "guard"
)

// Attached is what an elevated child took over from its stub.
type Attached struct {
	Mode    Mode
	GuiPID  int
	StubPID int
	// Core is the install's ThroneCore.exe the grant was made for, which the stub runs from.
	Core string
	// Conn is Throne's IPC pipe; core mode only.
	Conn net.Conn
}
