// Copyright 2013 <chaishushan{AT}gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package service

import (
	"log"
	"net"
	"net/rpc"
	"testing"

	"github.com/chai2010/protorpc"
)

func init() {
	err := listenAndServeArithAndEchoService("tcp", "127.0.0.1:1984")
	if err != nil {
		log.Fatalf("listenAndServeArithAndEchoService: %v", err)
	}
}

func TestAll(t *testing.T) {
	conn, err := net.Dial("tcp", "127.0.0.1:1984")
	if err != nil {
		t.Fatalf(`net.Dial("tcp", "127.0.0.1:1984"): %v`, err)
	}
	client := rpc.NewClientWithCodec(protorpc.NewClientCodec(conn))
	defer client.Close()

	testArithClient(t, client)
	testEchoClient(t, client)

	arithStub := &ArithServiceClient{client}
	echoStub := &EchoServiceClient{client}

	testArithStub(t, arithStub)
	testEchoStub(t, echoStub)
}

func listenAndServeArithAndEchoService(network, addr string) error {
	clients, err := net.Listen(network, addr)
	if err != nil {
		return err
	}
	srv := rpc.NewServer()
	if err := RegisterArithService(srv, new(Arith)); err != nil {
		return err
	}
	if err := RegisterEchoService(srv, new(Echo)); err != nil {
		return err
	}
	go func() {
		for {
			conn, err := clients.Accept()
			if err != nil {
				log.Printf("clients.Accept(): %v\n", err)
				continue
			}
			go srv.ServeCodec(protorpc.NewServerCodec(conn))
		}
	}()
	return nil
}

func testArithClient(t *testing.T, client *rpc.Client) {
	var args ArithRequest
	var reply ArithResponse
	var err error

	// Add
	args.A = 1
	args.B = 2
	if err = client.Call("ArithService.Add", &args, &reply); err != nil {
		t.Fatalf(`arith.Add: %v`, err)
	}
	if reply.C != 3 {
		t.Fatalf(`arith.Add: expected = %d, got = %d`, 3, reply.C)
	}

	// Mul
	args.A = 2
	args.B = 3
	if err = client.Call("ArithService.Mul", &args, &reply); err != nil {
		t.Fatalf(`arith.Mul: %v`, err)
	}
	if reply.C != 6 {
		t.Fatalf(`arith.Mul: expected = %d, got = %d`, 6, reply.C)
	}

	// Div
	args.A = 13
	args.B = 5
	if err = client.Call("ArithService.Div", &args, &reply); err != nil {
		t.Fatalf(`arith.Div: %v`, err)
	}
	if reply.C != 2 {
		t.Fatalf(`arith.Div: expected = %d, got = %d`, 2, reply.C)
	}

	// Div zero
	args.A = 1
	args.B = 0
	if err = client.Call("ArithService.Div", &args, &reply); err.Error() != "divide by zero" {
		t.Fatalf(`arith.Div: expected = "%s", got = "%s"`, "divide by zero", err.Error())
	}

	// Error
	args.A = 1
	args.B = 2
	if err = client.Call("ArithService.Error", &args, &reply); err.Error() != "ArithError" {
		t.Fatalf(`arith.Error: expected = "%s", got = "%s"`, "ArithError", err.Error())
	}
}

func testEchoClient(t *testing.T, client *rpc.Client) {
	var args EchoRequest
	var reply EchoResponse
	var err error

	// EchoService.Echo
	args.Msg = "Hello, Protobuf-RPC"
	if err = client.Call("EchoService.Echo", &args, &reply); err != nil {
		t.Fatalf(`echo.Echo: %v`, err)
	}
	if reply.Msg != args.Msg {
		t.Fatalf(`echo.Echo: expected = "%s", got = "%s"`, args.Msg, reply.Msg)
	}
}

func testArithStub(t *testing.T, stub *ArithServiceClient) {
	var args ArithRequest
	var reply *ArithResponse
	var err error

	// Add
	args.A = 1
	args.B = 2
	if reply, err = stub.Add(&args); err != nil {
		t.Fatalf(`stub.Add: %v`, err)
	}
	if reply.C != 3 {
		t.Fatalf(`stub.Add: expected = %d, got = %d`, 3, reply.C)
	}

	// Mul
	args.A = 2
	args.B = 3
	if reply, err = stub.Mul(&args); err != nil {
		t.Fatalf(`stub.Mul: %v`, err)
	}
	if reply.C != 6 {
		t.Fatalf(`stub.Mul: expected = %d, got = %d`, 6, reply.C)
	}

	// Div
	args.A = 13
	args.B = 5
	if reply, err = stub.Div(&args); err != nil {
		t.Fatalf(`stub.Div: %v`, err)
	}
	if reply.C != 2 {
		t.Fatalf(`stub.Div: expected = %d, got = %d`, 2, reply.C)
	}

	// Div zero
	args.A = 1
	args.B = 0
	if reply, err = stub.Div(&args); err.Error() != "divide by zero" {
		t.Fatalf(`stub.Div: expected = "%s", got = "%s"`, "divide by zero", err.Error())
	}

	// Error
	args.A = 1
	args.B = 2
	if reply, err = stub.Error(&args); err.Error() != "ArithError" {
		t.Fatalf(`stub.Error: expected = "%s", got = "%s"`, "ArithError", err.Error())
	}
}
func testEchoStub(t *testing.T, stub *EchoServiceClient) {
	var args EchoRequest
	var reply *EchoResponse
	var err error

	// EchoService.Echo
	args.Msg = "Hello, Protobuf-RPC"
	if reply, err = stub.Echo(&args); err != nil {
		t.Fatalf(`stub.Echo: %v`, err)
	}
	if reply.Msg != args.Msg {
		t.Fatalf(`stub.Echo: expected = "%s", got = "%s"`, args.Msg, reply.Msg)
	}
}
