package rpc

import (
	"context"
	"log"
	"path/filepath"
	"runtime"
	"strings"
	"time"

	"ThroneCore/gen"
	"ThroneCore/internal/boxmain"
	"ThroneCore/internal/proclist"
	"ThroneCore/internal/routetest"

	"github.com/sagernet/sing-box/option"
)

func (s *server) TestRoute(ctx context.Context, in *gen.TestRouteRequest) (*gen.TestRouteResponse, error) {
	ctx, cancel := context.WithTimeout(ctx, routetest.Timeout)
	defer cancel()
	query, err := routetest.Parse(routetest.Request{
		Process:  in.GetProcess(),
		Domain:   in.GetDomain(),
		IP:       in.GetIp(),
		Port:     in.GetPort(),
		Network:  in.GetNetwork(),
		Protocol: in.GetProtocol(),
	})
	if err != nil {
		return &gen.TestRouteResponse{Error: To(err.Error())}, nil
	}
	if owner := query.Metadata.ProcessInfo; owner != nil {
		owner.ProcessPaths = routeTestProcessPaths(owner.ProcessPaths[0])
	}
	var env *routetest.Env
	if in.GetConfig() != "" {
		env, err = startRouteTestBox(ctx, in.GetConfig())
	} else {
		env, err = runningRouteTestEnv()
	}
	if err != nil {
		return &gen.TestRouteResponse{Error: To(err.Error())}, nil
	}
	defer env.Close()
	return routeTestResultToProto(env.Test(ctx, query)), nil
}

// sing-box's Windows searcher reports a svchost-hosted service as its DLL followed by the svchost.exe hosting it.
func routeTestProcessPaths(process string) []string {
	if runtime.GOOS != "windows" || !strings.EqualFold(filepath.Ext(process), ".dll") {
		return []string{process}
	}
	entries, _ := proclist.List()
	var hostPID uint32
	for _, entry := range entries {
		if entry.Service != "" && (entry.Path == process || entry.Name == process) {
			hostPID = entry.PID
			break
		}
	}
	host := ""
	for _, entry := range entries {
		if entry.Service != "" || entry.Path == "" || !strings.EqualFold(entry.Name, "svchost.exe") {
			continue
		}
		if entry.PID == hostPID {
			host = entry.Path
			break
		}
		if host == "" {
			host = entry.Path
		}
	}
	if host == "" {
		return []string{process}
	}
	return []string{process, host}
}

func runningRouteTestEnv() (*routetest.Env, error) {
	box := currentBox()
	if box == nil {
		return nil, errInstanceNotRunning
	}
	return routetest.Running(box, connRules.routeRules(box)), nil
}

func startRouteTestBox(ctx context.Context, config string) (*routetest.Env, error) {
	keepCacheFile := func() bool { return currentBox() == nil }
	return routetest.StartStripped(ctx, keepCacheFile, func(strip func(*option.Options)) (routetest.Box, func(), error) {
		box, cancel, err := boxmain.Create([]byte(config), nil, func(options *option.Options) {
			strip(options)
			applyAutoRedirectMark(options, autoRedirectMark.Load())
		})
		if err != nil {
			return nil, nil, err
		}
		return box, func() { box.CloseWithTimeout(cancel, 2*time.Second, log.Println, true) }, nil
	})
}

// Sets each optional field only where the response always carried it: an error before the walk leaves out
// the destination and domain, and the empty step and response strings stay unset.
func routeTestResultToProto(result *routetest.Result) *gen.TestRouteResponse {
	response := &gen.TestRouteResponse{
		OutboundChain: result.OutboundChain,
		Skipped:       result.Skipped,
		Addresses:     result.Addresses,
		Inbound:       To(result.Inbound),
	}
	for _, step := range result.Steps {
		response.Steps = append(response.Steps, &gen.TestRouteStep{
			Index:         To(step.Index),
			Rule:          To(step.Rule),
			Action:        To(step.Action),
			ActionDetail:  To(step.ActionDetail),
			Matched:       To(step.Matched),
			Protocol:      routeTestOptional(step.Protocol),
			SniffedDomain: routeTestOptional(step.SniffedDomain),
			Addresses:     step.Addresses,
			Error:         routeTestOptional(step.Error),
			Note:          routeTestOptional(step.Note),
		})
	}
	if result.Error != "" {
		response.Error = To(result.Error)
		return response
	}
	response.Destination, response.Domain = To(result.Destination), To(result.Domain)
	if result.IsFinal {
		response.IsFinal = To(true)
	}
	response.Action = routeTestOptional(result.Action)
	response.Outbound = routeTestOptional(result.Outbound)
	return response
}

func routeTestOptional(value string) *string {
	if value == "" {
		return nil
	}
	return &value
}
