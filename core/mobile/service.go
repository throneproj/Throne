package mobile

import (
	"context"
	"net/netip"
	"os"
	"sync"
	"syscall"

	"github.com/sagernet/sing-box/adapter"
	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/option"
	tun "github.com/sagernet/sing-tun"
	"github.com/sagernet/sing/common"
	"github.com/sagernet/sing/common/control"
	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/logger"
)

var _ adapter.PlatformInterface = (*platformInterfaceWrapper)(nil)

// One per box, never shared: Initialize stores that box's NetworkManager, which the interface
// monitor refreshes on every default-interface change, so a wrapper shared with a probe box would
// refresh the wrong box.
type platformInterfaceWrapper struct {
	iif                    PlatformInterface
	useProcFS              bool
	networkManager         adapter.NetworkManager
	myTunName              string
	myTunAddress           []netip.Addr
	defaultInterfaceAccess sync.Mutex
	defaultInterface       *control.Interface
	isExpensive            bool
	isConstrained          bool
}

func newPlatformInterfaceWrapper(iif PlatformInterface) *platformInterfaceWrapper {
	return &platformInterfaceWrapper{iif: iif, useProcFS: iif.UseProcFS()}
}

func (w *platformInterfaceWrapper) Initialize(networkManager adapter.NetworkManager) error {
	w.networkManager = networkManager
	return nil
}

func (w *platformInterfaceWrapper) UsePlatformAutoDetectInterfaceControl() bool {
	return w.iif.UsePlatformAutoDetectInterfaceControl()
}

func (w *platformInterfaceWrapper) AutoDetectInterfaceControl(fd int) error {
	return w.iif.AutoDetectInterfaceControl(int32(fd))
}

func (w *platformInterfaceWrapper) UsePlatformInterface() bool {
	return true
}

func (w *platformInterfaceWrapper) OpenInterface(options *tun.Options, platformOptions option.TunPlatformOptions) (tun.Tun, error) {
	if len(options.IncludeUID) > 0 || len(options.ExcludeUID) > 0 {
		return nil, E.New("platform: unsupported uid options")
	}
	if len(options.IncludeAndroidUser) > 0 {
		return nil, E.New("platform: unsupported android_user option")
	}
	routeRanges, err := options.BuildAutoRouteRanges(true)
	if err != nil {
		return nil, err
	}
	tunFd, err := w.iif.OpenTun(&tunOptions{options, routeRanges, platformOptions})
	if err != nil {
		return nil, err
	}
	options.Name, err = getTunnelName(tunFd)
	if err != nil {
		return nil, E.Cause(err, "query tun name")
	}
	options.InterfaceMonitor.RegisterMyInterface(options.Name)
	dupFd, err := dup(int(tunFd))
	if err != nil {
		return nil, E.Cause(err, "dup tun file descriptor")
	}
	options.FileDescriptor = dupFd
	w.myTunName = options.Name
	w.myTunAddress = myTunAddress(options)
	return tun.New(*options)
}

func (w *platformInterfaceWrapper) ProcessPlatformOptions(options option.TunPlatformOptions) error {
	return nil
}

func myTunAddress(options *tun.Options) []netip.Addr {
	addresses := make([]netip.Addr, 0, len(options.Inet4Address)+len(options.Inet6Address))
	for _, prefix := range options.Inet4Address {
		addresses = append(addresses, prefix.Addr())
	}
	for _, prefix := range options.Inet6Address {
		addresses = append(addresses, prefix.Addr())
	}
	return addresses
}

func (w *platformInterfaceWrapper) MyInterfaceAddress() []netip.Addr {
	return w.myTunAddress
}

func (w *platformInterfaceWrapper) UsePlatformDefaultInterfaceMonitor() bool {
	return true
}

func (w *platformInterfaceWrapper) CreateDefaultInterfaceMonitor(logger logger.Logger) tun.DefaultInterfaceMonitor {
	return &platformDefaultInterfaceMonitor{
		platformInterfaceWrapper: w,
		logger:                   logger,
	}
}

func (w *platformInterfaceWrapper) UsePlatformNetworkInterfaces() bool {
	return true
}

func (w *platformInterfaceWrapper) NetworkInterfaces() ([]adapter.NetworkInterface, error) {
	interfaceIterator, err := w.iif.GetInterfaces()
	if err != nil {
		return nil, err
	}
	var interfaces []adapter.NetworkInterface
	for _, netInterface := range iteratorToArray[*NetworkInterface](interfaceIterator) {
		w.defaultInterfaceAccess.Lock()
		isDefault := netInterface.Name != w.myTunName && w.defaultInterface != nil && int(netInterface.Index) == w.defaultInterface.Index
		w.defaultInterfaceAccess.Unlock()
		interfaces = append(interfaces, adapter.NetworkInterface{
			Interface: control.Interface{
				Index:     int(netInterface.Index),
				MTU:       int(netInterface.MTU),
				Name:      netInterface.Name,
				Addresses: common.Map(iteratorToArray[string](netInterface.Addresses), netip.MustParsePrefix),
				Flags:     linkFlags(uint32(netInterface.Flags)),
			},
			Type:       C.InterfaceType(netInterface.Type),
			DNSServers: iteratorToArray[string](netInterface.DNSServer),
			Gateways: common.Filter(common.Map(iteratorToArray[string](netInterface.Gateway), func(it string) netip.Addr {
				gateway, _ := netip.ParseAddr(it)
				return gateway.Unmap().WithZone("")
			}), netip.Addr.IsValid),
			Expensive:   netInterface.Metered || isDefault && w.isExpensive,
			Constrained: isDefault && w.isConstrained,
		})
	}
	interfaces = common.UniqBy(interfaces, func(it adapter.NetworkInterface) string {
		return it.Name
	})
	return interfaces, nil
}

func (w *platformInterfaceWrapper) UnderNetworkExtension() bool {
	return false
}

func (w *platformInterfaceWrapper) NetworkExtensionIncludeAllNetworks() bool {
	return false
}

func (w *platformInterfaceWrapper) ClearDNSCache() {
	w.iif.ClearDNSCache()
}

func (w *platformInterfaceWrapper) RequestPermissionForWIFIState() error {
	return nil
}

func (w *platformInterfaceWrapper) UsePlatformWIFIMonitor() bool {
	return true
}

func (w *platformInterfaceWrapper) ReadWIFIState(ctx context.Context) adapter.WIFIState {
	wifiState := w.iif.ReadWIFIState()
	if wifiState == nil {
		return adapter.WIFIState{}
	}
	return adapter.WIFIState(*wifiState)
}

func (w *platformInterfaceWrapper) UsePlatformConnectionOwnerFinder() bool {
	return true
}

func (w *platformInterfaceWrapper) FindConnectionOwner(request *adapter.FindConnectionOwnerRequest) (*adapter.ConnectionOwner, error) {
	if w.useProcFS {
		sourceAddr, _ := netip.ParseAddr(request.SourceAddress)
		source := netip.AddrPortFrom(sourceAddr, uint16(request.SourcePort))
		destAddr, _ := netip.ParseAddr(request.DestinationAddress)
		destination := netip.AddrPortFrom(destAddr, uint16(request.DestinationPort))

		var network string
		switch request.IpProtocol {
		case int32(syscall.IPPROTO_TCP):
			network = "tcp"
		case int32(syscall.IPPROTO_UDP):
			network = "udp"
		default:
			return nil, E.New("unknown protocol: ", request.IpProtocol)
		}

		uid := resolveSocketByProcSearch(network, source, destination)
		if uid == -1 {
			return nil, E.New("procfs: not found")
		}
		// procfs knows only the uid; without its packages, package_name rules could never match here.
		packageNames, _ := w.iif.PackageNamesByUid(uid)
		return &adapter.ConnectionOwner{
			UserId:              uid,
			AndroidPackageNames: iteratorToArray[string](packageNames),
		}, nil
	}

	result, err := w.iif.FindConnectionOwner(request.IpProtocol, request.SourceAddress, request.SourcePort, request.DestinationAddress, request.DestinationPort)
	if err != nil {
		return nil, err
	}
	return &adapter.ConnectionOwner{
		UserId:              result.UserId,
		UserName:            result.UserName,
		ProcessPath:         result.ProcessPath,
		AndroidPackageNames: result.androidPackageNames,
	}, nil
}

func (w *platformInterfaceWrapper) UsePlatformNotification() bool {
	return true
}

func (w *platformInterfaceWrapper) SendNotification(notification *adapter.Notification) error {
	return w.iif.SendNotification((*Notification)(notification))
}

func (w *platformInterfaceWrapper) CancelNotification(identifier string, typeID int32) error {
	return w.iif.CancelNotification(identifier, typeID)
}

func (w *platformInterfaceWrapper) UsePlatformNeighborResolver() bool {
	return false
}

func (w *platformInterfaceWrapper) StartNeighborMonitor(listener adapter.NeighborUpdateListener) error {
	return os.ErrInvalid
}

func (w *platformInterfaceWrapper) CloseNeighborMonitor(listener adapter.NeighborUpdateListener) error {
	return nil
}

func (w *platformInterfaceWrapper) UsePlatformShell() bool {
	return false
}

func (w *platformInterfaceWrapper) CheckPlatformShell() error {
	return nil
}

func (w *platformInterfaceWrapper) OpenShellSession(user *adapter.PlatformUser, command string, env []string, term string, rows int32, cols int32) (adapter.ShellSession, error) {
	return nil, os.ErrInvalid
}

func (w *platformInterfaceWrapper) LookupUser(username string) (*adapter.PlatformUser, error) {
	return nil, os.ErrInvalid
}

func (w *platformInterfaceWrapper) LookupSFTPServer() (string, error) {
	return "", os.ErrInvalid
}

func (w *platformInterfaceWrapper) ReadSystemSSHHostKey() ([]byte, error) {
	return nil, os.ErrInvalid
}

func (w *platformInterfaceWrapper) TailscaleHostname() string {
	return ""
}

func (w *platformInterfaceWrapper) UsePlatformBridge() bool {
	return false
}

func (w *platformInterfaceWrapper) CreateBridge(options adapter.BridgeOptions) (adapter.BridgeSession, error) {
	return nil, os.ErrInvalid
}
