// Copyright 2014 <chaishushan{AT}gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

//go:generate protoc --go_out=. arith.proto echo.proto
//go:generate protoc --protorpc_out=. arith.proto echo.proto

package service
