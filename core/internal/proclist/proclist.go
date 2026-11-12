package proclist

type Entry struct {
	PID     uint32
	Name    string
	Path    string
	Service string
}
