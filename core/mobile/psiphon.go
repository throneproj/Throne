package mobile

import (
	"path/filepath"
	"regexp"

	"github.com/sagernet/sing-box/adapter"
	"github.com/sagernet/sing-box/include"
	"github.com/sagernet/sing-box/option"
	E "github.com/sagernet/sing/common/exceptions"
	N "github.com/sagernet/sing/common/network"
)

const psiphonFolder = "psiphon"

var (
	psiphonIDPattern     = regexp.MustCompile(`^[0-9A-Fa-f]{16}$`)
	psiphonRegionPattern = regexp.MustCompile(`^[A-Za-z]{2}$`)
)

// boxmain.pinPsiphon: the datastore stays in the working directory whatever the config says, and the IDs are checked.
func pinPsiphon(options *option.Options) error {
	for _, outbound := range options.Outbounds {
		psiphonOptions, isPsiphon := outbound.Options.(*option.PsiphonOutboundOptions)
		if !isPsiphon {
			continue
		}
		psiphonOptions.DataDirectory = filepath.Join(sWorkingPath, psiphonFolder)
		if id := psiphonOptions.PropagationChannelID; id != "" && !psiphonIDPattern.MatchString(id) {
			return E.New("psiphon outbound ", outbound.Tag, ": propagation_channel_id must be 16 hex digits")
		}
		if id := psiphonOptions.SponsorID; id != "" && !psiphonIDPattern.MatchString(id) {
			return E.New("psiphon outbound ", outbound.Tag, ": sponsor_id must be 16 hex digits")
		}
		if region := psiphonOptions.EgressRegion; region != "" && !psiphonRegionPattern.MatchString(region) {
			return E.New("psiphon outbound ", outbound.Tag, ": egress_region must be a two-letter country code")
		}
	}
	return nil
}

// The egress regions Psiphon last reported in this process, uppercase country codes; empty until a Psiphon outbound
// has connected once since the process started.
func PsiphonRegions() StringIterator {
	return newIterator(include.PsiphonAvailableEgressRegions())
}

// rpc.warmProxyChain: Tor and Psiphon start on their first dial, the ones the main route uses start now. Outbounds only
// rules reach, and group members not selected, stay lazy: connecting to Psiphon unasked can give a user away.
func warmProxyChain(outbounds adapter.OutboundManager) {
	if outbounds == nil {
		return
	}
	visited := make(map[string]bool)
	pending := []string{"proxy"}
	for len(pending) > 0 {
		tag := pending[len(pending)-1]
		pending = pending[:len(pending)-1]
		if visited[tag] {
			continue
		}
		visited[tag] = true
		outbound, found := outbounds.Outbound(tag)
		if !found {
			continue
		}
		if warmer, isWarmer := outbound.(interface{ Warm() }); isWarmer {
			go warmer.Warm()
		}
		group, isGroup := outbound.(adapter.OutboundGroup)
		if !isGroup {
			pending = append(pending, outbound.Dependencies()...)
			continue
		}
		// By tag: the auto selector hands out a wrapper that hides the member's own methods.
		for _, network := range []string{N.NetworkTCP, N.NetworkUDP} {
			if selected := group.Selected(network); selected != nil {
				pending = append(pending, selected.Tag())
			}
		}
	}
}
