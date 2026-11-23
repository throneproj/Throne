package xray

import (
	"bytes"
	"github.com/xtls/xray-core/core"
	"github.com/xtls/xray-core/infra/conf/serial"
)

func CreateXrayInstance(config string) (*core.Instance, error) {
	r := bytes.NewReader([]byte(config))
	conf, err := serial.DecodeJSONConfig(r)
	if err != nil {
		return nil, err
	}

	b, err := conf.Build()
	if err != nil {
		return nil, err
	}

	server, err := core.New(b)
	if err != nil {
		return nil, err
	}

	return server, nil
}

// prepare runs between core.New and Start; an instance that fails either is closed.
func StartInstance(config string, prepare func(*core.Instance) error) (*core.Instance, error) {
	instance, err := CreateXrayInstance(config)
	if err != nil {
		return nil, err
	}
	if err = prepare(instance); err != nil {
		_ = instance.Close()
		return nil, err
	}
	if err = instance.Start(); err != nil {
		_ = instance.Close()
		return nil, err
	}
	return instance, nil
}

// On failure the ones already started are closed; on success the caller must close them.
func StartInstances(configs []string, prepare func(*core.Instance) error) ([]*core.Instance, error) {
	instances := make([]*core.Instance, 0, len(configs))
	for _, config := range configs {
		instance, err := StartInstance(config, prepare)
		if err != nil {
			CloseInstances(instances)
			return nil, err
		}
		instances = append(instances, instance)
	}
	return instances, nil
}

func CloseInstances(instances []*core.Instance) {
	for _, instance := range instances {
		_ = instance.Close()
	}
}

// RestartInstance starts a fresh instance from config in place of running and returns whichever one serves afterwards.
// Where listeners share ports the new one binds next to the old, which closes only once its replacement is up, so no
// dial meets a closed port and a failed start leaves the old one serving; elsewhere the old one frees its ports first.
func RestartInstance(running *core.Instance, config string, prepare func(*core.Instance) error) (*core.Instance, error) {
	next, err := CreateXrayInstance(config)
	if err != nil {
		return running, err
	}
	if prepare != nil {
		if err = prepare(next); err != nil {
			_ = next.Close()
			return running, err
		}
	}
	if !listenersSharePorts && running != nil {
		_ = running.Close()
		running = nil
	}
	if err = next.Start(); err != nil {
		_ = next.Close()
		return running, err
	}
	if running != nil {
		_ = running.Close()
	}
	return next, nil
}

// Stops short of core.New on purpose: bulk validation runs concurrently while a live Xray instance is up.
func CheckXrayConfig(config string) error {
	r := bytes.NewReader([]byte(config))
	conf, err := serial.DecodeJSONConfig(r)
	if err != nil {
		return err
	}
	_, err = conf.Build()
	return err
}
