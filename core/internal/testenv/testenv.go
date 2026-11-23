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
func Prepare[B probe.Box](request Request, egress func() (string, uint32),
	build func(publish func(context.Context)) (B, func(), error)) (*Env[B], error) {
	var boxCtx xray.BoxContext
	prepare := xray.Preparer(egress, request.XrayDNSStrategy, boxCtx.Get)

	var cleanups []func()
	unwind := func() {
		for i := len(cleanups) - 1; i >= 0; i-- {
			cleanups[i]()
		}
	}

	if request.NeedXray {
		instance, err := xray.StartInstance(request.XrayConfig, prepare)
		if err != nil {
			return nil, err
		}
		cleanups = append(cleanups, func() { _ = instance.Close() })
	}

	fullXray, err := xray.StartInstances(request.XrayFullConfigs, prepare)
	if err != nil {
		unwind()
		return nil, err
	}
	cleanups = append(cleanups, func() { xray.CloseInstances(fullXray) })

	box, closeBox, err := build(boxCtx.Publish)
	if err != nil {
		unwind()
		return nil, err
	}
	cleanups = append(cleanups, closeBox)

	outTags := request.Tags
	if request.UseDefaultOutbound {
		outTags = []string{box.Outbound().Default().Tag()}
	}
	return &Env[B]{Box: box, Tags: outTags, close: unwind}, nil
}
