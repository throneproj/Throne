package xray

import (
	"bytes"
	"context"
	"github.com/xtls/xray-core/core"
	"github.com/xtls/xray-core/infra/conf/serial"
)

func CreateXrayInstance(config string) (*core.Instance, error) {
	b, err := parseConfig(config)
	if err != nil {
		return nil, err
	}
	return core.New(b)
}

func parseConfig(config string) (*core.Config, error) {
	conf, err := serial.DecodeJSONConfig(bytes.NewReader([]byte(config)))
	if err != nil {
		return nil, err
	}
	return conf.Build()
}

// prepare runs between core.New and Start; an instance that fails either is closed.
func StartInstance(config string, prepare func(*core.Instance) error) (*core.Instance, error) {
	return RestartInstance(nil, config, prepare)
}

func newProbe(ctx context.Context, config string) (*core.Instance, error) {
	b, err := parseConfig(config)
	if err != nil {
		return nil, err
	}
	return core.NewProbe(ctx, b)
}

// On failure the ones already started are closed; on success the caller must close them.
func StartInstances(ctx context.Context, configs []string, prepare func(*core.Instance) error) ([]*core.Instance, error) {
	instances := make([]*core.Instance, 0, len(configs))
	for _, config := range configs {
		if err := ctx.Err(); err != nil {
			CloseInstances(instances)
			return nil, err
		}
		instance, err := newProbe(ctx, config)
		if err == nil {
			instances = append(instances, instance)
			if err = prepare(instance); err == nil {
				err = instance.Start()
			}
		}
		if err != nil {
			CloseInstances(instances)
			return nil, err
		}
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
	_, err := parseConfig(config)
	return err
}
