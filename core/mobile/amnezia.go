package mobile

import (
	"context"

	"ThroneCore/internal/amnezia"
)

// rpc.GenQuicSignature: Chrome's first HTTP/3 flight to domain as AmneziaWG I1.. values, each "<b 0x…>",
// at most five. Built offline, nothing is sent; an IP literal is refused.
func GenQuicSignature(domain string) (StringIterator, error) {
	ctx, cancel := context.WithTimeout(context.Background(), amnezia.QUICSignatureTimeout)
	defer cancel()
	packets, err := amnezia.QUICSignature(ctx, domain)
	if err != nil {
		return nil, err
	}
	return newIterator(packets), nil
}
