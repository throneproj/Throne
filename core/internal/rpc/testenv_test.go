package rpc

import (
	"reflect"
	"testing"

	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/option"
	tun "github.com/sagernet/sing-tun"
)

func TestApplyAutoRedirectMark(t *testing.T) {
	const mark = tun.DefaultAutoRedirectOutputMark
	for _, tc := range []struct {
		name    string
		options option.Options
		mark    uint32
		want    *option.RouteOptions
	}{
		{
			name:    "no running auto_redirect",
			options: option.Options{Route: &option.RouteOptions{AutoDetectInterface: true}},
			mark:    0,
			want:    &option.RouteOptions{AutoDetectInterface: true},
		},
		{
			name:    "probe route keeps its settings",
			options: option.Options{Route: &option.RouteOptions{AutoDetectInterface: true}},
			mark:    mark,
			want:    &option.RouteOptions{AutoDetectInterface: true, DefaultMark: mark},
		},
		{
			name:    "missing route section",
			options: option.Options{},
			mark:    mark,
			want:    &option.RouteOptions{DefaultMark: mark},
		},
		{
			name:    "explicit default_mark wins",
			options: option.Options{Route: &option.RouteOptions{DefaultMark: 0x1234}},
			mark:    mark,
			want:    &option.RouteOptions{DefaultMark: 0x1234},
		},
		{
			name: "socks bridge inbounds don't block it",
			options: option.Options{
				Inbounds: []option.Inbound{{Type: C.TypeSOCKS, Tag: "bridge-2080"}},
				Route:    &option.RouteOptions{AutoDetectInterface: true},
			},
			mark: mark,
			want: &option.RouteOptions{AutoDetectInterface: true, DefaultMark: mark},
		},
		{
			name: "own tun inbound",
			options: option.Options{
				Inbounds: []option.Inbound{{Type: C.TypeSOCKS}, {Type: C.TypeTun, Tag: "tun-in"}},
			},
			mark: mark,
			want: nil,
		},
	} {
		t.Run(tc.name, func(t *testing.T) {
			applyAutoRedirectMark(&tc.options, tc.mark)
			if !reflect.DeepEqual(tc.options.Route, tc.want) {
				t.Errorf("route = %+v, want %+v", tc.options.Route, tc.want)
			}
		})
	}
}
