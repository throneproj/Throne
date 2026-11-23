// Package torbridges holds the bridge lines Throne offers for Tor and asks Tor's bridge service (Moat) for more.
// The bridges document is remote data, so every line it can hand to tor is validated here.
package torbridges

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
	"slices"
	"strings"
)

//go:embed bridges.json
var embeddedDocument []byte

const SupportedVersion = 1

// Transport types as the bridge service names them.
const (
	TypeSnowflake = "snowflake"
	TypeWebtunnel = "webtunnel"
	TypeObfs4     = "obfs4"
	TypeMeek      = "meek"
	TypeVanilla   = "vanilla"
)

const (
	SourceBuiltin  = "builtin"
	SourceBridgeDB = "bridgedb"
)

// DefaultTransports is what the service is asked for when a request names none.
var DefaultTransports = []string{TypeWebtunnel, TypeObfs4, TypeSnowflake, TypeMeek, TypeVanilla}

// The pluggable transport a type's lines name; vanilla lines name none.
var lineTransport = map[string]string{
	TypeSnowflake: "snowflake",
	TypeWebtunnel: "webtunnel",
	TypeObfs4:     "obfs4",
	TypeMeek:      "meek_lite",
	TypeVanilla:   "",
}

var knownTransports = map[string]bool{"snowflake": true, "webtunnel": true, "obfs4": true, "meek_lite": true}

const (
	maxDocumentSize       = 256 << 10
	maxLineLength         = 4096
	maxLineFields         = 32
	maxLinesPerSetting    = 64
	maxCountries          = 300
	maxSettingsPerCountry = 16
	maxTargetsLength      = 2048
	maxTargetGroups       = 8
	maxFrontsPerGroup     = 8
)

type Setting struct {
	Type    string
	Source  string
	Bridges []string
}

type Moat struct {
	// No trailing slash; requests go to URL + "/circumvention/...".
	URL string
	// meek_lite's targets argument: url|front[+front...][,url|front...].
	Targets string
}

type Document struct {
	Version  int
	Revision int64
	// Document order, each with source builtin.
	Builtin   []Setting
	Countries map[string][]Setting
	Moat      Moat
}

type documentJSON struct {
	Version   int             `json:"version"`
	Revision  int64           `json:"revision"`
	Builtin   json.RawMessage `json:"builtin"`
	Countries json.RawMessage `json:"countries"`
	Moat      *moatJSON       `json:"moat"`
}

type settingJSON struct {
	Type    string   `json:"type"`
	Source  string   `json:"source"`
	Bridges []string `json:"bridges"`
}

type moatJSON struct {
	URL     string `json:"url"`
	Targets string `json:"targets"`
}

var (
	countryPattern     = regexp.MustCompile(`^[a-z]{2}$`)
	fingerprintPattern = regexp.MustCompile(`^[0-9A-Fa-f]{40}$`)
	argKeyPattern      = regexp.MustCompile(`^[A-Za-z0-9_.-]{1,64}$`)
	hostLabel          = regexp.MustCompile(`^[a-z0-9]([a-z0-9-]{0,61}[a-z0-9])?$`)
)

// Embedded returns a fresh parse of the copy built into the core; failing here is a packaging bug.
func Embedded() *Document {
	doc, err := Parse(embeddedDocument)
	if err != nil {
		panic("torbridges: embedded document: " + err.Error())
	}
	return doc
}

// ParseOrEmbedded parses document, or returns the embedded copy when it is empty.
func ParseOrEmbedded(document string) (*Document, error) {
	if strings.TrimSpace(document) == "" {
		return Embedded(), nil
	}
	return Parse([]byte(document))
}

// Effective returns cached when it parses and is newer than the embedded copy, else the embedded copy.
func Effective(cached string) *Document {
	embedded := Embedded()
	if strings.TrimSpace(cached) == "" {
		return embedded
	}
	if doc, err := Parse([]byte(cached)); err == nil && doc.Revision > embedded.Revision {
		return doc
	}
	return embedded
}

func Parse(document []byte) (*Document, error) {
	if len(document) > maxDocumentSize {
		return nil, fmt.Errorf("document larger than %d bytes", maxDocumentSize)
	}
	decoder := json.NewDecoder(bytes.NewReader(document))
	// Unknown keys are refused: a core that ignored a new key could hand tor lines meant to be used differently.
	decoder.DisallowUnknownFields()
	var raw documentJSON
	if err := decoder.Decode(&raw); err != nil {
		return nil, fmt.Errorf("invalid JSON: %w", err)
	}
	if _, err := decoder.Token(); err != io.EOF {
		return nil, errors.New("invalid JSON: trailing data")
	}
	if raw.Version != SupportedVersion {
		return nil, fmt.Errorf("unsupported version %d", raw.Version)
	}
	if raw.Revision <= 0 {
		return nil, errors.New("missing revision")
	}
	doc := &Document{Version: raw.Version, Revision: raw.Revision, Countries: make(map[string][]Setting)}

	builtinTypes := make(map[string]bool)
	err := decodeObject(raw.Builtin, func(transportType string, decoder *json.Decoder) error {
		if _, known := lineTransport[transportType]; !known {
			return fmt.Errorf("unknown type %q", transportType)
		}
		var lines []string
		if err := decoder.Decode(&lines); err != nil {
			return fmt.Errorf("%s: %w", transportType, err)
		}
		if len(lines) == 0 {
			return fmt.Errorf("%s: no lines", transportType)
		}
		if err := checkLines(transportType, lines); err != nil {
			return fmt.Errorf("%s: %w", transportType, err)
		}
		builtinTypes[transportType] = true
		if lines = dropRefused(transportType, lines); len(lines) > 0 {
			doc.Builtin = append(doc.Builtin, Setting{Type: transportType, Source: SourceBuiltin, Bridges: lines})
		}
		return nil
	})
	if err != nil {
		return nil, fmt.Errorf("builtin: %w", err)
	}
	if len(doc.Builtin) == 0 {
		return nil, errors.New("builtin: no types")
	}

	if raw.Countries != nil {
		err = decodeObject(raw.Countries, func(country string, decoder *json.Decoder) error {
			if !countryPattern.MatchString(country) {
				return fmt.Errorf("invalid country %q", country)
			}
			if len(doc.Countries) >= maxCountries {
				return fmt.Errorf("more than %d countries", maxCountries)
			}
			var entries []settingJSON
			if err := decoder.Decode(&entries); err != nil {
				return fmt.Errorf("%s: %w", country, err)
			}
			if len(entries) == 0 || len(entries) > maxSettingsPerCountry {
				return fmt.Errorf("%s: need 1 to %d settings", country, maxSettingsPerCountry)
			}
			for idx, entry := range entries {
				setting, err := parseCountrySetting(entry, builtinTypes)
				if err != nil {
					return fmt.Errorf("%s setting %d: %w", country, idx, err)
				}
				// Left without lines, a bridgedb setting would stand for the private pool instead.
				if len(setting.Bridges) > 0 {
					if setting.Bridges = dropRefused(setting.Type, setting.Bridges); len(setting.Bridges) == 0 {
						continue
					}
				}
				doc.Countries[country] = append(doc.Countries[country], setting)
			}
			return nil
		})
		if err != nil {
			return nil, fmt.Errorf("countries: %w", err)
		}
	}

	if raw.Moat == nil {
		return nil, errors.New("missing moat")
	}
	if doc.Moat.URL, err = parseMoatURL(raw.Moat.URL); err != nil {
		return nil, fmt.Errorf("moat url: %w", err)
	}
	if err = checkTargets(raw.Moat.Targets); err != nil {
		return nil, fmt.Errorf("moat targets: %w", err)
	}
	doc.Moat.Targets = raw.Moat.Targets
	return doc, nil
}

func parseCountrySetting(entry settingJSON, builtinTypes map[string]bool) (Setting, error) {
	if _, known := lineTransport[entry.Type]; !known {
		return Setting{}, fmt.Errorf("unknown type %q", entry.Type)
	}
	switch entry.Source {
	case SourceBuiltin:
		// The built-in lines are the document's own, never repeated per country.
		if entry.Bridges != nil {
			return Setting{}, errors.New("builtin settings carry no bridges")
		}
		if !builtinTypes[entry.Type] {
			return Setting{}, fmt.Errorf("no built-in %s lines", entry.Type)
		}
	case SourceBridgeDB:
		// Without lines this stands for the service's private pool, which only an online request can draw from.
		if err := checkLines(entry.Type, entry.Bridges); err != nil {
			return Setting{}, err
		}
	default:
		return Setting{}, fmt.Errorf("unknown source %q", entry.Source)
	}
	return Setting{Type: entry.Type, Source: entry.Source, Bridges: entry.Bridges}, nil
}

// decodeObject walks a JSON object in document order and refuses duplicate keys, which encoding/json would silently merge.
func decodeObject(raw json.RawMessage, value func(key string, decoder *json.Decoder) error) error {
	if len(raw) == 0 {
		return errors.New("missing")
	}
	decoder := json.NewDecoder(bytes.NewReader(raw))
	decoder.DisallowUnknownFields()
	if token, err := decoder.Token(); err != nil || token != json.Delim('{') {
		return errors.New("not an object")
	}
	seen := make(map[string]bool)
	for decoder.More() {
		token, err := decoder.Token()
		if err != nil {
			return err
		}
		key := token.(string)
		if seen[key] {
			return fmt.Errorf("duplicate key %q", key)
		}
		seen[key] = true
		if err = value(key, decoder); err != nil {
			return err
		}
	}
	_, err := decoder.Token()
	return err
}

func checkLines(transportType string, lines []string) error {
	if len(lines) > maxLinesPerSetting {
		return fmt.Errorf("more than %d lines", maxLinesPerSetting)
	}
	seen := make(map[string]bool, len(lines))
	for idx, line := range lines {
		transport, err := checkLine(line)
		if err != nil {
			return fmt.Errorf("line %d: %w", idx, err)
		}
		if transport != lineTransport[transportType] {
			return fmt.Errorf("line %d: a %s line, not %s", idx, lineType(transport), transportType)
		}
		if seen[line] {
			return fmt.Errorf("line %d: duplicate", idx)
		}
		seen[line] = true
	}
	return nil
}

// checkLine validates one bridge line, "[transport] address:port [fingerprint] [key=value...]" with single spaces,
// and returns its transport ("" for a vanilla bridge).
func checkLine(line string) (string, error) {
	if line == "" || len(line) > maxLineLength {
		return "", fmt.Errorf("empty or longer than %d bytes", maxLineLength)
	}
	for idx := 0; idx < len(line); idx++ {
		if c := line[idx]; c > '~' || c < ' ' {
			return "", errors.New("not printable ASCII")
		}
	}
	fields := strings.Split(line, " ")
	if len(fields) > maxLineFields {
		return "", fmt.Errorf("more than %d fields", maxLineFields)
	}
	if slices.Contains(fields, "") {
		return "", errors.New("stray space")
	}
	var transport string
	next := 0
	// Addresses always carry a port colon; transport names never do.
	if !strings.ContainsAny(fields[0], ":[") {
		transport = fields[0]
		if !knownTransports[transport] {
			return "", fmt.Errorf("unsupported transport %q", transport)
		}
		next = 1
	}
	if next >= len(fields) {
		return "", errors.New("missing address")
	}
	address, err := netip.ParseAddrPort(fields[next])
	if err != nil || address.Port() == 0 || address.Addr().Zone() != "" {
		return "", fmt.Errorf("invalid address %q", fields[next])
	}
	next++
	if next < len(fields) && fingerprintPattern.MatchString(fields[next]) {
		next++
	}
	if transport == "" && next < len(fields) {
		return "", errors.New("a vanilla bridge takes no arguments")
	}
	for _, arg := range fields[next:] {
		if key, _, found := strings.Cut(arg, "="); !found || !argKeyPattern.MatchString(key) {
			return "", fmt.Errorf("invalid argument %q", arg)
		}
	}
	return transport, nil
}

// Mirrors the tor outbound: a snowflake line needs a broker url, or an SQS queue with credentials and no other rendezvous.
func refusedLine(transportType, line string) bool {
	if transportType != TypeSnowflake {
		return false
	}
	values := make(map[string]string)
	for _, field := range strings.Fields(line) {
		if key, value, found := strings.Cut(field, "="); found {
			if _, loaded := values[key]; !loaded {
				values[key] = value
			}
		}
	}
	if values["sqsqueue"] != "" {
		return values["url"] != "" || values["ampcache"] != "" || values["sqscreds"] == ""
	}
	return values["url"] == ""
}

func dropRefused(transportType string, lines []string) []string {
	return slices.DeleteFunc(lines, func(line string) bool { return refusedLine(transportType, line) })
}

func lineType(transport string) string {
	for transportType, name := range lineTransport {
		if name == transport {
			return transportType
		}
	}
	return transport
}

func parseMoatURL(raw string) (string, error) {
	target, err := url.Parse(raw)
	if err != nil {
		return "", err
	}
	if target.Scheme != "https" || target.User != nil || target.Opaque != "" || target.RawQuery != "" || target.Fragment != "" {
		return "", errors.New("must be a plain https URL")
	}
	if err = checkHost(target.Host); err != nil {
		return "", err
	}
	return strings.TrimSuffix(target.String(), "/"), nil
}

func checkTargets(targets string) error {
	if targets == "" || len(targets) > maxTargetsLength {
		return fmt.Errorf("empty or longer than %d bytes", maxTargetsLength)
	}
	groups := strings.Split(targets, ",")
	if len(groups) > maxTargetGroups {
		return fmt.Errorf("more than %d targets", maxTargetGroups)
	}
	for _, group := range groups {
		rawURL, fronts, found := strings.Cut(group, "|")
		if !found {
			return fmt.Errorf("%q has no front", group)
		}
		target, err := url.Parse(rawURL)
		if err != nil || target.Scheme != "https" || target.User != nil || target.RawQuery != "" || target.Fragment != "" {
			return fmt.Errorf("invalid URL %q", rawURL)
		}
		if err = checkHost(target.Host); err != nil {
			return fmt.Errorf("URL %q: %w", rawURL, err)
		}
		frontList := strings.Split(fronts, "+")
		if len(frontList) > maxFrontsPerGroup {
			return fmt.Errorf("more than %d fronts", maxFrontsPerGroup)
		}
		for _, front := range frontList {
			if err = checkHost(front); err != nil {
				return fmt.Errorf("front %q: %w", front, err)
			}
		}
	}
	return nil
}

// A DNS name without port: fronts go into TLS SNI as they are.
func checkHost(host string) error {
	labels := strings.Split(host, ".")
	if len(host) > 253 || len(labels) < 2 {
		return errors.New("not a host name")
	}
	for _, label := range labels {
		if !hostLabel.MatchString(label) {
			return errors.New("not a host name")
		}
	}
	return nil
}

// NormalizeCountry lowercases an ISO 3166-1 alpha-2 code; empty stays empty.
func NormalizeCountry(country string) (string, error) {
	country = strings.ToLower(strings.TrimSpace(country))
	if country != "" && !countryPattern.MatchString(country) {
		return "", fmt.Errorf("invalid country %q", country)
	}
	return country, nil
}

// NormalizeTransports drops repeats, keeps the order and falls back to DefaultTransports.
func NormalizeTransports(transports []string) ([]string, error) {
	if len(transports) == 0 {
		return slices.Clone(DefaultTransports), nil
	}
	var normalized []string
	for _, transportType := range transports {
		if _, known := lineTransport[transportType]; !known {
			return nil, fmt.Errorf("unsupported transport %q", transportType)
		}
		if !slices.Contains(normalized, transportType) {
			normalized = append(normalized, transportType)
		}
	}
	return normalized, nil
}

func (d *Document) builtinLines(transportType string) []string {
	for _, setting := range d.Builtin {
		if setting.Type == transportType {
			return setting.Bridges
		}
	}
	return nil
}

// Offline answers without the service: the country's own lines and the built-ins it points at, never the private pool.
func (d *Document) Offline(country string, transports []string) []Setting {
	var settings []Setting
	for _, entry := range d.Countries[country] {
		if !slices.Contains(transports, entry.Type) {
			continue
		}
		lines := entry.Bridges
		if entry.Source == SourceBuiltin {
			lines = d.builtinLines(entry.Type)
		}
		if len(lines) > 0 {
			settings = append(settings, Setting{Type: entry.Type, Source: entry.Source, Bridges: slices.Clone(lines)})
		}
	}
	if settings = dedupe(settings); len(settings) > 0 {
		return settings
	}
	return d.fallback(transports)
}

// The built-in snowflake and obfs4 lines, else whichever requested built-ins exist.
func (d *Document) fallback(transports []string) []Setting {
	var settings []Setting
	add := func(transportType string) {
		if lines := d.builtinLines(transportType); len(lines) > 0 && slices.Contains(transports, transportType) {
			settings = append(settings, Setting{Type: transportType, Source: SourceBuiltin, Bridges: slices.Clone(lines)})
		}
	}
	add(TypeSnowflake)
	add(TypeObfs4)
	if len(settings) == 0 {
		for _, transportType := range transports {
			add(transportType)
		}
	}
	return dedupe(settings)
}

// dedupe drops lines an earlier setting already carries, then the settings left without lines.
func dedupe(settings []Setting) []Setting {
	seen := make(map[string]bool)
	kept := settings[:0]
	for _, setting := range settings {
		var lines []string
		for _, line := range setting.Bridges {
			if !seen[line] {
				seen[line] = true
				lines = append(lines, line)
			}
		}
		if len(lines) > 0 {
			setting.Bridges = lines
			kept = append(kept, setting)
		}
	}
	return kept
}
