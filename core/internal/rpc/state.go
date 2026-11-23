package rpc

import (
	"context"
	"errors"
	"sync"

	"ThroneCore/internal/boxmain"
	"ThroneCore/internal/process"
	"ThroneCore/internal/xray"

	"github.com/sagernet/sing-box/option"
	"github.com/xtls/xray-core/core"
)

// Serializes Start against Stop: the dispatcher gives every request its own goroutine.
var lifecycleMu sync.Mutex

// Guards the instance pointers; never held across a Create/Start, so pollers do not block behind a profile start.
var stateMu sync.RWMutex

var boxInstance *boxmain.Box

var instanceCancel context.CancelFunc

// Published before its sidecars start, so each one is visible as soon as it is up.
var xrayStack *xray.Stack

// Reached only from Start/Stop, i.e. always under lifecycleMu.
var extraProcess *process.Process

var stopEgressForwardingWatch func()

var untrackDNSTunnels func()

var errInstanceNotRunning = errors.New("Instance is not running")

func currentBox() *boxmain.Box {
	stateMu.RLock()
	defer stateMu.RUnlock()
	return boxInstance
}

func currentInstance() (*boxmain.Box, context.CancelFunc) {
	stateMu.RLock()
	defer stateMu.RUnlock()
	return boxInstance, instanceCancel
}

func setBoxInstance(box *boxmain.Box, cancel context.CancelFunc, routeRules []option.Rule) {
	stateMu.Lock()
	defer stateMu.Unlock()
	boxInstance, instanceCancel = box, cancel
	connRules.reset(box, routeRules)
}

func setXrayStack(stack *xray.Stack) {
	stateMu.Lock()
	defer stateMu.Unlock()
	xrayStack = stack
}

// The slot is cleared before the close so no reader picks up a stack that is going away.
func closeXray() {
	stateMu.Lock()
	stack := xrayStack
	xrayStack = nil
	stateMu.Unlock()
	stack.Close()
}

func liveXrayInstances() []*core.Instance {
	stateMu.RLock()
	stack := xrayStack
	stateMu.RUnlock()
	return stack.Instances()
}
