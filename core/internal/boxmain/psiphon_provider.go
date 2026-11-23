//go:build with_psiphon

package boxmain

import (
	"context"

	"ThroneCore/internal/psiphonlist"

	"github.com/sagernet/sing-box/protocol/psiphon"
	"github.com/sagernet/sing/service"
)

type embeddedServerList struct{}

func (embeddedServerList) EmbeddedServerList() []byte {
	return psiphonlist.ServerList()
}

func withPsiphonServerList(ctx context.Context) context.Context {
	return service.ContextWith[psiphon.ServerListProvider](ctx, embeddedServerList{})
}
