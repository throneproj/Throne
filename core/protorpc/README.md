- *Go语言QQ群: 102319854, 1055927514*
- *凹语言(凹读音“Wa”)(The Wa Programming Language): https://github.com/wa-lang/wa*

----

# protorpc

```
██████╗ ██████╗  ██████╗ ████████╗ ██████╗       ██████╗ ██████╗  ██████╗
██╔══██╗██╔══██╗██╔═══██╗╚══██╔══╝██╔═══██╗      ██╔══██╗██╔══██╗██╔════╝
██████╔╝██████╔╝██║   ██║   ██║   ██║   ██║█████╗██████╔╝██████╔╝██║
██╔═══╝ ██╔══██╗██║   ██║   ██║   ██║   ██║╚════╝██╔══██╗██╔═══╝ ██║
██║     ██║  ██║╚██████╔╝   ██║   ╚██████╔╝      ██║  ██║██║     ╚██████╗
╚═╝     ╚═╝  ╚═╝ ╚═════╝    ╚═╝    ╚═════╝       ╚═╝  ╚═╝╚═╝      ╚═════╝
```

[![Build Status](https://travis-ci.org/chai2010/protorpc.svg)](https://travis-ci.org/chai2010/protorpc)
[![GoDoc](https://godoc.org/github.com/chai2010/protorpc?status.svg)](https://godoc.org/github.com/chai2010/protorpc)

- C++ Version(Proto2): [https://github.com/chai2010/protorpc.cxx](https://github.com/chai2010/protorpc.cxx)
- C++ Version(Proto3): [https://github.com/chai2010/protorpc3-cxx](https://github.com/chai2010/protorpc3-cxx)
- Talks: [Go/C++语言Protobuf-RPC简介](http://go-talks.appspot.com/github.com/chai2010/talks/chai2010-protorpc-intro.slide)

# Install

Install `protorpc` package:

1. `go install github.com/golang/protobuf/protoc-gen-go`
1. `go get github.com/chai2010/protorpc`
1. `go run hello.go`

Install `protoc-gen-go` plugin:

1. install `protoc` at first: http://github.com/google/protobuf/releases
1. `go get github.com/golang/protobuf/protoc-gen-go`
1. `go get github.com/chai2010/protorpc/protoc-gen-protorpc`
1. `go generate github.com/chai2010/protorpc/examples/service.pb`
1. `go test github.com/chai2010/protorpc/examples/service.pb`


# Examples

First, create [echo.proto](examples/service.pb/echo.proto):

```Proto
syntax = "proto3";

package service;

message EchoRequest {
	string msg = 1;
}

message EchoResponse {
	string msg = 1;
}

service EchoService {
	rpc Echo (EchoRequest) returns (EchoResponse);
	rpc EchoTwice (EchoRequest) returns (EchoResponse);
}
```

Second, generate [echo.pb.go](examples/service.pb/echo.pb.go) and [echo.pb.protorpc.go](examples/service.pb/echo.pb.protorpc.go)
from [echo.proto](examples/service.pb/echo.proto) (we can use `go generate` to invoke this command, see [proto.go](examples/service.pb/proto.go)).

	protoc --go_out=. echo.proto
	protoc --protorpc_out=. echo.proto


Now, we can use the stub code like this:

```Go
package main

import (
	"fmt"
	"log"

	"github.com/chai2010/protorpc"
	service "github.com/chai2010/protorpc/examples/service.pb"
)

type Echo int

func (t *Echo) Echo(args *service.EchoRequest, reply *service.EchoResponse) error {
	reply.Msg = args.Msg
	return nil
}

func (t *Echo) EchoTwice(args *service.EchoRequest, reply *service.EchoResponse) error {
	reply.Msg = args.Msg + args.Msg
	return nil
}

func init() {
	go service.ListenAndServeEchoService("tcp", `127.0.0.1:9527`, new(Echo))
}

func main() {
	echoClient, err := service.DialEchoService("tcp", `127.0.0.1:9527`)
	if err != nil {
		log.Fatalf("service.DialEchoService: %v", err)
	}
	defer echoClient.Close()

	args := &service.EchoRequest{Msg: "你好, 世界!"}
	reply, err := echoClient.EchoTwice(args)
	if err != nil {
		log.Fatalf("echoClient.EchoTwice: %v", err)
	}
	fmt.Println(reply.Msg)

	// or use normal client
	client, err := protorpc.Dial("tcp", `127.0.0.1:9527`)
	if err != nil {
		log.Fatalf("protorpc.Dial: %v", err)
	}
	defer client.Close()

	echoClient1 := &service.EchoServiceClient{client}
	echoClient2 := &service.EchoServiceClient{client}
	reply, err = echoClient1.EchoTwice(args)
	reply, err = echoClient2.EchoTwice(args)
	_, _ = reply, err

	// Output:
	// 你好, 世界!你好, 世界!
}
```

[More examples](examples).

# standard net/rpc

First, create [echo.proto](examples/stdrpc.pb/echo.proto):

```Proto
syntax = "proto3";

package service;

message EchoRequest {
	string msg = 1;
}

message EchoResponse {
	string msg = 1;
}

service EchoService {
	rpc Echo (EchoRequest) returns (EchoResponse);
	rpc EchoTwice (EchoRequest) returns (EchoResponse);
}
```

Second, generate [echo.pb.go](examples/stdrpc.pb/echo.pb.go) from [echo.proto](examples/stdrpc.pb/echo.proto) with `protoc-gen-stdrpc` plugin.

	protoc --stdrpc_out=. echo.proto

The stdrpc plugin generated code do not depends **protorpc** package, it use gob as the default rpc encoding.

# Add prefix

```
$ ENV_PROTOC_GEN_PROTORPC_FLAG_PREFIX=abc protoc --protorpc_out=. x.proto
```

# BUGS

Report bugs to <chaishushan@gmail.com>.

Thanks!
