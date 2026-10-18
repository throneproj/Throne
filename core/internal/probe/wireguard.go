package probe

import "github.com/sagernet/sing-box/option"

// Each endpoint otherwise starts three crypto goroutines per CPU, which a test box of a hundred profiles turns into thousands.
const testWireGuardWorkers = 2

func LimitWireGuardWorkers(options *option.Options) {
	for _, endpoint := range options.Endpoints {
		wireGuardOptions, isWireGuard := endpoint.Options.(*option.WireGuardEndpointOptions)
		if isWireGuard && wireGuardOptions.Workers == 0 {
			wireGuardOptions.Workers = testWireGuardWorkers
		}
	}
}
