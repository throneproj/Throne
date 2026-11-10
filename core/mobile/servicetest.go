package mobile

import (
	"encoding/json"
	"time"

	"ThroneCore/internal/probe"
	"ThroneCore/internal/servicecheck"

	E "github.com/sagernet/sing/common/exceptions"
)

// Service test only: limits the run to these check ids.
func (r *TestRequest) AddServiceCheck(id string) {
	r.serviceOnly = append(r.serviceOnly, id)
}

// resultJSON carries ServiceTestResult's checks and ip_info, keyed like libcore.proto.
type ServiceTestHandler interface {
	OnResult(tag string, resultJSON string, err string)
	OnDone()
}

type serviceOutcomeJSON struct {
	ID       string `json:"id"`
	Name     string `json:"name"`
	Category string `json:"category"`
	Status   string `json:"status"`
	Region   string `json:"region"`
	Note     string `json:"note"`
}

type serviceIPInfoJSON struct {
	Provider    string   `json:"provider"`
	IP          string   `json:"ip"`
	CountryCode string   `json:"country_code"`
	ASN         string   `json:"asn"`
	Org         string   `json:"org"`
	Type        string   `json:"type"`
	RiskScore   int      `json:"risk_score"`
	Flags       []string `json:"flags"`
	Error       string   `json:"error"`
}

type serviceResultJSON struct {
	Checks []serviceOutcomeJSON `json:"checks"`
	IPInfo []serviceIPInfoJSON  `json:"ip_info"`
}

func serviceResultString(result servicecheck.Result) string {
	out := serviceResultJSON{Checks: []serviceOutcomeJSON{}, IPInfo: []serviceIPInfoJSON{}}
	for _, c := range result.Checks {
		out.Checks = append(out.Checks, serviceOutcomeJSON{c.ID, c.Name, c.Category, c.Status, c.Region, c.Note})
	}
	for _, info := range result.IPInfo {
		flags := info.Flags
		if flags == nil {
			flags = []string{}
		}
		out.IPInfo = append(out.IPInfo, serviceIPInfoJSON{info.Provider, info.IP, info.CountryCode, info.ASN, info.Org, info.Type, info.RiskScore, flags, info.Error})
	}
	data, _ := json.Marshal(out)
	return string(data)
}

func StartServiceTest(platform PlatformInterface, request *TestRequest, handler ServiceTestHandler) error {
	if request == nil || handler == nil {
		return E.New("nil request or handler")
	}
	defs, err := servicecheck.ParseOrDefault(request.ServiceChecks)
	if err != nil {
		return E.Cause(err, "service checks")
	}
	options := servicecheck.Options{SkipIPInfo: request.SkipIPInfo}
	if len(request.serviceOnly) > 0 {
		options.Only = make(map[string]bool, len(request.serviceOnly))
		for _, id := range request.serviceOnly {
			options.Only[id] = true
		}
	}
	env, err := prepareTestEnv(nil, false, platform, request)
	if err != nil {
		return err
	}
	testCtx := probe.TestContext()
	timeout := time.Duration(request.TimeoutMs) * time.Millisecond
	go func() {
		defer handler.OnDone()
		defer env.close()
		newTestReporter(env.tags, probe.ServiceReporter.Results,
			func(result *probe.ServiceTestResult) string { return result.Tag },
			func(result *probe.ServiceTestResult) {
				if result.Error != nil {
					handler.OnResult(result.Tag, "", errorString(result.Error))
					return
				}
				handler.OnResult(result.Tag, serviceResultString(result.Result), "")
			},
		).run(func() []*probe.ServiceTestResult {
			return probe.BatchServiceTest(testCtx, env.box, env.tags, int(request.MaxConcurrency), true, defs, timeout, options)
		})
	}()
	return nil
}

type serviceCategoryJSON struct {
	ID   string `json:"id"`
	Name string `json:"name"`
	Icon string `json:"icon"`
}

type serviceCheckInfoJSON struct {
	ID       string `json:"id"`
	Name     string `json:"name"`
	Category string `json:"category"`
}

// ParseServiceChecks validates a definitions document ("" = the embedded default) and describes it as
// JSON {version, revision, categories:[{id,name,icon}], checks:[{id,name,category}]}.
func ParseServiceChecks(document string) (string, error) {
	defs, err := servicecheck.ParseOrDefault(document)
	if err != nil {
		return "", err
	}
	out := struct {
		Version    int                    `json:"version"`
		Revision   int64                  `json:"revision"`
		Categories []serviceCategoryJSON  `json:"categories"`
		Checks     []serviceCheckInfoJSON `json:"checks"`
	}{Version: defs.Version, Revision: defs.Revision}
	for _, c := range defs.Categories {
		out.Categories = append(out.Categories, serviceCategoryJSON{c.ID, c.Name, c.Icon})
	}
	for _, c := range defs.Checks {
		out.Checks = append(out.Checks, serviceCheckInfoJSON{c.ID, c.Name, c.Category})
	}
	data, err := json.Marshal(out)
	return string(data), err
}
