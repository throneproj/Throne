package servicecheck

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"net/netip"
	"strconv"
	"strings"
	"unicode"
)

type IPInfo struct {
	Provider    string
	IP          string
	CountryCode string
	ASN         string
	Org         string
	Type        string
	// 0-100, -1 = not given.
	RiskScore int
	Flags     []string
	Error     string
}

var ipInfoHeaders = map[string]string{
	"Accept":         "application/json",
	"Sec-Fetch-Dest": "empty",
	"Sec-Fetch-Mode": "cors",
}

// The bool reports whether the exit answered at all, an HTTP error included.
func (p *IPInfoProvider) lookup(ctx context.Context, client *Client) (IPInfo, bool) {
	s := client.newSession()
	defer s.close()
	resp := s.fetch(ctx, &Request{ID: p.ID, Method: "GET", URL: p.URL, Headers: ipInfoHeaders, Follow: true, MaxBody: p.MaxBody})
	info := IPInfo{Provider: p.Name, RiskScore: -1}
	if resp.Err != nil {
		info.Error = shortError(resp.Err)
		return info, resp.answered()
	}
	if err := p.parse(resp.Status, resp.Body, &info); err != nil {
		info.Error = err.Error()
	}
	return info, true
}

func (p *IPInfoProvider) parse(status int, body []byte, info *IPInfo) error {
	if status != 200 {
		return fmt.Errorf("HTTP %d", status)
	}
	decoder := json.NewDecoder(bytes.NewReader(body))
	decoder.UseNumber()
	var doc any
	if err := decoder.Decode(&doc); err != nil {
		return errors.New("invalid response")
	}
	field := func(name string) any {
		path, ok := p.fields[name]
		if !ok {
			return nil
		}
		return lookupPath(doc, path)
	}
	ip, _ := field("ip").(string)
	addr, err := netip.ParseAddr(strings.TrimSpace(ip))
	if err != nil {
		return errors.New("no IP in response")
	}
	info.IP = addr.Unmap().String()
	if code, ok := field("country_code").(string); ok {
		info.CountryCode = NormalizeRegion(code)
	}
	info.ASN = normalizeASN(field("asn"))
	if org, ok := field("org").(string); ok {
		info.Org = cleanText(org, 80)
	}
	if kind, ok := field("type").(string); ok {
		info.Type = strings.ToLower(cleanText(kind, 24))
	}
	if score, ok := field("risk_score").(json.Number); ok {
		if value, err := score.Float64(); err == nil {
			info.RiskScore = int(min(max(value, 0), 100))
		}
	}
	for _, flag := range p.flags {
		if set, ok := lookupPath(doc, flag.path).(bool); ok && set {
			info.Flags = append(info.Flags, flag.name)
		}
	}
	return nil
}

func lookupPath(value any, path []string) any {
	for _, segment := range path {
		switch node := value.(type) {
		case map[string]any:
			value = node[segment]
		case []any:
			idx, err := strconv.Atoi(segment)
			if err != nil || idx < 0 || idx >= len(node) {
				return nil
			}
			value = node[idx]
		default:
			return nil
		}
	}
	return value
}

func normalizeASN(value any) string {
	var text string
	switch v := value.(type) {
	case json.Number:
		text = v.String()
	case string:
		text = strings.TrimSpace(v)
	default:
		return ""
	}
	digits := strings.TrimPrefix(strings.ToUpper(text), "AS")
	if n, err := strconv.ParseUint(digits, 10, 32); err == nil && n > 0 {
		return "AS" + strconv.FormatUint(n, 10)
	}
	return ""
}

func cleanText(text string, maxRunes int) string {
	var b strings.Builder
	count := 0
	for _, r := range strings.TrimSpace(text) {
		if r == unicode.ReplacementChar || unicode.IsControl(r) || r == ' ' || r == ' ' {
			continue
		}
		if count == maxRunes {
			break
		}
		b.WriteRune(r)
		count++
	}
	return b.String()
}
