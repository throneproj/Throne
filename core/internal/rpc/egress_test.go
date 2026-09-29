package rpc

import (
	"runtime"
	"testing"

	tun "github.com/sagernet/sing-tun"
)

func TestConfigAutoRedirectMark(t *testing.T) {
	for _, tc := range []struct {
		name   string
		config string
		want   uint32
	}{
		{
			name:   "no inbounds at all",
			config: `{"outbounds":[{"type":"direct","tag":"direct"}]}`,
			want:   0,
		},
		{
			name:   "tun without auto_redirect",
			config: `{"inbounds":[{"type":"tun","tag":"tun-in","auto_route":true}]}`,
			want:   0,
		},
		{
			name:   "system proxy only",
			config: `{"inbounds":[{"type":"mixed","tag":"mixed-in","listen_port":2080}]}`,
			want:   0,
		},
		{
			name:   "tun with auto_redirect takes sing-tun's default",
			config: `{"inbounds":[{"type":"tun","tag":"tun-in","auto_route":true,"auto_redirect":true}]}`,
			want:   tun.DefaultAutoRedirectOutputMark,
		},
		{
			name:   "explicit output mark wins over the default",
			config: `{"inbounds":[{"type":"tun","auto_redirect":true,"auto_redirect_output_mark":4660}]}`,
			want:   4660,
		},
		{
			// option.FwMark marshals as a hex string, so a round-tripped config comes back in that form.
			name:   "hex string output mark",
			config: `{"inbounds":[{"type":"tun","auto_redirect":true,"auto_redirect_output_mark":"0x1234"}]}`,
			want:   4660,
		},
		{
			name:   "tun found behind other inbounds",
			config: `{"inbounds":[{"type":"direct","tag":"dns-in"},{"type":"mixed"},{"type":"tun","auto_redirect":true}]}`,
			want:   tun.DefaultAutoRedirectOutputMark,
		},
		{
			name:   "unexpected inbound shape is skipped, not fatal",
			config: `{"inbounds":[{"type":"tun","auto_redirect":["nonsense"]},{"type":"tun","auto_redirect":true}]}`,
			want:   tun.DefaultAutoRedirectOutputMark,
		},
		{
			name:   "malformed json",
			config: `{"inbounds":[`,
			want:   0,
		},
	} {
		t.Run(tc.name, func(t *testing.T) {
			if got := configAutoRedirectMark([]byte(tc.config)); got != tc.want {
				t.Errorf("configAutoRedirectMark() = %d, want %d", got, tc.want)
			}
		})
	}
}

func TestAutoRedirectMarkForIsLinuxOnly(t *testing.T) {
	config := []byte(`{"inbounds":[{"type":"tun","auto_redirect":true}]}`)
	want := uint32(0)
	if runtime.GOOS == "linux" {
		want = tun.DefaultAutoRedirectOutputMark
	}
	if got := autoRedirectMarkFor(config); got != want {
		t.Errorf("autoRedirectMarkFor() on %s = %d, want %d", runtime.GOOS, got, want)
	}
}
