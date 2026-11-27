package probe

import (
	"fmt"
	"time"
)

const (
	Gb = 1000 * Mb
	Mb = 1000 * Kb
	Kb = 1000
)

func BrateToStr(brate float64) string {
	// speedtest-go reports -1 when a direction could not be measured
	if brate < 0 {
		return "N/A"
	}
	brate *= 8
	if brate >= Gb {
		return fmt.Sprintf("%.2f%s", brate/Gb, "Gbps")
	}
	if brate >= Mb {
		return fmt.Sprintf("%.2f%s", brate/Mb, "Mbps")
	}
	return fmt.Sprintf("%.2f%s", brate/Kb, "Kbps")
}

func CalculateBRate(bytes float64, startTime time.Time) float64 {
	elapsed := time.Since(startTime).Seconds()
	return bytes / elapsed
}
