package boxmain

import (
	"context"

	box "github.com/sagernet/sing-box"
	"github.com/sagernet/sing-box/include"
)

func Check(content []byte) error {
	ctx := context.Background()
	ctx = box.Context(ctx, include.InboundRegistry(), include.OutboundRegistry(), include.EndpointRegistry(), include.DNSTransportRegistry(), include.ServiceRegistry(), include.CertificateProviderRegistry())
	options, err := parseConfig(ctx, content)
	if err != nil {
		return err
	}
	pinTailscaleDirectories(options)
	dropDebugOptions(options)
	ctx, cancel := context.WithCancel(ctx)
	instance, err := box.New(box.Options{
		Context: ctx,
		Options: *options,
	})
	if err == nil {
		instance.Close()
	}
	cancel()
	return err
}
