// Copyright 2013 <chaishushan{AT}gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package service

import (
	"fmt"
	"log"
	"net/rpc"
	"sync"
	"testing"
	"unicode/utf8"
)

var (
	echoHost = "127.0.0.1"
	echoPort = 2015

	echoRequest         = "Hello, new gopher!"
	echoResponse        = echoRequest + echoRequest
	echoMassiveRequest  = makeMassive("Hello, 世界.")
	echoMassiveResponse = echoMassiveRequest + echoMassiveRequest

	onceEcho sync.Once
)

func makeMassive(args string) string {
	runeLen := utf8.RuneCountInString(args)
	runeBuf := make([]rune, runeLen*1024*100)
	for i := 0; i < 1024*100; i++ {
		offset := i * runeLen
		j := 0
		for _, r := range args {
			runeBuf[offset+j] = r
			j++
		}
	}
	return string(runeBuf)
}

func setupEchoServer() {
	var wg sync.WaitGroup
	wg.Add(1)
	defer wg.Wait()

	go func() {
		wg.Done()

		addr := fmt.Sprintf("127.0.0.1:%d", echoPort)
		err := ListenAndServeEchoService("tcp", addr, new(Echo))
		if err != nil {
			log.Fatalf("ListenAndServeEchoService: %v", err)
		}
	}()
}

func TestEchoService(t *testing.T) {
	onceEcho.Do(setupEchoServer)

	addr := fmt.Sprintf("%s:%d", echoHost, echoPort)
	c, err := DialEchoService("tcp", addr)
	if err != nil {
		t.Fatalf(
			`net.Dial("tcp", "%s:%d"): %v`,
			echoHost, echoPort,
			err,
		)
	}
	defer c.Close()

	testEchoService(t, c.Client)
}

func testEchoService(t *testing.T, client *rpc.Client) {
	var args EchoRequest
	var reply EchoResponse
	var err error

	// EchoService.EchoTwice
	args.Msg = echoRequest
	err = client.Call("EchoService.EchoTwice", &args, &reply)
	if err != nil {
		t.Fatalf(`EchoService.EchoTwice: %v`, err)
	}
	if reply.Msg != echoResponse {
		t.Fatalf(
			`EchoService.EchoTwice: expected = "%s", got = "%s"`,
			echoResponse, reply.Msg,
		)
	}

	// EchoService.EchoTwice (Massive)
	args.Msg = echoMassiveRequest
	err = client.Call("EchoService.EchoTwice", &args, &reply)
	if err != nil {
		t.Fatalf(`EchoService.EchoTwice: %v`, err)
	}
	if reply.Msg != echoMassiveResponse {
		got := reply.Msg
		if len(got) > 8 {
			got = got[:8] + "..."
		}
		t.Fatalf(`EchoService.EchoTwice: len = %d, got = %v`,
			len(reply.Msg), got,
		)
	}
}

func TestClientSyncEcho(t *testing.T) {
	onceEcho.Do(setupEchoServer)

	addr := fmt.Sprintf("%s:%d", echoHost, echoPort)
	echoClient, err := DialEchoService("tcp", addr)
	if err != nil {
		t.Fatalf(
			`net.Dial("tcp", "%s:%d"): %v`,
			echoHost, echoPort,
			err,
		)
	}
	defer echoClient.Close()

	var args EchoRequest
	var reply *EchoResponse

	// EchoService.EchoTwice
	args.Msg = "abc"
	reply, err = echoClient.EchoTwice(&args)
	if err != nil {
		t.Fatalf(`EchoService.EchoTwice: %v`, err)
	}
	if reply.Msg != args.Msg+args.Msg {
		t.Fatalf(
			`EchoService.EchoTwice: expected = "%s", got = "%s"`,
			args.Msg+args.Msg, reply.Msg,
		)
	}

	// EchoService.EchoTwice
	args.Msg = "你好, 世界"
	reply, err = echoClient.EchoTwice(&args)
	if err != nil {
		t.Fatalf(`EchoService.EchoTwice: %v`, err)
	}
	if reply.Msg != args.Msg+args.Msg {
		t.Fatalf(
			`EchoService.EchoTwice: expected = "%s", got = "%s"`,
			args.Msg+args.Msg, reply.Msg,
		)
	}
}

func TestClientSyncMassive(t *testing.T) {
	onceEcho.Do(setupEchoServer)

	addr := fmt.Sprintf("%s:%d", echoHost, echoPort)
	echoClient, err := DialEchoService("tcp", addr)
	if err != nil {
		t.Fatalf(
			`net.Dial("tcp", "%s:%d"): %v`,
			echoHost, echoPort,
			err,
		)
	}
	defer echoClient.Close()

	var args EchoRequest
	var reply *EchoResponse

	// EchoService.EchoTwice
	args.Msg = echoMassiveRequest + "abc"
	reply, err = echoClient.EchoTwice(&args)
	if err != nil {
		t.Fatalf(`EchoService.EchoTwice: %v`, err)
	}
	if reply.Msg != args.Msg+args.Msg {
		got := reply.Msg
		if len(got) > 8 {
			got = got[:8] + "..."
		}
		t.Fatalf(`EchoService.EchoTwice: len = %d, got = %v`,
			len(reply.Msg), got,
		)
	}

	// EchoService.EchoTwice
	args.Msg = echoMassiveRequest + "你好, 世界"
	reply, err = echoClient.EchoTwice(&args)
	if err != nil {
		t.Fatalf(`EchoService.EchoTwice: %v`, err)
	}
	if reply.Msg != args.Msg+args.Msg {
		got := reply.Msg
		if len(got) > 8 {
			got = got[:8] + "..."
		}
		t.Fatalf(`EchoService.EchoTwice: len = %d, got = %v`,
			len(reply.Msg), got,
		)
	}
}

func TestClientAsyncEcho(t *testing.T) {
	onceEcho.Do(setupEchoServer)

	addr := fmt.Sprintf("%s:%d", echoHost, echoPort)
	client, err := DialEchoService("tcp", addr)
	if err != nil {
		t.Fatalf(
			`net.Dial("tcp", "%s:%d"): %v`,
			echoHost, echoPort,
			err,
		)
	}
	defer client.Close()

	var args EchoRequest
	var reply EchoResponse

	// EchoService.EchoTwice
	args.Msg = echoRequest
	call := client.Go("EchoService.EchoTwice", &args, &reply, nil)

	call = <-call.Done
	if call.Error != nil {
		t.Fatalf(`EchoService.EchoTwice: %v`, call.Error)
	}
	if call.Reply.(*EchoResponse).Msg != echoResponse {
		t.Fatalf(
			`EchoService.EchoTwice: expected = "%s", got = "%s"`,
			echoResponse, call.Reply.(*EchoResponse).Msg,
		)
	}
}

func TestClientAsyncEchoBatches(t *testing.T) {
	onceEcho.Do(setupEchoServer)

	addr := fmt.Sprintf("%s:%d", echoHost, echoPort)
	client, err := DialEchoService("tcp", addr)
	if err != nil {
		t.Fatalf(
			`net.Dial("tcp", "%s:%d"): %v`,
			echoHost, echoPort,
			err,
		)
	}
	defer client.Close()

	var args1 EchoRequest
	var reply1 EchoResponse
	var args2 EchoRequest
	var reply2 EchoResponse
	var args3 EchoRequest
	var reply3 EchoResponse

	// EchoService.EchoTwice
	args1.Msg = "abc"
	call1 := client.Go("EchoService.EchoTwice", &args1, &reply1, nil)
	args2.Msg = "你好, 世界"
	call2 := client.Go("EchoService.EchoTwice", &args2, &reply2, nil)
	args3.Msg = "Hello, 世界"
	call3 := client.Go("EchoService.EchoTwice", &args3, &reply3, nil)

	call1 = <-call1.Done
	call2 = <-call2.Done
	call3 = <-call3.Done

	// call1
	if call1.Error != nil {
		t.Fatalf(`EchoService.EchoTwice: %v`, call1.Error)
	}
	if call1.Reply.(*EchoResponse).Msg != args1.Msg+args1.Msg {
		t.Fatalf(
			`EchoService.EchoTwice: expected = "%s", got = "%s"`,
			args1.Msg+args1.Msg,
			call1.Reply.(*EchoResponse).Msg,
		)
	}

	// call2
	if call2.Error != nil {
		t.Fatalf(`EchoService.EchoTwice: %v`, call2.Error)
	}
	if call2.Reply.(*EchoResponse).Msg != args2.Msg+args2.Msg {
		t.Fatalf(
			`EchoService.EchoTwice: expected = "%s", got = "%s"`,
			args2.Msg+args2.Msg,
			call2.Reply.(*EchoResponse).Msg,
		)
	}

	// call3
	if call3.Error != nil {
		t.Fatalf(`EchoService.EchoTwice: %v`, call3.Error)
	}
	if call3.Reply.(*EchoResponse).Msg != args3.Msg+args3.Msg {
		t.Fatalf(
			`EchoService.EchoTwice: expected = "%s", got = "%s"`,
			args3.Msg+args3.Msg,
			call3.Reply.(*EchoResponse).Msg,
		)
	}
}

func TestClientAsyncMassive(t *testing.T) {
	onceEcho.Do(setupEchoServer)

	addr := fmt.Sprintf("%s:%d", echoHost, echoPort)
	client, err := DialEchoService("tcp", addr)
	if err != nil {
		t.Fatalf(
			`net.Dial("tcp", "%s:%d"): %v`,
			echoHost, echoPort,
			err,
		)
	}
	defer client.Close()

	var args EchoRequest
	var reply EchoResponse

	// EchoService.EchoTwice
	args.Msg = echoMassiveRequest
	call := client.Go("EchoService.EchoTwice", &args, &reply, nil)

	call = <-call.Done
	if call.Error != nil {
		t.Fatalf(`EchoService.EchoTwice: %v`, call.Error)
	}
	if call.Reply.(*EchoResponse).Msg != echoMassiveResponse {
		got := call.Reply.(*EchoResponse).Msg
		if len(got) > 8 {
			got = got[:8] + "..."
		}
		t.Fatalf(`EchoService.EchoTwice: len = %d, got = %v`,
			len(call.Reply.(*EchoResponse).Msg), got,
		)
	}
}

func TestClientAsyncMassiveBatches(t *testing.T) {
	onceEcho.Do(setupEchoServer)

	addr := fmt.Sprintf("%s:%d", echoHost, echoPort)
	client, err := DialEchoService("tcp", addr)
	if err != nil {
		t.Fatalf(
			`net.Dial("tcp", "%s:%d"): %v`,
			echoHost, echoPort,
			err,
		)
	}
	defer client.Close()

	var args1 EchoRequest
	var reply1 EchoResponse
	var args2 EchoRequest
	var reply2 EchoResponse
	var args3 EchoRequest
	var reply3 EchoResponse

	// EchoService.EchoTwice
	args1.Msg = echoMassiveRequest + "abc"
	call1 := client.Go("EchoService.EchoTwice", &args1, &reply1, nil)
	args2.Msg = echoMassiveRequest + "你好, 世界"
	call2 := client.Go("EchoService.EchoTwice", &args2, &reply2, nil)
	args3.Msg = echoMassiveRequest + "Hello, 世界"
	call3 := client.Go("EchoService.EchoTwice", &args3, &reply3, nil)

	call1 = <-call1.Done
	call2 = <-call2.Done
	call3 = <-call3.Done

	// call1
	if call1.Error != nil {
		t.Fatalf(`EchoService.EchoTwice: %v`, call1.Error)
	}
	if call1.Reply.(*EchoResponse).Msg != args1.Msg+args1.Msg {
		got := call1.Reply.(*EchoResponse).Msg
		if len(got) > 8 {
			got = got[:8] + "..."
		}
		t.Fatalf(`EchoService.EchoTwice: len = %d, got = %v`,
			len(call1.Reply.(*EchoResponse).Msg), got,
		)
	}

	// call2
	if call2.Error != nil {
		t.Fatalf(`EchoService.EchoTwice: %v`, call2.Error)
	}
	if call2.Reply.(*EchoResponse).Msg != args2.Msg+args2.Msg {
		got := call2.Reply.(*EchoResponse).Msg
		if len(got) > 8 {
			got = got[:8] + "..."
		}
		t.Fatalf(`EchoService.EchoTwice: len = %d, got = %v`,
			len(call2.Reply.(*EchoResponse).Msg), got,
		)
	}

	// call3
	if call3.Error != nil {
		t.Fatalf(`EchoService.EchoTwice: %v`, call3.Error)
	}
	if call3.Reply.(*EchoResponse).Msg != args3.Msg+args3.Msg {
		got := call3.Reply.(*EchoResponse).Msg
		if len(got) > 8 {
			got = got[:8] + "..."
		}
		t.Fatalf(`EchoService.EchoTwice: len = %d, got = %v`,
			len(call3.Reply.(*EchoResponse).Msg), got,
		)
	}
}

func BenchmarkSyncEcho(b *testing.B) {
	onceEcho.Do(setupEchoServer)

	addr := fmt.Sprintf("%s:%d", echoHost, echoPort)
	echoClient, err := DialEchoService("tcp", addr)
	if err != nil {
		b.Fatalf(
			`net.Dial("tcp", "%s:%d"): %v`,
			echoHost, echoPort,
			err,
		)
	}
	defer echoClient.Close()

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		var args EchoRequest
		var reply *EchoResponse

		// EchoService.EchoTwice
		args.Msg = "abc"
		reply, err = echoClient.EchoTwice(&args)
		if err != nil {
			b.Fatalf(`EchoService.EchoTwice: %v`, err)
		}
		if reply.Msg != args.Msg+args.Msg {
			b.Fatalf(
				`EchoService.EchoTwice: expected = "%s", got = "%s"`,
				args.Msg+args.Msg, reply.Msg,
			)
		}

		// EchoService.EchoTwice
		args.Msg = "你好, 世界"
		reply, err = echoClient.EchoTwice(&args)
		if err != nil {
			b.Fatalf(`EchoService.EchoTwice: %v`, err)
		}
		if reply.Msg != args.Msg+args.Msg {
			b.Fatalf(
				`EchoService.EchoTwice: expected = "%s", got = "%s"`,
				args.Msg+args.Msg, reply.Msg,
			)
		}

		// EchoService.EchoTwice
		args.Msg = "Hello, 世界"
		reply, err = echoClient.EchoTwice(&args)
		if err != nil {
			b.Fatalf(`EchoService.EchoTwice: %v`, err)
		}
		if reply.Msg != args.Msg+args.Msg {
			b.Fatalf(
				`EchoService.EchoTwice: expected = "%s", got = "%s"`,
				args.Msg+args.Msg, reply.Msg,
			)
		}
	}
}

func BenchmarkSyncMassive(b *testing.B) {
	onceEcho.Do(setupEchoServer)

	addr := fmt.Sprintf("%s:%d", echoHost, echoPort)
	echoClient, err := DialEchoService("tcp", addr)
	if err != nil {
		b.Fatalf(
			`net.Dial("tcp", "%s:%d"): %v`,
			echoHost, echoPort,
			err,
		)
	}
	defer echoClient.Close()

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		var args EchoRequest
		var reply *EchoResponse

		// EchoService.EchoTwice
		args.Msg = echoMassiveRequest + "abc"
		reply, err = echoClient.EchoTwice(&args)
		if err != nil {
			b.Fatalf(`EchoService.EchoTwice: %v`, err)
		}
		if reply.Msg != args.Msg+args.Msg {
			got := reply.Msg
			if len(got) > 8 {
				got = got[:8] + "..."
			}
			b.Fatalf(`EchoService.EchoTwice: len = %d, got = %v`,
				len(reply.Msg), got,
			)
		}

		// EchoService.EchoTwice
		args.Msg = echoMassiveRequest + "你好, 世界"
		reply, err = echoClient.EchoTwice(&args)
		if err != nil {
			b.Fatalf(`EchoService.EchoTwice: %v`, err)
		}
		if reply.Msg != args.Msg+args.Msg {
			got := reply.Msg
			if len(got) > 8 {
				got = got[:8] + "..."
			}
			b.Fatalf(`EchoService.EchoTwice: len = %d, got = %v`,
				len(reply.Msg), got,
			)
		}

		// EchoService.EchoTwice
		args.Msg = echoMassiveRequest + "Hello, 世界"
		reply, err = echoClient.EchoTwice(&args)
		if err != nil {
			b.Fatalf(`EchoService.EchoTwice: %v`, err)
		}
		if reply.Msg != args.Msg+args.Msg {
			got := reply.Msg
			if len(got) > 8 {
				got = got[:8] + "..."
			}
			b.Fatalf(`EchoService.EchoTwice: len = %d, got = %v`,
				len(reply.Msg), got,
			)
		}
	}
}

func BenchmarkAsyncEcho(b *testing.B) {
	onceEcho.Do(setupEchoServer)

	addr := fmt.Sprintf("%s:%d", echoHost, echoPort)
	client, err := DialEchoService("tcp", addr)
	if err != nil {
		b.Fatalf(
			`net.Dial("tcp", "%s:%d"): %v`,
			echoHost, echoPort,
			err,
		)
	}
	defer client.Close()

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		var args1 EchoRequest
		var reply1 EchoResponse
		var args2 EchoRequest
		var reply2 EchoResponse
		var args3 EchoRequest
		var reply3 EchoResponse

		// EchoService.EchoTwice
		args1.Msg = "abc"
		call1 := client.Go("EchoService.EchoTwice", &args1, &reply1, nil)
		args2.Msg = "你好, 世界"
		call2 := client.Go("EchoService.EchoTwice", &args2, &reply2, nil)
		args3.Msg = "Hello, 世界"
		call3 := client.Go("EchoService.EchoTwice", &args3, &reply3, nil)

		call1 = <-call1.Done
		call2 = <-call2.Done
		call3 = <-call3.Done

		// call1
		if call1.Error != nil {
			b.Fatalf(`EchoService.EchoTwice: %v`, call1.Error)
		}
		if call1.Reply.(*EchoResponse).Msg != args1.Msg+args1.Msg {
			b.Fatalf(
				`EchoService.EchoTwice: expected = "%s", got = "%s"`,
				args1.Msg+args1.Msg,
				call1.Reply.(*EchoResponse).Msg,
			)
		}

		// call2
		if call2.Error != nil {
			b.Fatalf(`EchoService.EchoTwice: %v`, call2.Error)
		}
		if call2.Reply.(*EchoResponse).Msg != args2.Msg+args2.Msg {
			b.Fatalf(
				`EchoService.EchoTwice: expected = "%s", got = "%s"`,
				args2.Msg+args2.Msg,
				call2.Reply.(*EchoResponse).Msg,
			)
		}

		// call3
		if call3.Error != nil {
			b.Fatalf(`EchoService.EchoTwice: %v`, call3.Error)
		}
		if call3.Reply.(*EchoResponse).Msg != args3.Msg+args3.Msg {
			b.Fatalf(
				`EchoService.EchoTwice: expected = "%s", got = "%s"`,
				args3.Msg+args3.Msg,
				call3.Reply.(*EchoResponse).Msg,
			)
		}
	}
}

func BenchmarkAsyncMassive(b *testing.B) {
	onceEcho.Do(setupEchoServer)

	addr := fmt.Sprintf("%s:%d", echoHost, echoPort)
	client, err := DialEchoService("tcp", addr)
	if err != nil {
		b.Fatalf(
			`net.Dial("tcp", "%s:%d"): %v`,
			echoHost, echoPort,
			err,
		)
	}
	defer client.Close()

	b.ResetTimer()
	for i := 0; i < b.N; i++ {
		var args1 EchoRequest
		var reply1 EchoResponse
		var args2 EchoRequest
		var reply2 EchoResponse
		var args3 EchoRequest
		var reply3 EchoResponse

		// EchoService.EchoTwice
		args1.Msg = echoMassiveRequest + "abc"
		call1 := client.Go("EchoService.EchoTwice", &args1, &reply1, nil)
		args2.Msg = echoMassiveRequest + "你好, 世界"
		call2 := client.Go("EchoService.EchoTwice", &args2, &reply2, nil)
		args3.Msg = echoMassiveRequest + "Hello, 世界"
		call3 := client.Go("EchoService.EchoTwice", &args3, &reply3, nil)

		call1 = <-call1.Done
		call2 = <-call2.Done
		call3 = <-call3.Done

		// call1
		if call1.Error != nil {
			b.Fatalf(`EchoService.EchoTwice: %v`, call1.Error)
		}
		if call1.Reply.(*EchoResponse).Msg != args1.Msg+args1.Msg {
			got := call1.Reply.(*EchoResponse).Msg
			if len(got) > 8 {
				got = got[:8] + "..."
			}
			b.Fatalf(`EchoService.EchoTwice: len = %d, got = %v`,
				len(call1.Reply.(*EchoResponse).Msg), got,
			)
		}

		// call2
		if call2.Error != nil {
			b.Fatalf(`EchoService.EchoTwice: %v`, call2.Error)
		}
		if call2.Reply.(*EchoResponse).Msg != args2.Msg+args2.Msg {
			got := call2.Reply.(*EchoResponse).Msg
			if len(got) > 8 {
				got = got[:8] + "..."
			}
			b.Fatalf(`EchoService.EchoTwice: len = %d, got = %v`,
				len(call2.Reply.(*EchoResponse).Msg), got,
			)
		}

		// call3
		if call3.Error != nil {
			b.Fatalf(`EchoService.EchoTwice: %v`, call3.Error)
		}
		if call3.Reply.(*EchoResponse).Msg != args3.Msg+args3.Msg {
			got := call3.Reply.(*EchoResponse).Msg
			if len(got) > 8 {
				got = got[:8] + "..."
			}
			b.Fatalf(`EchoService.EchoTwice: len = %d, got = %v`,
				len(call3.Reply.(*EchoResponse).Msg), got,
			)
		}
	}
}
