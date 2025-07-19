package process

import "log"

type pipeLogger struct {
	prefix string
	noOut  bool
}

func (p *pipeLogger) Write(b []byte) (int, error) {
	if !p.noOut {
		log.Println(p.prefix + ":" + string(b))
	}
	return len(b), nil
}
