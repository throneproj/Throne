// Package servicecheck runs data-driven checks of which services work through an exit and how
// its IP is classified. The definitions are remote data, so everything they can make the core
// do is bounded here: https GETs/HEADs to public host names, capped bodies, RE2 patterns.
package servicecheck

import (
	"bytes"
	_ "embed"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/netip"
	"net/url"
	"regexp"
	"strings"
	"unicode"
	"unicode/utf8"

	"golang.org/x/net/http/httpguts"
)

//go:embed checks.json
var defaultDocument []byte

const SupportedVersion = 1

const (
	maxDocumentSize      = 256 << 10
	maxCategories        = 16
	maxChecks            = 64
	maxRequestsPerCheck  = 4
	maxRulesPerCheck     = 16
	maxConditionsPerRule = 8
	maxRegionSources     = 8
	maxHeadersPerRequest = 16
	maxIPInfoProviders   = 4
	maxPatternLength     = 512
	maxPathSegments      = 8
	defaultMaxBody       = 256 << 10
	maxMaxBody           = 2 << 20
	defaultIPInfoBody    = 64 << 10
	maxIPInfoBody        = 256 << 10
)

const (
	StatusAvailable  = "available"
	StatusPartial    = "partial"
	StatusRestricted = "restricted"
	StatusBlocked    = "blocked"
	StatusUnknown    = "unknown"
	StatusFailed     = "failed"
)

var validStatuses = map[string]bool{
	StatusAvailable: true, StatusPartial: true, StatusRestricted: true,
	StatusBlocked: true, StatusUnknown: true, StatusFailed: true,
}

type Definitions struct {
	Version    int
	Revision   int64
	Categories []Category
	Checks     []*Check
	IPInfo     []*IPInfoProvider
}

type Category struct {
	ID   string
	Name string
	Icon string
}

type Check struct {
	ID           string
	Name         string
	Category     string
	Requests     []*Request
	Region       []*regionSource
	DenyRegions  map[string]bool
	AllowRegions map[string]bool
	Rules        []*rule
}

type Request struct {
	ID      string
	Method  string
	URL     *url.URL
	Headers map[string]string
	Follow  bool
	MaxBody int64
}

type regionSource struct {
	request int
	// "body", "url" or "header"
	from   string
	header string
	match  *regexp.Regexp
}

type rule struct {
	conditions []*condition
	result     string
	note       string
	// The page this rule recognises carries a placeholder region, not the exit's.
	clearRegion bool
}

type condition struct {
	request int
	failed  *bool
	status  []string
	headers []headerMatch
	body    *regexp.Regexp
	url     *regexp.Regexp
}

type headerMatch struct {
	name  string
	match *regexp.Regexp
}

type IPInfoProvider struct {
	ID      string
	Name    string
	URL     *url.URL
	MaxBody int64
	fields  map[string][]string
	flags   []flagPath
}

type flagPath struct {
	name string
	path []string
}

type documentJSON struct {
	Version    int            `json:"version"`
	Revision   int64          `json:"revision"`
	Categories []categoryJSON `json:"categories"`
	Checks     []checkJSON    `json:"checks"`
	IPInfo     []ipInfoJSON   `json:"ip_info"`
}

type categoryJSON struct {
	ID   string `json:"id"`
	Name string `json:"name"`
	Icon string `json:"icon"`
}

type checkJSON struct {
	ID           string        `json:"id"`
	Name         string        `json:"name"`
	Category     string        `json:"category"`
	Requests     []requestJSON `json:"requests"`
	Region       []regionJSON  `json:"region"`
	DenyRegions  []string      `json:"deny_regions"`
	AllowRegions []string      `json:"allow_regions"`
	Rules        []ruleJSON    `json:"rules"`
}

type requestJSON struct {
	ID      string            `json:"id"`
	URL     string            `json:"url"`
	Method  string            `json:"method"`
	Headers map[string]string `json:"headers"`
	Follow  *bool             `json:"follow"`
	MaxBody int64             `json:"max_body"`
}

type regionJSON struct {
	Request string `json:"request"`
	From    string `json:"from"`
	Match   string `json:"match"`
}

type ruleJSON struct {
	If          []conditionJSON `json:"if"`
	Result      string          `json:"result"`
	Note        string          `json:"note"`
	ClearRegion bool            `json:"clear_region"`
}

type conditionJSON struct {
	Request string            `json:"request"`
	Failed  *bool             `json:"failed"`
	Status  []string          `json:"status"`
	Headers map[string]string `json:"headers"`
	Body    string            `json:"body"`
	URL     string            `json:"url"`
}

type ipInfoJSON struct {
	ID      string            `json:"id"`
	Name    string            `json:"name"`
	URL     string            `json:"url"`
	MaxBody int64             `json:"max_body"`
	Fields  map[string]string `json:"fields"`
	Flags   map[string]string `json:"flags"`
}

var (
	idPattern     = regexp.MustCompile(`^[a-z0-9][a-z0-9_.-]{0,31}$`)
	statusPattern = regexp.MustCompile(`^[1-5][0-9x][0-9x]$`)
	tokenPattern  = regexp.MustCompile(`^[A-Za-z0-9-]{1,64}$`)
	pathPattern   = regexp.MustCompile(`^[A-Za-z0-9_]{1,64}$`)
	hostLabel     = regexp.MustCompile(`^[a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?$`)
)

// The transport owns these; a definition setting them could smuggle requests or credentials.
var forbiddenHeaders = map[string]bool{
	"host": true, "content-length": true, "transfer-encoding": true, "connection": true,
	"upgrade": true, "keep-alive": true, "te": true, "trailer": true, "expect": true,
	"proxy-authorization": true, "proxy-connection": true, "authorization": true,
	"accept-encoding": true,
}

var ipInfoFields = map[string]bool{"ip": true, "country_code": true, "asn": true, "org": true, "type": true, "risk_score": true}

var ipInfoFlagOrder = []string{"vpn", "proxy", "tor", "hosting", "abuser", "anonymous", "relay", "mobile"}

var ipInfoFlags = func() map[string]bool {
	set := make(map[string]bool, len(ipInfoFlagOrder))
	for _, name := range ipInfoFlagOrder {
		set[name] = true
	}
	return set
}()

// Names that resolve to the user's own machine or network wherever the request ends up.
var privateSuffixes = []string{"localhost", "local", "internal", "lan", "home.arpa", "intranet", "corp", "test", "invalid", "example"}

func Default() *Definitions {
	defs, err := Parse(defaultDocument)
	if err != nil {
		panic("servicecheck: embedded definitions: " + err.Error())
	}
	return defs
}

// ParseOrDefault parses document, or returns the embedded definitions when it is empty.
func ParseOrDefault(document string) (*Definitions, error) {
	if strings.TrimSpace(document) == "" {
		return Default(), nil
	}
	return Parse([]byte(document))
}

func Parse(document []byte) (*Definitions, error) {
	if len(document) > maxDocumentSize {
		return nil, fmt.Errorf("document larger than %d bytes", maxDocumentSize)
	}
	decoder := json.NewDecoder(bytes.NewReader(document))
	// Unknown keys are refused: an older core that ignored a new condition would match more than meant.
	decoder.DisallowUnknownFields()
	var doc documentJSON
	if err := decoder.Decode(&doc); err != nil {
		return nil, fmt.Errorf("invalid JSON: %w", err)
	}
	if _, err := decoder.Token(); err != io.EOF {
		return nil, errors.New("invalid JSON: trailing data")
	}
	if doc.Version != SupportedVersion {
		return nil, fmt.Errorf("unsupported version %d", doc.Version)
	}
	if doc.Revision <= 0 {
		return nil, errors.New("missing revision")
	}
	defs := &Definitions{Version: doc.Version, Revision: doc.Revision}

	if len(doc.Categories) == 0 || len(doc.Categories) > maxCategories {
		return nil, fmt.Errorf("need 1 to %d categories", maxCategories)
	}
	categories := make(map[string]bool)
	for idx, c := range doc.Categories {
		if !idPattern.MatchString(c.ID) || categories[c.ID] {
			return nil, fmt.Errorf("category %d: invalid or duplicate id %q", idx, c.ID)
		}
		if err := checkText(c.Name, 32, false); err != nil {
			return nil, fmt.Errorf("category %s: name: %w", c.ID, err)
		}
		if err := checkText(c.Icon, 16, false); err != nil || len(c.Icon) > 16 {
			return nil, fmt.Errorf("category %s: invalid icon", c.ID)
		}
		categories[c.ID] = true
		defs.Categories = append(defs.Categories, Category{ID: c.ID, Name: c.Name, Icon: c.Icon})
	}

	if len(doc.Checks) == 0 || len(doc.Checks) > maxChecks {
		return nil, fmt.Errorf("need 1 to %d checks", maxChecks)
	}
	checkIDs := make(map[string]bool)
	for idx := range doc.Checks {
		check, err := parseCheck(&doc.Checks[idx], categories)
		if err != nil {
			return nil, fmt.Errorf("check %d (%s): %w", idx, doc.Checks[idx].ID, err)
		}
		if checkIDs[check.ID] {
			return nil, fmt.Errorf("duplicate check id %q", check.ID)
		}
		checkIDs[check.ID] = true
		defs.Checks = append(defs.Checks, check)
	}

	if len(doc.IPInfo) > maxIPInfoProviders {
		return nil, fmt.Errorf("at most %d ip_info providers", maxIPInfoProviders)
	}
	providerIDs := make(map[string]bool)
	for idx := range doc.IPInfo {
		provider, err := parseIPInfo(&doc.IPInfo[idx])
		if err != nil {
			return nil, fmt.Errorf("ip_info %d (%s): %w", idx, doc.IPInfo[idx].ID, err)
		}
		if providerIDs[provider.ID] {
			return nil, fmt.Errorf("duplicate ip_info id %q", provider.ID)
		}
		providerIDs[provider.ID] = true
		defs.IPInfo = append(defs.IPInfo, provider)
	}
	return defs, nil
}

func parseCheck(c *checkJSON, categories map[string]bool) (*Check, error) {
	if !idPattern.MatchString(c.ID) {
		return nil, errors.New("invalid id")
	}
	if err := checkText(c.Name, 48, false); err != nil {
		return nil, fmt.Errorf("name: %w", err)
	}
	if !categories[c.Category] {
		return nil, fmt.Errorf("unknown category %q", c.Category)
	}
	check := &Check{ID: c.ID, Name: c.Name, Category: c.Category}

	if len(c.Requests) == 0 || len(c.Requests) > maxRequestsPerCheck {
		return nil, fmt.Errorf("need 1 to %d requests", maxRequestsPerCheck)
	}
	requestIndex := make(map[string]int)
	for idx := range c.Requests {
		request, err := parseRequest(&c.Requests[idx])
		if err != nil {
			return nil, fmt.Errorf("request %d: %w", idx, err)
		}
		if _, dup := requestIndex[request.ID]; dup {
			return nil, fmt.Errorf("duplicate request id %q", request.ID)
		}
		requestIndex[request.ID] = idx
		check.Requests = append(check.Requests, request)
	}
	resolve := func(id string) (int, error) {
		if id == "" && len(check.Requests) == 1 {
			return 0, nil
		}
		if idx, ok := requestIndex[id]; ok {
			return idx, nil
		}
		return 0, fmt.Errorf("unknown request %q", id)
	}

	if len(c.Region) > maxRegionSources {
		return nil, fmt.Errorf("at most %d region sources", maxRegionSources)
	}
	for idx, r := range c.Region {
		source, err := parseRegionSource(r, resolve)
		if err != nil {
			return nil, fmt.Errorf("region %d: %w", idx, err)
		}
		check.Region = append(check.Region, source)
	}
	var err error
	if check.DenyRegions, err = parseRegionList(c.DenyRegions); err != nil {
		return nil, fmt.Errorf("deny_regions: %w", err)
	}
	if check.AllowRegions, err = parseRegionList(c.AllowRegions); err != nil {
		return nil, fmt.Errorf("allow_regions: %w", err)
	}

	if len(c.Rules) == 0 || len(c.Rules) > maxRulesPerCheck {
		return nil, fmt.Errorf("need 1 to %d rules", maxRulesPerCheck)
	}
	for idx := range c.Rules {
		r, err := parseRule(&c.Rules[idx], resolve)
		if err != nil {
			return nil, fmt.Errorf("rule %d: %w", idx, err)
		}
		check.Rules = append(check.Rules, r)
	}
	return check, nil
}

func parseRequest(r *requestJSON) (*Request, error) {
	if !idPattern.MatchString(r.ID) {
		return nil, fmt.Errorf("invalid id %q", r.ID)
	}
	target, err := parseTargetURL(r.URL)
	if err != nil {
		return nil, err
	}
	request := &Request{ID: r.ID, URL: target, Follow: true, MaxBody: defaultMaxBody}
	switch strings.ToUpper(r.Method) {
	case "", "GET":
		request.Method = "GET"
	case "HEAD":
		request.Method = "HEAD"
	default:
		return nil, fmt.Errorf("method %q not allowed", r.Method)
	}
	if r.Follow != nil {
		request.Follow = *r.Follow
	}
	if r.MaxBody < 0 || r.MaxBody > maxMaxBody {
		return nil, fmt.Errorf("max_body above %d", maxMaxBody)
	}
	if r.MaxBody > 0 {
		request.MaxBody = r.MaxBody
	}
	if len(r.Headers) > maxHeadersPerRequest {
		return nil, fmt.Errorf("at most %d headers", maxHeadersPerRequest)
	}
	for name, value := range r.Headers {
		if !tokenPattern.MatchString(name) || forbiddenHeaders[strings.ToLower(name)] {
			return nil, fmt.Errorf("header %q not allowed", name)
		}
		if len(value) > 1024 || !httpguts.ValidHeaderFieldValue(value) {
			return nil, fmt.Errorf("header %q: invalid value", name)
		}
	}
	request.Headers = r.Headers
	return request, nil
}

func parseRegionSource(r regionJSON, resolve func(string) (int, error)) (*regionSource, error) {
	idx, err := resolve(r.Request)
	if err != nil {
		return nil, err
	}
	source := &regionSource{request: idx}
	switch {
	case r.From == "body" || r.From == "url":
		source.from = r.From
	case strings.HasPrefix(r.From, "header:") && tokenPattern.MatchString(strings.TrimPrefix(r.From, "header:")):
		source.from = "header"
		source.header = strings.TrimPrefix(r.From, "header:")
	default:
		return nil, fmt.Errorf("invalid from %q", r.From)
	}
	if source.match, err = compilePattern(r.Match); err != nil {
		return nil, err
	}
	if source.match.NumSubexp() < 1 {
		return nil, errors.New("match needs a capture group")
	}
	return source, nil
}

func parseRule(r *ruleJSON, resolve func(string) (int, error)) (*rule, error) {
	if !validStatuses[r.Result] {
		return nil, fmt.Errorf("invalid result %q", r.Result)
	}
	if err := checkText(r.Note, 80, true); err != nil {
		return nil, fmt.Errorf("note: %w", err)
	}
	if len(r.If) == 0 || len(r.If) > maxConditionsPerRule {
		return nil, fmt.Errorf("need 1 to %d conditions", maxConditionsPerRule)
	}
	parsed := &rule{result: r.Result, note: r.Note, clearRegion: r.ClearRegion}
	for idx := range r.If {
		c, err := parseCondition(&r.If[idx], resolve)
		if err != nil {
			return nil, fmt.Errorf("condition %d: %w", idx, err)
		}
		parsed.conditions = append(parsed.conditions, c)
	}
	return parsed, nil
}

func parseCondition(c *conditionJSON, resolve func(string) (int, error)) (*condition, error) {
	idx, err := resolve(c.Request)
	if err != nil {
		return nil, err
	}
	parsed := &condition{request: idx, failed: c.Failed}
	hasResponseTest := len(c.Status) > 0 || len(c.Headers) > 0 || c.Body != "" || c.URL != ""
	if c.Failed != nil && *c.Failed && hasResponseTest {
		return nil, errors.New("failed: true cannot be combined with response tests")
	}
	if c.Failed == nil && !hasResponseTest {
		return nil, errors.New("empty condition")
	}
	for _, s := range c.Status {
		if !statusPattern.MatchString(s) {
			return nil, fmt.Errorf("invalid status %q", s)
		}
		parsed.status = append(parsed.status, s)
	}
	if len(c.Headers) > maxHeadersPerRequest {
		return nil, fmt.Errorf("at most %d headers", maxHeadersPerRequest)
	}
	for name, pattern := range c.Headers {
		if !tokenPattern.MatchString(name) {
			return nil, fmt.Errorf("invalid header name %q", name)
		}
		match, err := compilePattern(pattern)
		if err != nil {
			return nil, fmt.Errorf("header %s: %w", name, err)
		}
		parsed.headers = append(parsed.headers, headerMatch{name: name, match: match})
	}
	if c.Body != "" {
		if parsed.body, err = compilePattern(c.Body); err != nil {
			return nil, fmt.Errorf("body: %w", err)
		}
	}
	if c.URL != "" {
		if parsed.url, err = compilePattern(c.URL); err != nil {
			return nil, fmt.Errorf("url: %w", err)
		}
	}
	return parsed, nil
}

func parseIPInfo(p *ipInfoJSON) (*IPInfoProvider, error) {
	if !idPattern.MatchString(p.ID) {
		return nil, errors.New("invalid id")
	}
	if err := checkText(p.Name, 48, false); err != nil {
		return nil, fmt.Errorf("name: %w", err)
	}
	target, err := parseTargetURL(p.URL)
	if err != nil {
		return nil, err
	}
	provider := &IPInfoProvider{ID: p.ID, Name: p.Name, URL: target, MaxBody: defaultIPInfoBody, fields: make(map[string][]string)}
	if p.MaxBody < 0 || p.MaxBody > maxIPInfoBody {
		return nil, fmt.Errorf("max_body above %d", maxIPInfoBody)
	}
	if p.MaxBody > 0 {
		provider.MaxBody = p.MaxBody
	}
	if p.Fields["ip"] == "" {
		return nil, errors.New("fields.ip is required")
	}
	for name, path := range p.Fields {
		if !ipInfoFields[name] {
			return nil, fmt.Errorf("unknown field %q", name)
		}
		if provider.fields[name], err = parsePath(path); err != nil {
			return nil, fmt.Errorf("field %s: %w", name, err)
		}
	}
	for name := range p.Flags {
		if !ipInfoFlags[name] {
			return nil, fmt.Errorf("unknown flag %q", name)
		}
	}
	for _, name := range ipInfoFlagOrder {
		path, ok := p.Flags[name]
		if !ok {
			continue
		}
		segments, err := parsePath(path)
		if err != nil {
			return nil, fmt.Errorf("flag %s: %w", name, err)
		}
		provider.flags = append(provider.flags, flagPath{name: name, path: segments})
	}
	return provider, nil
}

func parsePath(path string) ([]string, error) {
	segments := strings.Split(path, ".")
	if len(segments) > maxPathSegments {
		return nil, fmt.Errorf("path %q too deep", path)
	}
	for _, s := range segments {
		if !pathPattern.MatchString(s) {
			return nil, fmt.Errorf("invalid path %q", path)
		}
	}
	return segments, nil
}

func parseRegionList(codes []string) (map[string]bool, error) {
	if len(codes) == 0 {
		return nil, nil
	}
	if len(codes) > 300 {
		return nil, errors.New("too many regions")
	}
	set := make(map[string]bool, len(codes))
	for _, code := range codes {
		if len(code) != 2 || NormalizeRegion(code) != code {
			return nil, fmt.Errorf("invalid region %q", code)
		}
		set[code] = true
	}
	return set, nil
}

func compilePattern(pattern string) (*regexp.Regexp, error) {
	if pattern == "" || len(pattern) > maxPatternLength {
		return nil, fmt.Errorf("pattern empty or longer than %d", maxPatternLength)
	}
	return regexp.Compile(pattern)
}

func checkText(text string, maxRunes int, allowEmpty bool) error {
	if text == "" {
		if allowEmpty {
			return nil
		}
		return errors.New("empty")
	}
	if !utf8.ValidString(text) || utf8.RuneCountInString(text) > maxRunes {
		return fmt.Errorf("invalid or longer than %d characters", maxRunes)
	}
	for _, r := range text {
		if unicode.IsControl(r) || r == ' ' || r == ' ' {
			return errors.New("control characters")
		}
	}
	return nil
}

// Definitions and redirects may only reach public host names over https on 443.
func parseTargetURL(raw string) (*url.URL, error) {
	if len(raw) > 2048 {
		return nil, errors.New("url too long")
	}
	target, err := url.Parse(raw)
	if err != nil {
		return nil, fmt.Errorf("invalid url: %w", err)
	}
	if err := checkTarget(target); err != nil {
		return nil, err
	}
	return target, nil
}

func checkTarget(target *url.URL) error {
	if target.Scheme != "https" {
		return fmt.Errorf("url %q: only https is allowed", target.Redacted())
	}
	if target.User != nil || target.Opaque != "" {
		return fmt.Errorf("url %q: credentials or opaque form not allowed", target.Redacted())
	}
	if port := target.Port(); port != "" && port != "443" {
		return fmt.Errorf("url %q: only port 443 is allowed", target.Redacted())
	}
	host := strings.ToLower(target.Hostname())
	if _, err := netip.ParseAddr(host); err == nil {
		return fmt.Errorf("url %q: IP addresses are not allowed", target.Redacted())
	}
	host = strings.TrimSuffix(host, ".")
	labels := strings.Split(host, ".")
	if len(host) > 253 || len(labels) < 2 {
		return fmt.Errorf("url %q: not a public host name", target.Redacted())
	}
	for _, label := range labels {
		if !hostLabel.MatchString(label) {
			return fmt.Errorf("url %q: invalid host name", target.Redacted())
		}
	}
	if tld := labels[len(labels)-1]; strings.Trim(tld, "abcdefghijklmnopqrstuvwxyz") != "" && !strings.HasPrefix(tld, "xn--") {
		return fmt.Errorf("url %q: invalid top-level domain", target.Redacted())
	}
	for _, suffix := range privateSuffixes {
		if host == suffix || strings.HasSuffix(host, "."+suffix) {
			return fmt.Errorf("url %q: private host name", target.Redacted())
		}
	}
	return nil
}
