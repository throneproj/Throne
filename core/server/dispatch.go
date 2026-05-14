package main

import (
	"ThroneCore/gen"
	"context"
	"encoding/binary"
	"fmt"
	"io"
	"log"
	"net"
	"sync"

	"google.golang.org/protobuf/proto"
)

var globalServer = &server{}

// runDispatch reads request frames and dispatches each one to its own goroutine.
// Responses are written back with the matching reqId so the GUI can demultiplex.
//
// Request wire format (little-endian):
//
//	[uint32 reqId][uint16 methodLen][method bytes][uint32 payloadLen][payload]
//
// Response wire format (little-endian):
//
//	[uint32 reqId][uint8 status][uint32 dataLen][data]
func runDispatch(conn net.Conn) {
	defer func() {
		conn.Close()
		log.Fatal("IPC connection dropped, exiting")
	}()

	var writeMu sync.Mutex

	writeResponse := func(reqId uint32, status uint8, data []byte) {
		var header [9]byte
		binary.LittleEndian.PutUint32(header[0:], reqId)
		header[4] = status
		binary.LittleEndian.PutUint32(header[5:], uint32(len(data)))

		writeMu.Lock()
		defer writeMu.Unlock()
		if _, err := conn.Write(header[:]); err != nil {
			return
		}
		if len(data) > 0 {
			conn.Write(data) //nolint:errcheck — connection error caught on next read
		}
	}

	for {
		// Read reqId (uint32 LE)
		var reqId uint32
		if err := binary.Read(conn, binary.LittleEndian, &reqId); err != nil {
			return
		}

		// Read method name length (uint16 LE)
		var methodLen uint16
		if err := binary.Read(conn, binary.LittleEndian, &methodLen); err != nil {
			return
		}

		// Read method name
		methodBytes := make([]byte, methodLen)
		if _, err := io.ReadFull(conn, methodBytes); err != nil {
			return
		}

		// Read payload length (uint32 LE)
		var payloadLen uint32
		if err := binary.Read(conn, binary.LittleEndian, &payloadLen); err != nil {
			return
		}

		// Read payload
		payload := make([]byte, payloadLen)
		if _, err := io.ReadFull(conn, payload); err != nil {
			return
		}

		// Dispatch concurrently so long-running calls don't block the reader.
		go func(id uint32, method string, pl []byte) {
			respData, dispatchErr := dispatch(method, pl)
			if dispatchErr != nil {
				writeResponse(id, 1, []byte(dispatchErr.Error()))
			} else {
				writeResponse(id, 0, respData)
			}
		}(reqId, string(methodBytes), payload)
	}
}

func dispatch(methodName string, payload []byte) ([]byte, error) {
	ctx := context.Background()
	s := globalServer

	switch methodName {
	case "Start":
		req := &gen.LoadConfigReq{}
		if err := proto.Unmarshal(payload, req); err != nil {
			return nil, err
		}
		resp, err := s.Start(ctx, req)
		if err != nil {
			return nil, err
		}
		return proto.Marshal(resp)

	case "Stop":
		req := &gen.EmptyReq{}
		if err := proto.Unmarshal(payload, req); err != nil {
			return nil, err
		}
		resp, err := s.Stop(ctx, req)
		if err != nil {
			return nil, err
		}
		return proto.Marshal(resp)

	case "CheckConfig":
		req := &gen.LoadConfigReq{}
		if err := proto.Unmarshal(payload, req); err != nil {
			return nil, err
		}
		resp, err := s.CheckConfig(ctx, req)
		if err != nil {
			return nil, err
		}
		return proto.Marshal(resp)

	case "Test":
		req := &gen.TestReq{}
		if err := proto.Unmarshal(payload, req); err != nil {
			return nil, err
		}
		resp, err := s.Test(ctx, req)
		if err != nil {
			return nil, err
		}
		return proto.Marshal(resp)

	case "StopTest":
		req := &gen.EmptyReq{}
		if err := proto.Unmarshal(payload, req); err != nil {
			return nil, err
		}
		resp, err := s.StopTest(ctx, req)
		if err != nil {
			return nil, err
		}
		return proto.Marshal(resp)

	case "QueryURLTest":
		req := &gen.EmptyReq{}
		if err := proto.Unmarshal(payload, req); err != nil {
			return nil, err
		}
		resp, err := s.QueryURLTest(ctx, req)
		if err != nil {
			return nil, err
		}
		return proto.Marshal(resp)

	case "IPTest":
		req := &gen.IPTestRequest{}
		if err := proto.Unmarshal(payload, req); err != nil {
			return nil, err
		}
		resp, err := s.IPTest(ctx, req)
		if err != nil {
			return nil, err
		}
		return proto.Marshal(resp)

	case "QueryIPTest":
		req := &gen.EmptyReq{}
		if err := proto.Unmarshal(payload, req); err != nil {
			return nil, err
		}
		resp, err := s.QueryIPTest(ctx, req)
		if err != nil {
			return nil, err
		}
		return proto.Marshal(resp)

	case "QueryStats":
		req := &gen.EmptyReq{}
		if err := proto.Unmarshal(payload, req); err != nil {
			return nil, err
		}
		resp, err := s.QueryStats(ctx, req)
		if err != nil {
			return nil, err
		}
		return proto.Marshal(resp)

	case "ListConnections":
		req := &gen.EmptyReq{}
		if err := proto.Unmarshal(payload, req); err != nil {
			return nil, err
		}
		resp, err := s.ListConnections(ctx, req)
		if err != nil {
			return nil, err
		}
		return proto.Marshal(resp)

	case "IsPrivileged":
		req := &gen.EmptyReq{}
		if err := proto.Unmarshal(payload, req); err != nil {
			return nil, err
		}
		resp, err := s.IsPrivileged(ctx, req)
		if err != nil {
			return nil, err
		}
		return proto.Marshal(resp)

	case "SetSystemDNS":
		req := &gen.SetSystemDNSRequest{}
		if err := proto.Unmarshal(payload, req); err != nil {
			return nil, err
		}
		resp, err := s.SetSystemDNS(ctx, req)
		if err != nil {
			return nil, err
		}
		return proto.Marshal(resp)

	case "SpeedTest":
		req := &gen.SpeedTestRequest{}
		if err := proto.Unmarshal(payload, req); err != nil {
			return nil, err
		}
		resp, err := s.SpeedTest(ctx, req)
		if err != nil {
			return nil, err
		}
		return proto.Marshal(resp)

	case "QuerySpeedTest":
		req := &gen.EmptyReq{}
		if err := proto.Unmarshal(payload, req); err != nil {
			return nil, err
		}
		resp, err := s.QuerySpeedTest(ctx, req)
		if err != nil {
			return nil, err
		}
		return proto.Marshal(resp)

	case "QueryCountryTest":
		req := &gen.EmptyReq{}
		if err := proto.Unmarshal(payload, req); err != nil {
			return nil, err
		}
		resp, err := s.QueryCountryTest(ctx, req)
		if err != nil {
			return nil, err
		}
		return proto.Marshal(resp)

	case "GenWgKeyPair":
		req := &gen.EmptyReq{}
		if err := proto.Unmarshal(payload, req); err != nil {
			return nil, err
		}
		resp, err := s.GenWgKeyPair(ctx, req)
		if err != nil {
			return nil, err
		}
		return proto.Marshal(resp)

	default:
		return nil, fmt.Errorf("unknown method: %s", methodName)
	}
}
