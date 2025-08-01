package main

import (
	"Core/gen"
	"Core/internal/boxdns"
)

func (s *server) SetSystemDNS(in *gen.SetSystemDNSRequest, out *gen.EmptyResp) error {
	err := boxdns.DnsManagerInstance.SetSystemDNS(nil, *in.Clear)
	if err != nil {
		return err
	}

	return nil
}
