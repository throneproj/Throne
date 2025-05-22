package boxdns

import (
	"context"
	"errors"
	D "github.com/sagernet/sing-dns"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	"net"
	"reflect"
	"runtime"
	"unsafe"
)

var underlyingDNS string

func init() {
	D.RegisterTransport([]string{"underlying"}, createUnderlyingTransport)
}

func createUnderlyingTransport(options D.TransportOptions) (D.Transport, error) {
	if runtime.GOOS != "windows" {
		// Linux no resolv.conf change
		return D.NewLocalTransport(D.TransportOptions{
			Context: options.Context,
			Logger:  options.Logger,
			Name:    options.Name,
			Dialer:  options.Dialer,
			Address: "local",
		}), nil
	}
	// Windows Underlying DNS hook
	t, _ := D.NewUDPTransport(options)
	handler_ := reflect.Indirect(reflect.ValueOf(t)).FieldByName("dialer")
	handler_ = reflect.NewAt(handler_.Type(), unsafe.Pointer(handler_.UnsafeAddr())).Elem()
	handler_.Set(reflect.ValueOf(&myDialer{Dialer: options.Dialer}))
	return t, nil
}

var _ N.Dialer = (*myDialer)(nil)

type myDialer struct {
	N.Dialer
}

func (t *myDialer) DialContext(ctx context.Context, network string, destination M.Socksaddr) (net.Conn, error) {
	if underlyingDNS == "" {
		return nil, errors.New("no underlyingDNS")
	}
	return t.Dialer.DialContext(ctx, "udp", M.ParseSocksaddrHostPort(underlyingDNS, 53))
}
