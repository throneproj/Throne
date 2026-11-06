package sysdns

const (
	// Address is a lo0 alias: a query the system resolver scopes to the physical interface still loops back to it.
	Address = "127.0.0.53"
	// InboundTag names generate.cpp's DNS inbound on Address; the core moves it behind a pf redirect from port 53.
	InboundTag = "dns-system"
)
