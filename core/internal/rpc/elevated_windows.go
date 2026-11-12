package rpc

import "ThroneCore/internal/elevation"

func isElevated() bool {
	return elevation.Elevated()
}
