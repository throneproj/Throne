// Copyright 2013 <chaishushan{AT}gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

/*
Package protorpc implements a Protobuf-RPC ClientCodec and ServerCodec
for the rpc package.

To install it, you must first have Go (version 1) installed
(see http://golang.org/doc/install). Next, install the standard
protocol buffer implementation from http://github.com/google/protobuf/;
you must be running version 2.3 or higher.

Finally run

	go get github.com/chai2010/protorpc
	go get github.com/chai2010/protorpc/protoc-gen-protorpc

to install the support library and protocol compiler.

Here is a simple proto file("arith.pb/arith.proto"):

	package arith;

	message ArithRequest {
		optional int32 a = 1;
		optional int32 b = 2;
	}

	message ArithResponse {
		optional int32 val = 1;
		optional int32 quo = 2;
		optional int32 rem = 3;
	}

	service ArithService {
		rpc multiply (ArithRequest) returns (ArithResponse);
		rpc divide (ArithRequest) returns (ArithResponse);
	}

Then use "protoc-gen-go" to generate "arith.pb.go" file:

	cd arith.pb && protoc --go_out=. arith.proto


Use "protoc-gen-protorpc" to generate "arith.pb.protorpc.go" file (include stub code):

	cd arith.pb && protoc --protorpc_out=. arith.proto

The server calls (for TCP service):

	package server

	import (
		"errors"

		"github.com/golang/protobuf/proto"

		"./arith.pb"
	)

	type Arith int

	func (t *Arith) Multiply(args *arith.ArithRequest, reply *arith.ArithResponse) error {
		reply.Val = proto.Int32(args.GetA() * args.GetB())
		return nil
	}

	func (t *Arith) Divide(args *arith.ArithRequest, reply *arith.ArithResponse) error {
		if args.GetB() == 0 {
			return errors.New("divide by zero")
		}
		reply.Quo = proto.Int32(args.GetA() / args.GetB())
		reply.Rem = proto.Int32(args.GetA() % args.GetB())
		return nil
	}

	func main() {
		arith.ListenAndServeArithService("tcp", ":1984", new(Arith))
	}

At this point, clients can see a service "Arith" with methods "ArithService.Multiply" and
"ArithService.Divide". To invoke one, a client first dials the server:

	stub, err := arith.DialArithService("tcp", "127.0.0.1:1984")
	if err != nil {
		log.Fatal(`arith.DialArithService("tcp", "127.0.0.1:1984"):`, err)
	}
	defer stub.Close()

Then it can make a remote call with stub:

	var args ArithRequest

	args.A = proto.Int32(7)
	args.B = proto.Int32(8)
	reply, err := stub.Multiply(&args, &reply)
	if err != nil {
		log.Fatal("arith error:", err)
	}
	fmt.Printf("Arith: %d*%d=%d", args.GetA(), args.GetB(), reply.GetVal())

More example:

	go test github.com/chai2010/protorpc/internal/service.pb

Report bugs to <chaishushan@gmail.com>.

Thanks!
*/
package protorpc // import "github.com/chai2010/protorpc"
