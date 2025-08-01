# Copyright 2013 <chaishushan{AT}gmail.com>. All rights reserved.
# Use of this source code is governed by a BSD-style
# license that can be found in the LICENSE file.

PROTO_FILES=$(sort $(wildcard ./*.proto))

default: $(PROTO_FILES) Makefile
	go install github.com/golang/protobuf/protoc-gen-go
	go install github.com/chai2010/protorpc/protoc-gen-protorpc
	protoc --go_out=. ${PROTO_FILES}
	ENV_PROTOC_GEN_PROTORPC_FLAG_PREFIX= protoc --protorpc_out=. ${PROTO_FILES}
	go test

clean:
