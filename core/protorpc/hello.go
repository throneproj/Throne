// Copyright 2014 <chaishushan{AT}gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// +build ingore

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
