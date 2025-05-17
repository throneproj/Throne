package main

import (
	"context"
	"nekobox_core/gen"
	"nekobox_core/internal/boxdns"
)

func (s *server) SetSystemDNS(ctx context.Context, in *gen.SetSystemDNSRequest) (*gen.EmptyResp, error) {
	err := boxdns.DnsManagerInstance.SetSystemDNS(nil, in.Clear)
	if err != nil {
		return nil, err
	}

	return &gen.EmptyResp{}, nil
}
