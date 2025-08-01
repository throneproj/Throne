// Copyright 2013 <chaishushan{AT}gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package service

import (
	"fmt"
	"log"
	"sync"
)

var (
	arithHost = "127.0.0.1"
	arithPort = 2010

	onceArith sync.Once
)

func setupArithServer() {
	var wg sync.WaitGroup
	wg.Add(1)
	defer wg.Wait()

	go func() {
		wg.Done()

		addr := fmt.Sprintf("127.0.0.1:%d", arithPort)
		err := ListenAndServeArithService("tcp", addr, new(Arith))
		if err != nil {
			log.Fatalf("ListenAndServeArithService: %v", err)
		}
	}()
}
