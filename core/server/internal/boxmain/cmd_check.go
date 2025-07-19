package boxmain

import (
	"context"
	"github.com/sagernet/sing-box/include"

	"Core/internal/boxbox"
)

func Check(content []byte) error {
	ctx := context.Background()
	ctx = boxbox.Context(ctx, include.InboundRegistry(), include.OutboundRegistry(), include.EndpointRegistry())
	options, err := parseConfig(ctx, content)
	if err != nil {
		return err
	}
	ctx, cancel := context.WithCancel(ctx)
	instance, err := boxbox.New(boxbox.Options{
		Context: ctx,
		Options: *options,
	})
	if err == nil {
		instance.Close()
	}
	cancel()
	return err
}
