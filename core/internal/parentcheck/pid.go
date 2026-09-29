package parentcheck

import "os"

// Captured once: once the real parent dies getppid() returns a different pid, and every parent-identity check must agree.
var ParentPID = os.Getppid()
