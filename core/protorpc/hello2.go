// Copyright 2014 <chaishushan{AT}gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// +build ingore

package main

import (
	"fmt"
	"log"
	"net/rpc"
	"time"

	stdrpc "github.com/chai2010/protorpc/examples/stdrpc.pb"
)

type Echo int

func (t *Echo) Echo(args *stdrpc.EchoRequest, reply *stdrpc.EchoResponse) error {
	reply.Msg = args.Msg
	return nil
}

func (t *Echo) EchoTwice(args *stdrpc.EchoRequest, reply *stdrpc.EchoResponse) error {
	reply.Msg = args.Msg + args.Msg
	return nil
}

func init() {
	go stdrpc.ListenAndServeEchoService("tcp", `127.0.0.1:9527`, new(Echo))
}

func main() {
	time.Sleep(time.Second)

	echoClient, err := stdrpc.DialEchoService("tcp", `127.0.0.1:9527`)
	if err != nil {
		log.Fatalf("stdrpc.DialEchoService: %v", err)
	}
	defer echoClient.Close()

	args := &stdrpc.EchoRequest{Msg: "你好, 世界!"}
	reply, err := echoClient.EchoTwice(args)
	if err != nil {
		log.Fatalf("echoClient.EchoTwice: %v", err)
	}
	fmt.Println(reply.Msg)

	// or use normal client
	client, err := rpc.Dial("tcp", `127.0.0.1:9527`)
	if err != nil {
		log.Fatalf("rpc.Dial: %v", err)
	}
	defer client.Close()

	echoClient1 := &stdrpc.EchoServiceClient{client}
	echoClient2 := &stdrpc.EchoServiceClient{client}
	reply, err = echoClient1.EchoTwice(args)
	reply, err = echoClient2.EchoTwice(args)
	_, _ = reply, err

	// Output:
	// 你好, 世界!你好, 世界!
}
