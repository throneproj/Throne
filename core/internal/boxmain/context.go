package boxmain

import (
	"context"
	"os"
	"os/user"
	"strconv"

	box "github.com/sagernet/sing-box"
	"github.com/sagernet/sing-box/experimental/deprecated"
	"github.com/sagernet/sing-box/include"
	"github.com/sagernet/sing-box/log"
	"github.com/sagernet/sing/service"
	"github.com/sagernet/sing/service/filemanager"
)

var disableColor bool

// Must stay per-box: boxes sharing one service.Registry each overwrite the previous box's MustRegister[OutboundManager].
func newBoxContext() context.Context {
	ctx := context.Background()
	sudoUser := os.Getenv("SUDO_USER")
	ownerUID, _ := strconv.Atoi(os.Getenv("SUDO_UID"))
	ownerGID, _ := strconv.Atoi(os.Getenv("SUDO_GID"))
	if ownerUID == 0 && ownerGID == 0 && sudoUser != "" {
		sudoUserObject, _ := user.Lookup(sudoUser)
		if sudoUserObject != nil {
			ownerUID, _ = strconv.Atoi(sudoUserObject.Uid)
			ownerGID, _ = strconv.Atoi(sudoUserObject.Gid)
		}
	}
	// A setuid launch carries no SUDO_*, but the real ids are still the invoking user's; -1 on Windows falls through.
	if ownerUID <= 0 || ownerGID <= 0 {
		ownerUID, ownerGID = os.Getuid(), os.Getgid()
	}
	if ownerUID > 0 && ownerGID > 0 {
		ctx = filemanager.WithDefault(ctx, "", "", ownerUID, ownerGID)
	}
	ctx = service.ContextWith(ctx, deprecated.NewStderrManager(log.StdLogger()))
	ctx = box.Context(ctx, include.InboundRegistry(), include.OutboundRegistry(), include.EndpointRegistry(), include.DNSTransportRegistry(), include.ServiceRegistry(), include.CertificateProviderRegistry())
	return ctx
}
