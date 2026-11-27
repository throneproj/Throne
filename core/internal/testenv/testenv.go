// Package testenv builds what a probe batch runs against: the running box, or a throwaway box plus the Xray instances it dials through.
package testenv

import (
	"context"

	"ThroneCore/internal/probe"
	"ThroneCore/internal/xray"
)

type Env[B probe.Box] struct {
	Box   B
	Tags  []string
	close func()
}

func (e *Env[B]) Close() {
	e.close()
}

// Measures the running box, through "proxy" when it has one, and owns nothing.
func Current[B probe.Box](box B, tags []string, useDefaultOutbound bool) *Env[B] {
	outTags := tags
	if _, exists := box.Outbound().Outbound("proxy"); exists {
		outTags = []string{"proxy"}
	} else {
		useDefaultOutbound = true
	}
	if useDefaultOutbound {
		outTags = []string{box.Outbound().Default().Tag()}
	}
	return &Env[B]{Box: box, Tags: outTags, close: func() {}}
}

type Request struct {
	NeedXray           bool
	XrayConfig         string
	XrayFullConfigs    []string
	XrayDNSStrategy    string
	Tags               []string
	UseDefaultOutbound bool
}

// The Xray instances start first and resolve through build's box, so build must publish its context between New and Start.
func Prepare[B probe.Box](parent context.Context, request Request, egress func() (string, uint32),
	build func(publish func(context.Context)) (B, func(), error)) (*Env[B], error) {
	if err := parent.Err(); err != nil {
		return nil, err
	}
	var boxCtx xray.BoxContext
	prepare := xray.Preparer(egress, request.XrayDNSStrategy, boxCtx.Get)

	configs := request.XrayFullConfigs
	if request.NeedXray {
		configs = append([]string{request.XrayConfig}, configs...)
	}
	ctx, cancel := context.WithCancel(parent)
	instances, err := xray.StartInstances(ctx, configs, prepare)
	if err != nil {
		cancel()
		return nil, err
	}
	var closeBox func()
	var cancelDNS context.CancelFunc
	closeEnv := func() {
		cancel()
		if cancelDNS != nil {
			cancelDNS()
		}
		xray.CloseInstances(instances)
		if closeBox != nil {
			closeBox()
		}
	}

	box, closeBox, err := build(func(box context.Context) {
		// LookupIP has no request context; retire in-flight resolver calls with the sidecars.
		var dnsCtx context.Context
		dnsCtx, cancelDNS = context.WithCancel(box)
		context.AfterFunc(ctx, cancelDNS)
		boxCtx.Publish(dnsCtx)
	})
	if err == nil {
		err = ctx.Err()
	}
	if err != nil {
		closeEnv()
		return nil, err
	}

	outTags := request.Tags
	if request.UseDefaultOutbound {
		outTags = []string{box.Outbound().Default().Tag()}
	}
	return &Env[B]{Box: box, Tags: outTags, close: closeEnv}, nil
}
