package rpc

import (
	"context"
	"errors"
	"sync"

	"ThroneCore/internal/boxbox"
	"ThroneCore/internal/process"
	"ThroneCore/internal/xray"

	"github.com/xtls/xray-core/core"
)

// Serializes Start against Stop: the dispatcher gives every request its own goroutine.
var lifecycleMu sync.Mutex

// Guards the instance pointers; never held across a Create/Start, so pollers do not block behind a profile start.
var stateMu sync.RWMutex

var boxInstance *boxbox.Box

var instanceCancel context.CancelFunc

// Exactly one is set while a profile runs: xrayInstance when eager, xrayGate when the profile asked it to stay cold.
var xrayInstance *core.Instance

var xrayGate *xray.Gate

// One gate per opaque full config; never merged into the sidecar above.
var xrayFullGates []*xray.Gate

// Reached only from Start/Stop, i.e. always under lifecycleMu.
var extraProcess *process.Process

var needUnsetDNS bool

var stopEgressForwardingWatch func()

var errInstanceNotRunning = errors.New("Instance is not running")

func currentBox() *boxbox.Box {
	stateMu.RLock()
	defer stateMu.RUnlock()
	return boxInstance
}

func currentInstance() (*boxbox.Box, context.CancelFunc) {
	stateMu.RLock()
	defer stateMu.RUnlock()
	return boxInstance, instanceCancel
}

func setBoxInstance(box *boxbox.Box, cancel context.CancelFunc) {
	stateMu.Lock()
	defer stateMu.Unlock()
	boxInstance, instanceCancel = box, cancel
}

func setXray(instance *core.Instance, gate *xray.Gate) {
	stateMu.Lock()
	defer stateMu.Unlock()
	xrayInstance, xrayGate = instance, gate
}

func setXrayFullGates(gates []*xray.Gate) {
	stateMu.Lock()
	defer stateMu.Unlock()
	xrayFullGates = gates
}

// Shorter than the profile's config list: a gated instance is absent between activations.
func liveXrayInstances() []*core.Instance {
	stateMu.RLock()
	instance, gate := xrayInstance, xrayGate
	fullGates := xrayFullGates
	stateMu.RUnlock()

	var instances []*core.Instance
	if instance != nil {
		instances = append(instances, instance)
	} else if gate != nil {
		if live := gate.Instance(); live != nil {
			instances = append(instances, live)
		}
	}
	for _, fullGate := range fullGates {
		if live := fullGate.Instance(); live != nil {
			instances = append(instances, live)
		}
	}
	return instances
}
