package probe

import (
	"context"
	"testing"

	"github.com/sagernet/sing-box/include"
	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing/common/json"
)

func TestLimitWireGuardWorkers(t *testing.T) {
	options, err := json.UnmarshalExtendedContext[option.Options](include.Context(context.Background()), []byte(`{"endpoints": [
		{"type": "wireguard", "tag": "default"},
		{"type": "wireguard", "tag": "explicit", "workers": 4}
	]}`))
	if err != nil {
		t.Fatal(err)
	}
	LimitWireGuardWorkers(&options)
	want := map[string]int{"default": testWireGuardWorkers, "explicit": 4}
	for _, endpoint := range options.Endpoints {
		wireGuardOptions, isWireGuard := endpoint.Options.(*option.WireGuardEndpointOptions)
		if !isWireGuard {
			t.Fatalf("%s: options are %T", endpoint.Tag, endpoint.Options)
		}
		if wireGuardOptions.Workers != want[endpoint.Tag] {
			t.Errorf("%s: workers = %d, want %d", endpoint.Tag, wireGuardOptions.Workers, want[endpoint.Tag])
		}
	}
}
