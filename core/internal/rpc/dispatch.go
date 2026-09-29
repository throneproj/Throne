package rpc

import (
	"context"
	"encoding/binary"
	"fmt"
	"io"
	"log"
	"net"
	runtimeDebug "runtime/debug"
	"sync"

	"ThroneCore/gen"

	"google.golang.org/protobuf/proto"
)

type server struct {
	gen.UnimplementedLibcoreServiceServer
}

var globalServer = &server{}

// Set once by Serve, before the first request can reach a handler.
var debugLog bool

// Serve answers framed requests off conn until the peer goes away, then closes conn; what a dropped peer means is the caller's policy.
// Little-endian frames: req [uint32 reqId][uint16 methodLen][method][uint32 payloadLen][payload], resp [uint32 reqId][uint8 status][uint32 dataLen][data].
func Serve(conn net.Conn, debug bool) {
	debugLog = debug
	defer conn.Close()

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
		var reqId uint32
		if err := binary.Read(conn, binary.LittleEndian, &reqId); err != nil {
			return
		}

		var methodLen uint16
		if err := binary.Read(conn, binary.LittleEndian, &methodLen); err != nil {
			return
		}

		methodBytes := make([]byte, methodLen)
		if _, err := io.ReadFull(conn, methodBytes); err != nil {
			return
		}

		var payloadLen uint32
		if err := binary.Read(conn, binary.LittleEndian, &payloadLen); err != nil {
			return
		}

		payload := make([]byte, payloadLen)
		if _, err := io.ReadFull(conn, payload); err != nil {
			return
		}

		go func(id uint32, method string, pl []byte) {
			// main()'s recover covers only the main goroutine, so without this one bad request takes the core down.
			defer func() {
				if r := recover(); r != nil {
					log.Printf("panic in %s: %v\n%s", method, r, runtimeDebug.Stack())
					writeResponse(id, 1, []byte(fmt.Sprintf("core panic in %s: %v", method, r)))
				}
			}()
			respData, dispatchErr := dispatch(method, pl)
			if dispatchErr != nil {
				writeResponse(id, 1, []byte(dispatchErr.Error()))
			} else {
				writeResponse(id, 0, respData)
			}
		}(reqId, string(methodBytes), payload)
	}
}

type handlerFn func(context.Context, []byte) ([]byte, error)

// PReq pins Req to the pointer type the generated code implements proto.Message on.
func handle[Req any, PReq interface {
	*Req
	proto.Message
}, Resp proto.Message](fn func(context.Context, PReq) (Resp, error)) handlerFn {
	return func(ctx context.Context, payload []byte) ([]byte, error) {
		req := PReq(new(Req))
		if err := proto.Unmarshal(payload, req); err != nil {
			return nil, err
		}
		resp, err := fn(ctx, req)
		if err != nil {
			return nil, err
		}
		return proto.Marshal(resp)
	}
}

var handlers = map[string]handlerFn{
	"Start":               handle(globalServer.Start),
	"Stop":                handle(globalServer.Stop),
	"CheckConfig":         handle(globalServer.CheckConfig),
	"Test":                handle(globalServer.Test),
	"StopTest":            handle(globalServer.StopTest),
	"QueryURLTest":        handle(globalServer.QueryURLTest),
	"IPTest":              handle(globalServer.IPTest),
	"QueryIPTest":         handle(globalServer.QueryIPTest),
	"QueryStats":          handle(globalServer.QueryStats),
	"QueryConnections":    handle(globalServer.QueryConnections),
	"CloseConnections":    handle(globalServer.CloseConnections),
	"QueryAutoSelectors":  handle(globalServer.QueryAutoSelectors),
	"AutoSelectorAction":  handle(globalServer.AutoSelectorAction),
	"IsPrivileged":        handle(globalServer.IsPrivileged),
	"SetSystemDNS":        handle(globalServer.SetSystemDNS),
	"GetDefaultInterface": handle(globalServer.GetDefaultInterface),
	"SpeedTest":           handle(globalServer.SpeedTest),
	"QuerySpeedTest":      handle(globalServer.QuerySpeedTest),
	"QueryCountryTest":    handle(globalServer.QueryCountryTest),
	"GenWgKeyPair":        handle(globalServer.GenWgKeyPair),
	"WarpRegister":        handle(globalServer.WarpRegister),
	"InstallDashboard":    handle(globalServer.InstallDashboard),
	"QueryVPNStatus":      handle(globalServer.QueryVPNStatus),
	"SubmitVPNChallenge":  handle(globalServer.SubmitVPNChallenge),
	"CancelVPNChallenge":  handle(globalServer.CancelVPNChallenge),
	"CaptureDiagnostics":  handle(globalServer.CaptureDiagnostics),
	"StopDiagnostics":     handle(globalServer.StopDiagnostics),
	"UpdateRuleSets":      handle(globalServer.UpdateRuleSets),
}

func dispatch(methodName string, payload []byte) ([]byte, error) {
	h, found := handlers[methodName]
	if !found {
		return nil, fmt.Errorf("unknown method: %s", methodName)
	}
	return h(context.Background(), payload)
}

// The wire path goes through the table above, not gRPC; keep the interface honest.
var _ gen.LibcoreServiceServer = globalServer
