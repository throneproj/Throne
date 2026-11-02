package sysdns

/*
#cgo LDFLAGS: -framework SystemConfiguration -framework CoreFoundation
#include <stdlib.h>
#include <SystemConfiguration/SystemConfiguration.h>

static SCDynamicStoreRef store;

static int storeOpen(void) {
	if (store == NULL) {
		store = SCDynamicStoreCreate(NULL, CFSTR("Throne"), NULL, NULL);
	}
	return store != NULL ? kSCStatusOK : SCError();
}

static int storeAddTemporary(const char *key, const void *plist, long length) {
	CFDataRef data = CFDataCreate(NULL, plist, length);
	if (data == NULL) {
		return kSCStatusFailed;
	}
	CFPropertyListRef value = CFPropertyListCreateWithData(NULL, data, kCFPropertyListImmutable, NULL, NULL);
	CFRelease(data);
	if (value == NULL) {
		return kSCStatusFailed;
	}
	CFStringRef name = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
	Boolean added = SCDynamicStoreAddTemporaryValue(store, name, value);
	CFRelease(name);
	CFRelease(value);
	return added ? kSCStatusOK : SCError();
}

static void storeRemove(const char *key) {
	CFStringRef name = CFStringCreateWithCString(NULL, key, kCFStringEncodingUTF8);
	SCDynamicStoreRemoveValue(store, name);
	CFRelease(name);
}

static void storeClose(void) {
	if (store != NULL) {
		CFRelease(store);
		store = NULL;
	}
}
*/
import "C"

import (
	"bytes"
	"encoding/xml"
	"log"
	"net"
	"net/netip"
	"slices"
	"strings"
	"sync"
	"unsafe"

	E "github.com/sagernet/sing/common/exceptions"
	"github.com/sagernet/sing/common/shell"
)

const (
	serviceKeyPrefix = "State:/Network/Service/Throne/"
	// mDNSResponder scopes each query to the interface NECP routes 0.0.0.0 to, and sing-tun's darwin sub-ranges leave that on the physical one.
	lookupRoute = "0.0.0.0/8"
)

var (
	mu          sync.Mutex
	keys        []string
	routeDevice string
)

// Apply publishes the Tun as a network service whose DNS server sits inside it, then routes the lookup probe into it.
func Apply(tunCIDR string) error {
	mu.Lock()
	defer mu.Unlock()
	releaseLocked()
	prefix, err := netip.ParsePrefix(tunCIDR)
	if err != nil || !prefix.Addr().Is4() {
		return E.New("invalid Tun address ", tunCIDR)
	}
	device, inet6, err := tunDevice(prefix.Addr())
	if err != nil {
		return err
	}
	if code := C.storeOpen(); code != C.kSCStatusOK {
		return E.New("open the dynamic store: ", scError(code))
	}
	type entity struct {
		name  string
		value map[string]any
	}
	// Router = own address makes every route local to the Tun; with no default route configd builds no scoped resolver.
	ipEntity := func(address string) map[string]any {
		return map[string]any{"Addresses": []string{address}, "Router": address, "InterfaceName": device}
	}
	entities := []entity{
		// Added first and removed last: Never keeps the service from ever becoming primary.
		{"Service", map[string]any{"PrimaryRank": "Never"}},
		{"IPv4", ipEntity(prefix.Addr().String())},
	}
	if inet6.IsValid() {
		entities = append(entities, entity{"IPv6", ipEntity(inet6.String())})
	}
	entities = append(entities, entity{"DNS", map[string]any{"ServerAddresses": []string{prefix.Addr().Next().String()}}})
	for _, entity := range entities {
		if err = addTemporary(serviceKeyPrefix+entity.name, plist(entity.value)); err != nil {
			releaseLocked()
			return E.Cause(err, "publish the Tun ", entity.name)
		}
	}
	output, err := shell.Exec("/sbin/route", "-n", "add", "-net", lookupRoute, "-interface", device).Read()
	if err = withOutput(err, output); err != nil {
		releaseLocked()
		return E.Cause(err, "route ", lookupRoute, " into the Tun")
	}
	routeDevice = device
	return nil
}

// Release takes the route and the service back; a dying core loses both anyway, with its utun and its configd session.
func Release() {
	mu.Lock()
	defer mu.Unlock()
	releaseLocked()
}

func releaseLocked() {
	if routeDevice != "" {
		output, err := shell.Exec("/sbin/route", "-n", "delete", "-net", lookupRoute, "-interface", routeDevice).Read()
		if err = withOutput(err, output); err != nil {
			log.Println("system DNS: delete the lookup route:", err)
		}
		routeDevice = ""
	}
	for _, key := range slices.Backward(keys) {
		name := C.CString(key)
		C.storeRemove(name)
		C.free(unsafe.Pointer(name))
	}
	keys = nil
	C.storeClose()
}

func addTemporary(key string, value []byte) error {
	name := C.CString(key)
	defer C.free(unsafe.Pointer(name))
	if code := C.storeAddTemporary(name, unsafe.Pointer(&value[0]), C.long(len(value))); code != C.kSCStatusOK {
		return E.New(scError(code))
	}
	keys = append(keys, key)
	return nil
}

func scError(code C.int) string {
	return C.GoString(C.SCErrorString(code))
}

// plist encodes string and []string values as an XML property list dictionary.
func plist(values map[string]any) []byte {
	var buffer bytes.Buffer
	buffer.WriteString(`<?xml version="1.0" encoding="UTF-8"?><plist version="1.0"><dict>`)
	writeElement := func(tag, text string) {
		buffer.WriteString("<" + tag + ">")
		_ = xml.EscapeText(&buffer, []byte(text))
		buffer.WriteString("</" + tag + ">")
	}
	for key, value := range values {
		writeElement("key", key)
		switch value := value.(type) {
		case string:
			writeElement("string", value)
		case []string:
			buffer.WriteString("<array>")
			for _, item := range value {
				writeElement("string", item)
			}
			buffer.WriteString("</array>")
		}
	}
	buffer.WriteString("</dict></plist>")
	return buffer.Bytes()
}

// tunDevice finds the utun holding address, and its first routable IPv6 address if it has one.
func tunDevice(address netip.Addr) (string, netip.Addr, error) {
	interfaces, err := net.Interfaces()
	if err != nil {
		return "", netip.Addr{}, err
	}
	for _, networkInterface := range interfaces {
		addresses, addrErr := networkInterface.Addrs()
		if addrErr != nil {
			continue
		}
		var found bool
		var inet6 netip.Addr
		for _, entry := range addresses {
			ipNet, ok := entry.(*net.IPNet)
			if !ok {
				continue
			}
			ip, ok := netip.AddrFromSlice(ipNet.IP)
			if !ok {
				continue
			}
			ip = ip.Unmap()
			if ip == address {
				found = true
			} else if ip.Is6() && !ip.IsLinkLocalUnicast() && !inet6.IsValid() {
				inet6 = ip
			}
		}
		if found {
			return networkInterface.Name, inet6, nil
		}
	}
	return "", netip.Addr{}, E.New("no interface holds the Tun address ", address)
}

func withOutput(err error, output string) error {
	if err == nil {
		return nil
	}
	if output = strings.TrimSpace(output); output != "" {
		return E.Cause(err, output)
	}
	return err
}
