package mobile

import (
	"context"
	"net/netip"
	"os"

	"ThroneCore/internal/xray"

	box "github.com/sagernet/sing-box"
	"github.com/sagernet/sing-box/adapter"
	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/dns"
	"github.com/sagernet/sing-box/experimental/deprecated"
	"github.com/sagernet/sing-box/include"
	"github.com/sagernet/sing-box/log"
	"github.com/sagernet/sing-box/option"
	tun "github.com/sagernet/sing-tun"
	"github.com/sagernet/sing/common/control"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/json"
	"github.com/sagernet/sing/common/logger"
	"github.com/sagernet/sing/common/x/list"
	"github.com/sagernet/sing/service"
	"github.com/sagernet/sing/service/filemanager"
)

// Fresh per box, like boxmain.newBoxContext: boxes sharing one service registry overwrite each
// other's managers. The platform `local` DNS transport is installed through the registry so the
// fork's root box.New reaches it for its implicit fallback transport too, as libbox does.
func newBoxContext(platform PlatformInterface, platformInterface adapter.PlatformInterface) context.Context {
	dnsRegistry := include.DNSTransportRegistry()
	if platform != nil {
		if localTransport := platform.LocalDNSTransport(); localTransport != nil {
			dns.RegisterTransport[option.LocalDNSServerOptions](dnsRegistry, C.DNSTypeLocal, func(ctx context.Context, logger log.ContextLogger, tag string, options option.LocalDNSServerOptions) (adapter.DNSTransport, error) {
				return newPlatformTransport(ctx, logger, localTransport, tag, options)
			})
		}
	}
	ctx := context.Background()
	ctx = filemanager.WithDefault(ctx, sWorkingPath, sTempPath, os.Getuid(), os.Getgid())
	ctx = box.Context(ctx, include.InboundRegistry(), include.OutboundRegistry(), include.EndpointRegistry(), dnsRegistry, include.ServiceRegistry(), include.CertificateProviderRegistry())
	ctx = service.ContextWith[deprecated.Manager](ctx, deprecated.NewStderrManager(log.StdLogger()))
	if platformInterface != nil {
		ctx = service.ContextWith[adapter.PlatformInterface](ctx, platformInterface)
	}
	return ctx
}

func parseConfig(ctx context.Context, configContent string) (option.Options, error) {
	options, err := json.UnmarshalExtendedContext[option.Options](ctx, []byte(configContent))
	if err != nil {
		return option.Options{}, E.Cause(err, "decode config")
	}
	return options, nil
}

// Recovers like rpc.CheckConfig: box.New can panic on malformed configs, and a panic inside a
// gomobile call takes the whole process down.
func CheckConfig(coreConfig string) (err error) {
	defer func() {
		if r := recover(); r != nil {
			err = E.New("check config panic: ", r)
		}
	}()
	ctx := newBoxContext(nil, (*platformInterfaceStub)(nil))
	options, err := parseConfig(ctx, coreConfig)
	if err != nil {
		return err
	}
	ctx, cancel := context.WithCancel(ctx)
	defer cancel()
	instance, err := box.New(box.Options{
		Context: ctx,
		Options: options,
	})
	if err == nil {
		instance.Close()
	}
	return err
}

func CheckXrayConfig(xrayConfig string) error {
	return xray.CheckXrayConfig(xrayConfig)
}

type platformInterfaceStub struct{}

func (s *platformInterfaceStub) Initialize(networkManager adapter.NetworkManager) error {
	return nil
}

func (s *platformInterfaceStub) UsePlatformAutoDetectInterfaceControl() bool {
	return true
}

func (s *platformInterfaceStub) AutoDetectInterfaceControl(fd int) error {
	return nil
}

func (s *platformInterfaceStub) UsePlatformInterface() bool {
	return false
}

func (s *platformInterfaceStub) OpenInterface(options *tun.Options, platformOptions option.TunPlatformOptions) (tun.Tun, error) {
	return nil, os.ErrInvalid
}

func (s *platformInterfaceStub) ProcessPlatformOptions(options option.TunPlatformOptions) error {
	return nil
}

func (s *platformInterfaceStub) UsePlatformDefaultInterfaceMonitor() bool {
	return true
}

func (s *platformInterfaceStub) CreateDefaultInterfaceMonitor(logger logger.Logger) tun.DefaultInterfaceMonitor {
	return (*interfaceMonitorStub)(nil)
}

func (s *platformInterfaceStub) UsePlatformNetworkInterfaces() bool {
	return false
}

func (s *platformInterfaceStub) NetworkInterfaces() ([]adapter.NetworkInterface, error) {
	return nil, os.ErrInvalid
}

func (s *platformInterfaceStub) UnderNetworkExtension() bool {
	return false
}

func (s *platformInterfaceStub) NetworkExtensionIncludeAllNetworks() bool {
	return false
}

func (s *platformInterfaceStub) ClearDNSCache() {
}

func (s *platformInterfaceStub) RequestPermissionForWIFIState() error {
	return nil
}

func (s *platformInterfaceStub) UsePlatformWIFIMonitor() bool {
	return false
}

func (s *platformInterfaceStub) ReadWIFIState(ctx context.Context) adapter.WIFIState {
	return adapter.WIFIState{}
}

func (s *platformInterfaceStub) UsePlatformConnectionOwnerFinder() bool {
	return false
}

func (s *platformInterfaceStub) FindConnectionOwner(request *adapter.FindConnectionOwnerRequest) (*adapter.ConnectionOwner, error) {
	return nil, os.ErrInvalid
}

func (s *platformInterfaceStub) UsePlatformNotification() bool {
	return false
}

func (s *platformInterfaceStub) SendNotification(notification *adapter.Notification) error {
	return nil
}

func (s *platformInterfaceStub) CancelNotification(identifier string, typeID int32) error {
	return nil
}

func (s *platformInterfaceStub) MyInterfaceAddress() []netip.Addr {
	return nil
}

func (s *platformInterfaceStub) UsePlatformNeighborResolver() bool {
	return false
}

func (s *platformInterfaceStub) StartNeighborMonitor(listener adapter.NeighborUpdateListener) error {
	return os.ErrInvalid
}

func (s *platformInterfaceStub) CloseNeighborMonitor(listener adapter.NeighborUpdateListener) error {
	return nil
}

func (s *platformInterfaceStub) UsePlatformShell() bool {
	return false
}

func (s *platformInterfaceStub) CheckPlatformShell() error {
	return nil
}

func (s *platformInterfaceStub) OpenShellSession(user *adapter.PlatformUser, command string, env []string, term string, rows int32, cols int32) (adapter.ShellSession, error) {
	return nil, os.ErrInvalid
}

func (s *platformInterfaceStub) LookupSFTPServer() (string, error) {
	return "", os.ErrInvalid
}

func (s *platformInterfaceStub) ReadSystemSSHHostKey() ([]byte, error) {
	return nil, os.ErrInvalid
}

func (s *platformInterfaceStub) TailscaleHostname() string {
	return ""
}

func (s *platformInterfaceStub) UsePlatformBridge() bool {
	return false
}

func (s *platformInterfaceStub) CreateBridge(options adapter.BridgeOptions) (adapter.BridgeSession, error) {
	return nil, os.ErrInvalid
}

func (s *platformInterfaceStub) LookupUser(username string) (*adapter.PlatformUser, error) {
	return nil, os.ErrInvalid
}

type interfaceMonitorStub struct{}

func (s *interfaceMonitorStub) Start() error {
	return os.ErrInvalid
}

func (s *interfaceMonitorStub) Close() error {
	return os.ErrInvalid
}

func (s *interfaceMonitorStub) DefaultInterface() *control.Interface {
	return nil
}

func (s *interfaceMonitorStub) OverrideAndroidVPN() bool {
	return false
}

func (s *interfaceMonitorStub) AndroidVPNEnabled() bool {
	return false
}

func (s *interfaceMonitorStub) RegisterCallback(callback tun.DefaultInterfaceUpdateCallback) *list.Element[tun.DefaultInterfaceUpdateCallback] {
	return nil
}

func (s *interfaceMonitorStub) UnregisterCallback(element *list.Element[tun.DefaultInterfaceUpdateCallback]) {
}

func (s *interfaceMonitorStub) RegisterMyInterface(interfaceName string) {
}

func (s *interfaceMonitorStub) MyInterfaces() []string {
	return nil
}
