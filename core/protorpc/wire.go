// Copyright 2013 <chaishushan{AT}gmail.com>. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

package protorpc

import (
	"fmt"
	"io"

	wire "github.com/chai2010/protorpc/wire.pb"
	"github.com/golang/protobuf/proto"
)

func To[T any](v T) *T {
	return &v
}

func maxUint32(a, b uint32) uint32 {
	if a > b {
		return a
	}
	return b
}

func writeRequest(w io.Writer, id uint64, method string, request proto.Message) error {
	// marshal request
	pbRequest := []byte{}
	if request != nil {
		var err error
		pbRequest, err = proto.Marshal(request)
		if err != nil {
			return err
		}
	}

	// generate header
	header := &wire.RequestHeader{
		Id:                         To(id),
		Method:                     To(method),
		RawRequestLen:              To(uint32(len(pbRequest))),
	}

	// check header size
	pbHeader, err := proto.Marshal(header)
	if err != err {
		return err
	}
	if len(pbHeader) > int(wire.Const_MAX_REQUEST_HEADER_LEN) {
		return fmt.Errorf("protorpc.writeRequest: header larger than max_header_len: %d.", len(pbHeader))
	}

	// send header (more)
	if err := sendFrame(w, pbHeader); err != nil {
		return err
	}

	// send body (end)
	if err := sendFrame(w, pbRequest); err != nil {
		return err
	}

	return nil
}

func readRequestHeader(r io.Reader, header *wire.RequestHeader) (err error) {
	// recv header (more)
	pbHeader, err := recvFrame(r, int(wire.Const_MAX_REQUEST_HEADER_LEN))
	if err != nil {
		return err
	}

	// Marshal Header
	err = proto.Unmarshal(pbHeader, header)
	if err != nil {
		return err
	}

	return nil
}

func readRequestBody(r io.Reader, header *wire.RequestHeader, request proto.Message) error {
	// recv body (end)
	pbRequest, err := recvFrame(r, int(*header.RawRequestLen))
	if err != nil {
		return err
	}

	// Unmarshal to proto message
	if request != nil {
		err = proto.Unmarshal(pbRequest, request)
		if err != nil {
			return err
		}
	}

	return nil
}

func writeResponse(w io.Writer, id uint64, serr string, response proto.Message) (err error) {
	// clean response if error
	if serr != "" {
		response = nil
	}

	// marshal response
	pbResponse := []byte{}
	if response != nil {
		pbResponse, err = proto.Marshal(response)
		if err != nil {
			return err
		}
	}

	// generate header
	header := &wire.ResponseHeader{
		Id:                          To(id),
		Error:                       To(serr),
		RawResponseLen:              To(uint32(len(pbResponse))),
	}

	// check header size
	pbHeader, err := proto.Marshal(header)
	if err != err {
		return
	}

	// send header (more)
	if err = sendFrame(w, pbHeader); err != nil {
		return
	}

	// send body (end)
	if err = sendFrame(w, pbResponse); err != nil {
		return
	}

	return nil
}

func readResponseHeader(r io.Reader, header *wire.ResponseHeader) error {
	// recv header (more)
	pbHeader, err := recvFrame(r, 0)
	if err != nil {
		return err
	}

	// Marshal Header
	err = proto.Unmarshal(pbHeader, header)
	if err != nil {
		return err
	}

	return nil
}

func readResponseBody(r io.Reader, header *wire.ResponseHeader, response proto.Message) error {
	// recv body (end)
	pbResponse, err := recvFrame(r, int(*header.RawResponseLen))
	if err != nil {
		return err
	}

	// Unmarshal to proto message
	if response != nil {
		err = proto.Unmarshal(pbResponse, response)
		if err != nil {
			return err
		}
	}

	return nil
}
