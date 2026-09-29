package rpc

import (
	"context"
	"time"

	"ThroneCore/gen"
	"ThroneCore/internal/warp"
)

const (
	warpRegisterTimeout     = 10 * time.Second
	warpRegisterHostTimeout = 10 * time.Second
)

func (s *server) WarpRegister(ctx context.Context, in *gen.WarpRegisterRequest) (*gen.WarpRegisterResponse, error) {
	hosts := len(in.GetApiHosts())
	if hosts == 0 {
		hosts = 1
	}
	ctx, cancel := context.WithTimeout(ctx, warpRegisterTimeout+time.Duration(hosts)*warpRegisterHostTimeout)
	defer cancel()
	identity, err := warp.Register(ctx, in.GetTunnelType(), in.GetProxy(), in.GetApiHosts())
	if err != nil {
		return &gen.WarpRegisterResponse{Error: To(err.Error())}, nil
	}
	reserved := make([]int32, 0, len(identity.Reserved))
	for _, b := range identity.Reserved {
		reserved = append(reserved, int32(b))
	}
	return &gen.WarpRegisterResponse{
		DeviceId:      To(identity.DeviceID),
		Token:         To(identity.Token),
		License:       To(identity.License),
		Ipv4:          To(identity.IPv4),
		Ipv6:          To(identity.IPv6),
		PrivateKey:    To(identity.PrivateKey),
		PeerPublicKey: To(identity.PeerPublicKey),
		Endpoint:      To(identity.Endpoint),
		Reserved:      reserved,
	}, nil
}

func (s *server) GenWgKeyPair(ctx context.Context, _ *gen.EmptyReq) (out *gen.GenWgKeyPairResponse, _ error) {
	var res gen.GenWgKeyPairResponse
	privateKey, err := warp.GeneratePrivateKey()
	if err != nil {
		res.Error = To(err.Error())
		return &res, nil
	}
	res.PrivateKey = To(privateKey.String())
	res.PublicKey = To(privateKey.PublicKey().String())
	return &res, nil
}
