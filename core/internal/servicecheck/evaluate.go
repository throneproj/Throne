package servicecheck

import (
	"fmt"
	"net/http"
	"strconv"
)

// Response is what one request of a check produced. A request that failed after a redirect keeps
// the last response it got, so region sources can still read e.g. the Location it was sent to.
type Response struct {
	Status int
	URL    string
	Header http.Header
	Body   []byte
	Err    error
}

func (r *Response) answered() bool { return r != nil && r.Status != 0 }

type Outcome struct {
	ID       string
	Name     string
	Category string
	Status   string
	Region   string
	Note     string
}

// Evaluate turns a check's responses (indexed like Requests; nil = not sent) into an outcome:
// the first matching rule decides, then a known region outside the service's coverage overrides it.
func (c *Check) Evaluate(responses []*Response) Outcome {
	out := Outcome{ID: c.ID, Name: c.Name, Category: c.Category}
	out.Region = c.region(responses)
	for _, r := range c.Rules {
		if r.matches(responses) {
			out.Status = r.result
			out.Note = r.note
			if r.clearRegion {
				out.Region = ""
			}
			break
		}
	}
	if out.Status == "" {
		out.Status, out.Note = c.fallback(responses)
	}
	if out.Region != "" && (c.DenyRegions[out.Region] || (len(c.AllowRegions) > 0 && !c.AllowRegions[out.Region])) {
		out.Status = StatusRestricted
		out.Note = ""
	}
	return out
}

func (c *Check) fallback(responses []*Response) (string, string) {
	var last *Response
	for _, r := range responses {
		if r == nil {
			continue
		}
		if r.Err != nil {
			return StatusFailed, shortError(r.Err)
		}
		last = r
	}
	if last == nil {
		return StatusFailed, "not run"
	}
	return StatusUnknown, "unexpected response (HTTP " + strconv.Itoa(last.Status) + ")"
}

func (c *Check) region(responses []*Response) string {
	for _, source := range c.Region {
		if source.request >= len(responses) {
			continue
		}
		r := responses[source.request]
		if !r.answered() {
			continue
		}
		var candidates []string
		switch source.from {
		case "body":
			if m := source.match.FindSubmatch(r.Body); m != nil {
				candidates = append(candidates, string(m[1]))
			}
		case "url":
			if m := source.match.FindStringSubmatch(r.URL); m != nil {
				candidates = append(candidates, m[1])
			}
		case "header":
			for _, value := range r.Header.Values(source.header) {
				if m := source.match.FindStringSubmatch(value); m != nil {
					candidates = append(candidates, m[1])
				}
			}
		}
		for _, candidate := range candidates {
			if region := NormalizeRegion(candidate); region != "" {
				return region
			}
		}
	}
	return ""
}

func (r *rule) matches(responses []*Response) bool {
	for _, c := range r.conditions {
		if !c.matches(responses) {
			return false
		}
	}
	return true
}

func (c *condition) matches(responses []*Response) bool {
	if c.request >= len(responses) || responses[c.request] == nil {
		return false
	}
	r := responses[c.request]
	failed := r.Err != nil
	if c.failed != nil {
		if *c.failed != failed {
			return false
		}
		if failed {
			return true
		}
	}
	if failed {
		return false
	}
	if len(c.status) > 0 && !matchStatus(c.status, r.Status) {
		return false
	}
	for _, h := range c.headers {
		matched := false
		for _, value := range r.Header.Values(h.name) {
			if h.match.MatchString(value) {
				matched = true
				break
			}
		}
		if !matched {
			return false
		}
	}
	if c.body != nil && !c.body.Match(r.Body) {
		return false
	}
	if c.url != nil && !c.url.MatchString(r.URL) {
		return false
	}
	return true
}

func matchStatus(patterns []string, status int) bool {
	code := fmt.Sprintf("%03d", status)
	for _, pattern := range patterns {
		matched := len(code) == 3
		for i := 0; matched && i < 3; i++ {
			matched = pattern[i] == 'x' || pattern[i] == code[i]
		}
		if matched {
			return true
		}
	}
	return false
}
