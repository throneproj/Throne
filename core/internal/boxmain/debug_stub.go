//go:build debug && !linux

package boxmain

func rusageMaxRSS() float64 {
	return -1
}
