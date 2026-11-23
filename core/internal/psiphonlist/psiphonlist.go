// Package psiphonlist carries the Psiphon server list built into the core, so a first connection needs no remote list.
// CI downloads the current list over the empty placeholder before building; a local build embeds nothing.
package psiphonlist

import (
	_ "embed"
	"slices"
)

//go:embed server_list_compressed
var serverList []byte

// ServerList returns a copy of the embedded list in Psiphon's signed compressed form, empty when none was embedded.
func ServerList() []byte {
	return slices.Clone(serverList)
}
