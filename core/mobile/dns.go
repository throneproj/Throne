package mobile

import (
	"context"
	"errors"
	"net/netip"
	"slices"
	"strings"
	"sync"
	"sync/atomic"
	"syscall"
	"time"

	"github.com/sagernet/sing-box/adapter"
	C "github.com/sagernet/sing-box/constant"
	"github.com/sagernet/sing-box/dns"
	"github.com/sagernet/sing-box/dns/transport"
	"github.com/sagernet/sing-box/dns/transport/local"
	"github.com/sagernet/sing-box/log"
	"github.com/sagernet/sing-box/option"
	"github.com/sagernet/sing/common"
	E "github.com/sagernet/sing/common/exceptions"
	M "github.com/sagernet/sing/common/metadata"
	N "github.com/sagernet/sing/common/network"
	"github.com/sagernet/sing/service"

	mDNS "github.com/miekg/dns"
)

// The budget sing-box gives the servers of resolv.conf and of the Windows adapters.
const (
	networkDNSTimeout  = 5 * time.Second
	networkDNSAttempts = 2
)

type LocalDNSTransport interface {
	Raw() bool
	Lookup(ctx *ExchangeContext, network string, domain string) error
	Exchange(ctx *ExchangeContext, message []byte) error
}

// `local` asks the default network's DNS servers itself, as sing-box does on Linux and Windows. Android's
// resolver also applies Private DNS, which stalls every lookup when its server is unreachable without the
// VPN, so it only answers for a network that lists no DNS server.
type platformTransport struct {
	dns.TransportAdapter
	logger            log.ContextLogger
	iif               LocalDNSTransport
	preferredResolver *local.PreferredDomainResolver
	networkManager    adapter.NetworkManager
	dialer            N.Dialer
	serverSet         atomic.Pointer[networkServerSet]
	serverSetAccess   sync.Mutex
}

type networkServerSet struct {
	servers    []string
	transports []adapter.DNSTransport
}

// localDNSError is an exchange of the local server that got no answer; Instance.Start looks for it to tell a
// start that failed on the network's DNS from other failures.
type localDNSError struct {
	servers string
	err     error
}

func (e *localDNSError) Error() string {
	return e.err.Error()
}

func (e *localDNSError) Unwrap() error {
	return e.err
}

func newPlatformTransport(ctx context.Context, logger log.ContextLogger, iif LocalDNSTransport, tag string, options option.LocalDNSServerOptions) (*platformTransport, error) {
	preferredResolver, err := local.NewPreferredDomainResolver(ctx, logger, options)
	if err != nil {
		return nil, err
	}
	transportDialer, err := dns.NewLocalDialer(ctx, options)
	if err != nil {
		return nil, err
	}
	return &platformTransport{
		TransportAdapter:  dns.NewTransportAdapterWithLocalOptions(C.DNSTypeLocal, tag, options),
		logger:            logger,
		iif:               iif,
		preferredResolver: preferredResolver,
		networkManager:    service.FromContext[adapter.NetworkManager](ctx),
		dialer:            transportDialer,
	}, nil
}

func (p *platformTransport) Start(stage adapter.StartStage) error {
	p.preferredResolver.Start(stage)
	return nil
}

func (p *platformTransport) Close() error {
	serverSet := p.serverSet.Swap(nil)
	if serverSet != nil {
		serverSet.close()
	}
	return nil
}

func (p *platformTransport) Reset() {
	serverSet := p.serverSet.Load()
	if serverSet != nil {
		for _, serverTransport := range serverSet.transports {
			serverTransport.Reset()
		}
	}
}

func (p *platformTransport) PreferredDomain(domain string) bool {
	return p.preferredResolver.PreferredDomain(domain)
}

func (p *platformTransport) Environment() []string {
	if p.networkManager == nil {
		return nil
	}
	defaultInterface := p.networkManager.DefaultNetworkInterface()
	if defaultInterface == nil {
		return nil
	}
	return defaultInterface.DNSServers
}

func (p *platformTransport) Exchange(ctx context.Context, message *mDNS.Msg) (*mDNS.Msg, error) {
	localResponse := p.preferredResolver.Lookup(message)
	if localResponse != nil {
		return localResponse, nil
	}
	serverSet, err := p.networkServers()
	if err != nil {
		return nil, err
	}
	var (
		response *mDNS.Msg
		servers  string
	)
	if serverSet != nil {
		servers = strings.Join(serverSet.servers, ", ")
		response, err = serverSet.exchange(ctx, message)
	} else {
		response, err = p.exchangePlatform(ctx, message)
	}
	var rcodeError dns.RcodeError
	if err != nil && !errors.Is(err, context.Canceled) && !errors.As(err, &rcodeError) {
		return nil, &localDNSError{servers: servers, err: err}
	}
	return response, err
}

// The servers the default network lists, nil when it lists none.
func (p *platformTransport) networkServers() (*networkServerSet, error) {
	if p.networkManager == nil {
		return nil, nil
	}
	defaultInterface := p.networkManager.DefaultNetworkInterface()
	if defaultInterface == nil || len(defaultInterface.DNSServers) == 0 {
		return nil, nil
	}
	servers := defaultInterface.DNSServers
	serverSet := p.serverSet.Load()
	if serverSet != nil && slices.Equal(serverSet.servers, servers) {
		return serverSet, nil
	}
	p.serverSetAccess.Lock()
	defer p.serverSetAccess.Unlock()
	serverSet = p.serverSet.Load()
	if serverSet != nil && slices.Equal(serverSet.servers, servers) {
		return serverSet, nil
	}
	transports := make([]adapter.DNSTransport, 0, len(servers))
	for _, server := range servers {
		serverAddr := M.ParseSocksaddrHostPort(server, 53)
		if !serverAddr.IsIP() {
			continue
		}
		serverTransport := transport.NewUDPRaw(p.logger, dns.NewTransportAdapter(C.DNSTypeUDP, "", nil), p.dialer, serverAddr)
		err := serverTransport.Start(adapter.StartStateStart)
		if err != nil {
			for _, startedTransport := range transports {
				startedTransport.Close()
			}
			return nil, E.Cause(err, "initialize transport for ", serverAddr)
		}
		transports = append(transports, serverTransport)
	}
	if len(transports) == 0 {
		return nil, nil
	}
	newServerSet := &networkServerSet{
		servers:    slices.Clone(servers),
		transports: transports,
	}
	oldServerSet := p.serverSet.Swap(newServerSet)
	if oldServerSet != nil {
		oldServerSet.close()
	}
	p.logger.Debug("network DNS servers: ", strings.Join(servers, ", "))
	return newServerSet, nil
}

// Each attempt walks the servers in order, as local_shared.go does for resolv.conf.
func (s *networkServerSet) exchange(ctx context.Context, message *mDNS.Msg) (*mDNS.Msg, error) {
	fqdn := message.Question[0].Name
	exchangers := make([]transport.AsyncExchanger, 0, networkDNSAttempts*len(s.transports))
	for range networkDNSAttempts {
		for _, serverTransport := range s.transports {
			exchangers = append(exchangers, func(ctx context.Context, callback func(response *mDNS.Msg, err error)) {
				attemptCtx, cancel := context.WithTimeout(ctx, networkDNSTimeout)
				serverTransport.ExchangeAsync(attemptCtx, transport.NewFanOutRequest(message, fqdn, false), func(response *mDNS.Msg, err error) {
					cancel()
					callback(response, err)
				})
			})
		}
	}
	done := make(chan struct{})
	var (
		response *mDNS.Msg
		err      error
	)
	transport.ExchangeSequential(ctx, exchangers, nil, func(callbackResponse *mDNS.Msg, callbackErr error) {
		response, err = callbackResponse, callbackErr
		close(done)
	})
	<-done
	return response, err
}

func (s *networkServerSet) close() {
	for _, serverTransport := range s.transports {
		serverTransport.Close()
	}
}

// The Kotlin resolver runs on its own goroutine so a stalled platform call cannot hold the DNS
// router past the query's context.
func (p *platformTransport) exchangePlatform(ctx context.Context, message *mDNS.Msg) (*mDNS.Msg, error) {
	response := &ExchangeContext{
		context: ctx,
	}
	if p.iif.Raw() {
		messageBytes, err := message.Pack()
		if err != nil {
			return nil, err
		}
		done := make(chan error, 1)
		go func() {
			exchangeErr := p.iif.Exchange(response, messageBytes)
			if exchangeErr == nil {
				exchangeErr = response.error
			}
			done <- exchangeErr
		}()
		select {
		case err = <-done:
			if err != nil {
				return nil, err
			}
			return &response.message, nil
		case <-ctx.Done():
			return nil, ctx.Err()
		}
	} else {
		question := message.Question[0]
		var network string
		switch question.Qtype {
		case mDNS.TypeA:
			network = "ip4"
		case mDNS.TypeAAAA:
			network = "ip6"
		default:
			return nil, E.New("only IP queries are supported by current version of Android")
		}
		done := make(chan error, 1)
		go func() {
			lookupErr := p.iif.Lookup(response, network, question.Name)
			if lookupErr == nil {
				lookupErr = response.error
			}
			done <- lookupErr
		}()
		select {
		case err := <-done:
			if err != nil {
				return nil, err
			}
			return dns.FixedResponse(message.Id, question, response.addresses, C.DefaultDNSTTL), nil
		case <-ctx.Done():
			return nil, ctx.Err()
		}
	}
}

func (p *platformTransport) ExchangeAsync(ctx context.Context, message *mDNS.Msg, callback func(response *mDNS.Msg, err error)) {
	go func() {
		callback(p.Exchange(ctx, message))
	}()
}

type Func interface {
	Invoke() error
}

type ExchangeContext struct {
	context   context.Context
	message   mDNS.Msg
	addresses []netip.Addr
	error     error
}

func (c *ExchangeContext) OnCancel(callback Func) {
	go func() {
		<-c.context.Done()
		callback.Invoke()
	}()
}

func (c *ExchangeContext) Success(result string) {
	c.addresses = common.Map(common.Filter(strings.Split(result, "\n"), func(it string) bool {
		return !common.IsEmpty(it)
	}), func(it string) netip.Addr {
		return M.ParseSocksaddrHostPort(it, 0).Unwrap().Addr
	})
}

func (c *ExchangeContext) RawSuccess(result []byte) {
	err := c.message.Unpack(result)
	if err != nil {
		c.error = E.Cause(err, "parse response")
	}
}

func (c *ExchangeContext) ErrorCode(code int32) {
	c.error = dns.RcodeError(code)
}

func (c *ExchangeContext) ErrnoCode(code int32) {
	c.error = syscall.Errno(code)
}

var (
	_ adapter.DNSTransport                    = (*platformTransport)(nil)
	_ adapter.DNSTransportWithPreferredDomain = (*platformTransport)(nil)
	_ adapter.DNSTransportWithEnvironment     = (*platformTransport)(nil)
)
