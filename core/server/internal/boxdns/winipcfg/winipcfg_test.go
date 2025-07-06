/* SPDX-License-Identifier: MIT
 *
 * Copyright (C) 2019-2022 WireGuard LLC. All Rights Reserved.
 */

/*

Some tests in this file require:

- A dedicated network adapter
	Any network adapter will do. It may be virtual (WireGuardNT, Wintun,
	etc.). The adapter name must contain string "winipcfg_test".
	Tests will add, remove, flush DNS servers, change adapter IP address, manipulate
	routes etc.
	The adapter will not be returned to previous state, so use an expendable one.

- Elevation
	Run go test as Administrator

*/

package winipcfg

import (
	"net/netip"
	"strings"
	"syscall"
	"testing"
	"time"

	"golang.org/x/sys/windows"
)

const (
	testInterfaceMarker = "winipcfg_test" // The interface we will use for testing must contain this string in its name
)

// TODO: Add IPv6 tests.
var (
	nonexistantIPv4ToAdd      = netip.MustParsePrefix("172.16.1.114/24")
	nonexistentRouteIPv4ToAdd = RouteData{
		Destination: netip.MustParsePrefix("172.16.200.0/24"),
		NextHop:     netip.MustParseAddr("172.16.1.2"),
		Metric:      0,
	}
	dnsesToSet = []netip.Addr{netip.MustParseAddr("8.8.8.8"), netip.MustParseAddr("8.8.4.4"), netip.MustParseAddr("9.9.9.9"), netip.MustParseAddr("149.112.112.112")}
)

func runningElevated() bool {
	var process windows.Token
	err := windows.OpenProcessToken(windows.CurrentProcess(), windows.TOKEN_QUERY, &process)
	if err != nil {
		return false
	}
	defer process.Close()
	return process.IsElevated()
}

func getTestInterface() (*IPAdapterAddresses, error) {
	ifcs, err := GetAdaptersAddresses(windows.AF_UNSPEC, GAAFlagIncludeAll)
	if err != nil {
		return nil, err
	}

	marker := strings.ToLower(testInterfaceMarker)
	for _, ifc := range ifcs {
		if strings.Contains(strings.ToLower(ifc.FriendlyName()), marker) {
			return ifc, nil
		}
	}

	return nil, windows.ERROR_NOT_FOUND
}

func getTestIPInterface(family AddressFamily) (*MibIPInterfaceRow, error) {
	ifc, err := getTestInterface()
	if err != nil {
		return nil, err
	}

	return ifc.LUID.IPInterface(family)
}

func TestAdaptersAddresses(t *testing.T) {
	ifcs, err := GetAdaptersAddresses(windows.AF_UNSPEC, GAAFlagIncludeAll)
	if err != nil {
		t.Errorf("GetAdaptersAddresses() returned error: %w", err)
	} else if ifcs == nil {
		t.Errorf("GetAdaptersAddresses() returned nil.")
	} else if len(ifcs) == 0 {
		t.Errorf("GetAdaptersAddresses() returned empty.")
	} else {
		for _, i := range ifcs {
			i.AdapterName()
			i.DNSSuffix()
			i.Description()
			i.FriendlyName()
			i.PhysicalAddress()
			i.DHCPv6ClientDUID()
			for dnsSuffix := i.FirstDNSSuffix; dnsSuffix != nil; dnsSuffix = dnsSuffix.Next {
				_ = dnsSuffix.String()
			}
		}
	}

	ifcs, err = GetAdaptersAddresses(windows.AF_UNSPEC, GAAFlagDefault)

	for _, i := range ifcs {
		ifc, err := i.LUID.Interface()
		if err != nil {
			t.Errorf("LUID.Interface() returned an error: %w", err)
			continue
		} else if ifc == nil {
			t.Errorf("LUID.Interface() returned nil.")
			continue
		}
	}

	for _, i := range ifcs {
		guid, err := i.LUID.GUID()
		if err != nil {
			t.Errorf("LUID.GUID() returned an error: %w", err)
			continue
		}
		if guid == nil {
			t.Error("LUID.GUID() returned nil.")
			continue
		}

		luid, err := LUIDFromGUID(guid)
		if err != nil {
			t.Errorf("LUIDFromGUID() returned an error: %w", err)
			continue
		}
		if luid != i.LUID {
			t.Errorf("LUIDFromGUID() returned LUID %d, although expected was %d.", luid, i.LUID)
			continue
		}
	}
}

func TestIPInterface(t *testing.T) {
	ifcs, err := GetAdaptersAddresses(windows.AF_UNSPEC, GAAFlagDefault)
	if err != nil {
		t.Errorf("GetAdaptersAddresses() returned error: %w", err)
	}

	for _, i := range ifcs {
		_, err := i.LUID.IPInterface(windows.AF_INET)
		if err == windows.ERROR_NOT_FOUND {
			// Ignore isatap and similar adapters without IPv4.
			continue
		}
		if err != nil {
			t.Errorf("LUID.IPInterface(%s) returned an error: %w", i.FriendlyName(), err)
		}

		_, err = i.LUID.IPInterface(windows.AF_INET6)
		if err != nil {
			t.Errorf("LUID.IPInterface(%s) returned an error: %w", i.FriendlyName(), err)
		}
	}
}

func TestIPInterfaces(t *testing.T) {
	tab, err := GetIPInterfaceTable(windows.AF_UNSPEC)
	if err != nil {
		t.Errorf("GetIPInterfaceTable() returned an error: %w", err)
		return
	} else if tab == nil {
		t.Error("GetIPInterfaceTable() returned nil.")
	}

	if len(tab) == 0 {
		t.Error("GetIPInterfaceTable() returned an empty slice.")
		return
	}
}

func TestIPChangeMetric(t *testing.T) {
	ipifc, err := getTestIPInterface(windows.AF_INET)
	if err != nil {
		t.Errorf("getTestIPInterface() returned an error: %w", err)
		return
	}
	if !runningElevated() {
		t.Errorf("%s requires elevation", t.Name())
		return
	}

	var changed bool
	cb, err := RegisterInterfaceChangeCallback(func(notificationType MibNotificationType, iface *MibIPInterfaceRow) {
		if iface == nil || iface.InterfaceLUID != ipifc.InterfaceLUID {
			return
		}
		switch notificationType {
		case MibParameterNotification:
			changed = true
		}
	})
	if err != nil {
		t.Errorf("RegisterInterfaceChangeCallback() returned error: %w", err)
		return
	}
	defer func() {
		err = cb.Unregister()
		if err != nil {
			t.Errorf("UnregisterInterfaceChangeCallback() returned error: %w", err)
		}
	}()

	useAutomaticMetric := ipifc.UseAutomaticMetric
	metric := ipifc.Metric

	newMetric := uint32(100)
	if newMetric == metric {
		newMetric = 200
	}

	ipifc.UseAutomaticMetric = false
	ipifc.Metric = newMetric
	err = ipifc.Set()
	if err != nil {
		t.Errorf("MibIPInterfaceRow.Set() returned an error: %w", err)
	}

	time.Sleep(500 * time.Millisecond)

	ipifc, err = getTestIPInterface(windows.AF_INET)
	if err != nil {
		t.Errorf("getTestIPInterface() returned an error: %w", err)
		return
	}
	if ipifc.Metric != newMetric {
		t.Errorf("Expected metric: %d; actual metric: %d", newMetric, ipifc.Metric)
	}
	if ipifc.UseAutomaticMetric {
		t.Error("UseAutomaticMetric is true although it's set to false.")
	}
	if !changed {
		t.Errorf("Notification handler has not been called on metric change.")
	}
	changed = false

	ipifc.UseAutomaticMetric = useAutomaticMetric
	ipifc.Metric = metric
	err = ipifc.Set()
	if err != nil {
		t.Errorf("MibIPInterfaceRow.Set() returned an error: %w", err)
	}

	time.Sleep(500 * time.Millisecond)

	ipifc, err = getTestIPInterface(windows.AF_INET)
	if err != nil {
		t.Errorf("getTestIPInterface() returned an error: %w", err)
		return
	}
	if ipifc.Metric != metric {
		t.Errorf("Expected metric: %d; actual metric: %d", metric, ipifc.Metric)
	}
	if ipifc.UseAutomaticMetric != useAutomaticMetric {
		t.Errorf("UseAutomaticMetric is %v although %v is expected.", ipifc.UseAutomaticMetric, useAutomaticMetric)
	}
	if !changed {
		t.Errorf("Notification handler has not been called on metric change.")
	}
}

func TestIPChangeMTU(t *testing.T) {
	ipifc, err := getTestIPInterface(windows.AF_INET)
	if err != nil {
		t.Errorf("getTestIPInterface() returned an error: %w", err)
		return
	}
	if !runningElevated() {
		t.Errorf("%s requires elevation", t.Name())
		return
	}

	prevMTU := ipifc.NLMTU
	mtuToSet := prevMTU - 1
	ipifc.NLMTU = mtuToSet
	err = ipifc.Set()
	if err != nil {
		t.Errorf("Interface.Set() returned error: %w", err)
	}

	time.Sleep(500 * time.Millisecond)

	ipifc, err = getTestIPInterface(windows.AF_INET)
	if err != nil {
		t.Errorf("getTestIPInterface() returned an error: %w", err)
		return
	}
	if ipifc.NLMTU != mtuToSet {
		t.Errorf("Interface.NLMTU is %d although %d is expected.", ipifc.NLMTU, mtuToSet)
	}

	ipifc.NLMTU = prevMTU
	err = ipifc.Set()
	if err != nil {
		t.Errorf("Interface.Set() returned error: %w", err)
	}

	time.Sleep(500 * time.Millisecond)

	ipifc, err = getTestIPInterface(windows.AF_INET)
	if err != nil {
		t.Errorf("getTestIPInterface() returned an error: %w", err)
	}
	if ipifc.NLMTU != prevMTU {
		t.Errorf("Interface.NLMTU is %d although %d is expected.", ipifc.NLMTU, prevMTU)
	}
}

func TestGetIfRow(t *testing.T) {
	ifc, err := getTestInterface()
	if err != nil {
		t.Errorf("getTestInterface() returned an error: %w", err)
		return
	}

	row, err := ifc.LUID.Interface()
	if err != nil {
		t.Errorf("LUID.Interface() returned an error: %w", err)
		return
	}

	row.Alias()
	row.Description()
	row.PhysicalAddress()
	row.PermanentPhysicalAddress()
}

func TestGetIfRows(t *testing.T) {
	tab, err := GetIfTable2Ex(MibIfEntryNormal)
	if err != nil {
		t.Errorf("GetIfTable2Ex() returned an error: %w", err)
		return
	} else if tab == nil {
		t.Errorf("GetIfTable2Ex() returned nil")
		return
	}

	for i := range tab {
		tab[i].Alias()
		tab[i].Description()
		tab[i].PhysicalAddress()
		tab[i].PermanentPhysicalAddress()
	}
}

func TestUnicastIPAddress(t *testing.T) {
	_, err := GetUnicastIPAddressTable(windows.AF_UNSPEC)
	if err != nil {
		t.Errorf("GetUnicastAddresses() returned an error: %w", err)
		return
	}
}

func TestAddDeleteIPAddress(t *testing.T) {
	ifc, err := getTestInterface()
	if err != nil {
		t.Errorf("getTestInterface() returned an error: %w", err)
		return
	}
	if !runningElevated() {
		t.Errorf("%s requires elevation", t.Name())
		return
	}

	addr, err := ifc.LUID.IPAddress(nonexistantIPv4ToAdd.Addr())
	if err == nil {
		t.Errorf("Unicast address %s already exists. Please set nonexistantIPv4ToAdd appropriately.", nonexistantIPv4ToAdd.Addr().String())
		return
	} else if err != windows.ERROR_NOT_FOUND {
		t.Errorf("LUID.IPAddress() returned an error: %w", err)
		return
	}

	var created, deleted bool
	cb, err := RegisterUnicastAddressChangeCallback(func(notificationType MibNotificationType, addr *MibUnicastIPAddressRow) {
		if addr == nil || addr.InterfaceLUID != ifc.LUID {
			return
		}
		switch notificationType {
		case MibAddInstance:
			created = true
		case MibDeleteInstance:
			deleted = true
		}
	})
	if err != nil {
		t.Errorf("RegisterUnicastAddressChangeCallback() returned an error: %w", err)
	} else {
		defer cb.Unregister()
	}
	var count int
	for addr := ifc.FirstUnicastAddress; addr != nil; addr = addr.Next {
		count--
	}
	err = ifc.LUID.AddIPAddresses([]netip.Prefix{nonexistantIPv4ToAdd})
	if err != nil {
		t.Errorf("LUID.AddIPAddresses() returned an error: %w", err)
	}

	time.Sleep(500 * time.Millisecond)

	ifc, _ = getTestInterface()
	for addr := ifc.FirstUnicastAddress; addr != nil; addr = addr.Next {
		count++
	}
	if count != 1 {
		t.Errorf("After adding there are %d new interface(s).", count)
	}
	addr, err = ifc.LUID.IPAddress(nonexistantIPv4ToAdd.Addr())
	if err != nil {
		t.Errorf("LUID.IPAddress() returned an error: %w", err)
	} else if addr == nil {
		t.Errorf("Unicast address %s still doesn't exist, although it's added successfully.", nonexistantIPv4ToAdd.Addr().String())
	}
	if !created {
		t.Errorf("Notification handler has not been called on add.")
	}

	err = ifc.LUID.DeleteIPAddress(nonexistantIPv4ToAdd)
	if err != nil {
		t.Errorf("LUID.DeleteIPAddress() returned an error: %w", err)
	}

	time.Sleep(500 * time.Millisecond)

	addr, err = ifc.LUID.IPAddress(nonexistantIPv4ToAdd.Addr())
	if err == nil {
		t.Errorf("Unicast address %s still exists, although it's deleted successfully.", nonexistantIPv4ToAdd.Addr().String())
	} else if err != windows.ERROR_NOT_FOUND {
		t.Errorf("LUID.IPAddress() returned an error: %w", err)
	}
	if !deleted {
		t.Errorf("Notification handler has not been called on delete.")
	}
}

func TestGetRoutes(t *testing.T) {
	_, err := GetIPForwardTable2(windows.AF_UNSPEC)
	if err != nil {
		t.Errorf("GetIPForwardTable2() returned error: %w", err)
	}
}

func TestAddDeleteRoute(t *testing.T) {
	findRoute := func(luid LUID, dest netip.Prefix) ([]MibIPforwardRow2, error) {
		var family AddressFamily
		if dest.Addr().Is4() {
			family = windows.AF_INET
		} else if dest.Addr().Is6() {
			family = windows.AF_INET6
		} else {
			return nil, windows.ERROR_INVALID_PARAMETER
		}
		r, err := GetIPForwardTable2(family)
		if err != nil {
			return nil, err
		}
		matches := make([]MibIPforwardRow2, 0, len(r))
		for _, route := range r {
			if route.InterfaceLUID == luid && route.DestinationPrefix.PrefixLength == uint8(dest.Bits()) && route.DestinationPrefix.RawPrefix.Family == family && route.DestinationPrefix.RawPrefix.Addr() == dest.Addr() {
				matches = append(matches, route)
			}
		}
		return matches, nil
	}

	ifc, err := getTestInterface()
	if err != nil {
		t.Errorf("getTestInterface() returned an error: %w", err)
		return
	}
	if !runningElevated() {
		t.Errorf("%s requires elevation", t.Name())
		return
	}

	_, err = ifc.LUID.Route(nonexistentRouteIPv4ToAdd.Destination, nonexistentRouteIPv4ToAdd.NextHop)
	if err == nil {
		t.Error("LUID.Route() returned a route although it isn't added yet. Have you forgot to set nonexistentRouteIPv4ToAdd appropriately?")
		return
	} else if err != windows.ERROR_NOT_FOUND {
		t.Errorf("LUID.Route() returned an error: %w", err)
		return
	}

	routes, err := findRoute(ifc.LUID, nonexistentRouteIPv4ToAdd.Destination)
	if err != nil {
		t.Errorf("findRoute() returned an error: %w", err)
	} else if len(routes) != 0 {
		t.Errorf("findRoute() returned %d items although the route isn't added yet. Have you forgot to set nonexistentRouteIPv4ToAdd appropriately?", len(routes))
	}

	var created, deleted bool
	cb, err := RegisterRouteChangeCallback(func(notificationType MibNotificationType, route *MibIPforwardRow2) {
		switch notificationType {
		case MibAddInstance:
			created = true
		case MibDeleteInstance:
			deleted = true
		}
	})
	if err != nil {
		t.Errorf("RegisterRouteChangeCallback() returned an error: %w", err)
	} else {
		defer cb.Unregister()
	}
	err = ifc.LUID.AddRoute(nonexistentRouteIPv4ToAdd.Destination, nonexistentRouteIPv4ToAdd.NextHop, nonexistentRouteIPv4ToAdd.Metric)
	if err != nil {
		t.Errorf("LUID.AddRoute() returned an error: %w", err)
	}

	time.Sleep(500 * time.Millisecond)

	route, err := ifc.LUID.Route(nonexistentRouteIPv4ToAdd.Destination, nonexistentRouteIPv4ToAdd.NextHop)
	if err == windows.ERROR_NOT_FOUND {
		t.Error("LUID.Route() returned nil although the route is added successfully.")
	} else if err != nil {
		t.Errorf("LUID.Route() returned an error: %w", err)
	} else if route.DestinationPrefix.RawPrefix.Addr() != nonexistentRouteIPv4ToAdd.Destination.Addr() || route.NextHop.Addr() != nonexistentRouteIPv4ToAdd.NextHop {
		t.Error("LUID.Route() returned a wrong route!")
	}
	if !created {
		t.Errorf("Route handler has not been called on add.")
	}

	routes, err = findRoute(ifc.LUID, nonexistentRouteIPv4ToAdd.Destination)
	if err != nil {
		t.Errorf("findRoute() returned an error: %w", err)
	} else if len(routes) != 1 {
		t.Errorf("findRoute() returned %d items although %d is expected.", len(routes), 1)
	} else if routes[0].DestinationPrefix.RawPrefix.Addr() != nonexistentRouteIPv4ToAdd.Destination.Addr() {
		t.Errorf("findRoute() returned a wrong route. Dest: %s; expected: %s.", routes[0].DestinationPrefix.RawPrefix.Addr().String(), nonexistentRouteIPv4ToAdd.Destination.Addr().String())
	}

	err = ifc.LUID.DeleteRoute(nonexistentRouteIPv4ToAdd.Destination, nonexistentRouteIPv4ToAdd.NextHop)
	if err != nil {
		t.Errorf("LUID.DeleteRoute() returned an error: %w", err)
	}

	time.Sleep(500 * time.Millisecond)

	_, err = ifc.LUID.Route(nonexistentRouteIPv4ToAdd.Destination, nonexistentRouteIPv4ToAdd.NextHop)
	if err == nil {
		t.Error("LUID.Route() returned a route although it is removed successfully.")
	} else if err != windows.ERROR_NOT_FOUND {
		t.Errorf("LUID.Route() returned an error: %w", err)
	}
	if !deleted {
		t.Errorf("Route handler has not been called on delete.")
	}

	routes, err = findRoute(ifc.LUID, nonexistentRouteIPv4ToAdd.Destination)
	if err != nil {
		t.Errorf("findRoute() returned an error: %w", err)
	} else if len(routes) != 0 {
		t.Errorf("findRoute() returned %d items although the route is deleted successfully.", len(routes))
	}
}

func TestFlushDNS(t *testing.T) {
	ifc, err := getTestInterface()
	if err != nil {
		t.Errorf("getTestInterface() returned an error: %w", err)
		return
	}
	if !runningElevated() {
		t.Errorf("%s requires elevation", t.Name())
		return
	}

	prevDNSes, err := ifc.LUID.DNS()
	if err != nil {
		t.Errorf("LUID.DNS() returned an error: %w", err)
	}

	err = ifc.LUID.FlushDNS(syscall.AF_INET)
	if err != nil {
		t.Errorf("LUID.FlushDNS() returned an error: %w", err)
	}

	ifc, _ = getTestInterface()

	n := 0
	dns, err := ifc.LUID.DNS()
	if err != nil {
		t.Errorf("LUID.DNS() returned an error: %w", err)
	}
	for _, a := range dns {
		if a.Is4() {
			n++
		}
	}
	if n != 0 {
		t.Errorf("DNSServerAddresses contains %d items, although FlushDNS is executed successfully.", n)
	}

	err = ifc.LUID.SetDNS(windows.AF_INET, prevDNSes, nil)
	if err != nil {
		t.Errorf("LUID.SetDNS() returned an error: %v.", err)
	}
}

func TestSetDNS(t *testing.T) {
	ifc, err := getTestInterface()
	if err != nil {
		t.Errorf("getTestInterface() returned an error: %w", err)
		return
	}
	if !runningElevated() {
		t.Errorf("%s requires elevation", t.Name())
		return
	}

	prevDNSes, err := ifc.LUID.DNS()
	if err != nil {
		t.Errorf("LUID.DNS() returned an error: %w", err)
	}

	err = ifc.LUID.SetDNS(windows.AF_INET, dnsesToSet, nil)
	if err != nil {
		t.Errorf("LUID.SetDNS() returned an error: %w", err)
		return
	}

	ifc, _ = getTestInterface()

	newDNSes, err := ifc.LUID.DNS()
	if err != nil {
		t.Errorf("LUID.DNS() returned an error: %w", err)
	} else if len(newDNSes) != len(dnsesToSet) {
		t.Errorf("dnsesToSet contains %d items, while DNSServerAddresses contains %d.", len(dnsesToSet), len(newDNSes))
	} else {
		for i := range dnsesToSet {
			if dnsesToSet[i] != newDNSes[i] {
				t.Errorf("dnsesToSet[%d] = %s while DNSServerAddresses[%d] = %s.", i, dnsesToSet[i].String(), i, newDNSes[i].String())
			}
		}
	}

	err = ifc.LUID.SetDNS(windows.AF_INET, prevDNSes, nil)
	if err != nil {
		t.Errorf("LUID.SetDNS() returned an error: %v.", err)
	}
}

func TestAnycastIPAddress(t *testing.T) {
	_, err := GetAnycastIPAddressTable(windows.AF_UNSPEC)
	if err != nil {
		t.Errorf("GetAnycastIPAddressTable() returned an error: %w", err)
		return
	}
}
