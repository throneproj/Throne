package internal

import "fmt"

const (
	Gb = 1000 * Mb
	Mb = 1000 * Kb
	Kb = 1000
)

func BrateToStr(brate float64) string {
	brate *= 8
	if brate >= Gb {
		return fmt.Sprintf("%.2f%s", brate/Gb, "Gbps")
	}
	if brate >= Mb {
		return fmt.Sprintf("%.2f%s", brate/Mb, "Mbps")
	}
	return fmt.Sprintf("%.2f%s", brate/Kb, "Kbps")
}
