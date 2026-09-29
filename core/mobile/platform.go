package mobile

import C "github.com/sagernet/sing-box/constant"

// Shapes mirror sing-box's libbox so the Kotlin side can follow sing-box-for-android. Members
// Android never needs (shell, SSH, bridge, neighbor table, Apple network-extension flags) are not
// asked of the platform; the wrapper answers them itself.
type PlatformInterface interface {
	LocalDNSTransport() LocalDNSTransport
	UsePlatformAutoDetectInterfaceControl() bool
	AutoDetectInterfaceControl(fd int32) error
	OpenTun(options TunOptions) (int32, error)
	UseProcFS() bool
	FindConnectionOwner(ipProtocol int32, sourceAddress string, sourcePort int32, destinationAddress string, destinationPort int32) (*ConnectionOwner, error)
	// The packages of a uid that the procfs lookup found (UseProcFS); FindConnectionOwner returns them itself.
	PackageNamesByUid(uid int32) (StringIterator, error)
	StartDefaultInterfaceMonitor(listener InterfaceUpdateListener) error
	CloseDefaultInterfaceMonitor(listener InterfaceUpdateListener) error
	GetInterfaces() (NetworkInterfaceIterator, error)
	ReadWIFIState() *WIFIState
	ClearDNSCache()
	SendNotification(notification *Notification) error
	CancelNotification(identifier string, typeID int32) error
}

type ConnectionOwner struct {
	UserId              int32
	UserName            string
	ProcessPath         string
	androidPackageNames []string
}

func (c *ConnectionOwner) SetAndroidPackageNames(names StringIterator) {
	c.androidPackageNames = iteratorToArray[string](names)
}

func (c *ConnectionOwner) AndroidPackageNames() StringIterator {
	return newIterator(c.androidPackageNames)
}

type InterfaceUpdateListener interface {
	UpdateDefaultInterface(interfaceName string, interfaceIndex int32, isExpensive bool, isConstrained bool)
	UpdateNetworkPath(networkPath string)
}

const (
	InterfaceTypeWIFI     = int32(C.InterfaceTypeWIFI)
	InterfaceTypeCellular = int32(C.InterfaceTypeCellular)
	InterfaceTypeEthernet = int32(C.InterfaceTypeEthernet)
	InterfaceTypeOther    = int32(C.InterfaceTypeOther)
)

type NetworkInterface struct {
	Index     int32
	MTU       int32
	Name      string
	Addresses StringIterator
	Flags     int32

	Type      int32
	DNSServer StringIterator
	Gateway   StringIterator
	Metered   bool
}

type WIFIState struct {
	SSID  string
	BSSID string
}

func NewWIFIState(wifiSSID string, wifiBSSID string) *WIFIState {
	return &WIFIState{wifiSSID, wifiBSSID}
}

type NetworkInterfaceIterator interface {
	Next() *NetworkInterface
	HasNext() bool
}

type Notification struct {
	Identifier string
	TypeName   string
	TypeID     int32
	Title      string
	Subtitle   string
	Body       string
	OpenURL    string
}
