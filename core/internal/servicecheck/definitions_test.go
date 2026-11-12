package servicecheck

import (
	"strings"
	"testing"
)

func TestParseEmbeddedDefault(t *testing.T) {
	defs, err := Parse(defaultDocument)
	if err != nil {
		t.Fatalf("embedded definitions: %v", err)
	}
	if defs.Version != SupportedVersion {
		t.Fatalf("version = %d, want %d", defs.Version, SupportedVersion)
	}
	if defs.Revision <= 0 {
		t.Fatal("missing revision")
	}
	if len(defs.Categories) == 0 || len(defs.Checks) == 0 {
		t.Fatal("embedded file must list categories and checks")
	}
}

func TestParseOrDefaultEmpty(t *testing.T) {
	defs, err := ParseOrDefault("  \n  ")
	if err != nil {
		t.Fatal(err)
	}
	if len(defs.Checks) == 0 {
		t.Fatal("empty input should fall back to embedded checks")
	}
}

func TestParseRejectsUnknownField(t *testing.T) {
	document := testDocument(`{
		"id": "a", "name": "A", "category": "t",
		"requests": [{"id": "r", "url": "https://example.com/"}],
		"rules": [{"if": [{"status": ["2xx"]}], "result": "available"}]
	}`)
	document = strings.TrimSuffix(document, "}") + `,"extra": true}`
	_, err := Parse([]byte(document)) // unknown top-level key
	if err == nil || !strings.Contains(err.Error(), "unknown") {
		t.Fatalf("want unknown field error, got %v", err)
	}
}

func TestParseRejectsTrailingJSON(t *testing.T) {
	document := testDocument(`{
		"id": "a", "name": "A", "category": "t",
		"requests": [{"id": "r", "url": "https://example.com/"}],
		"rules": [{"if": [{"status": ["2xx"]}], "result": "available"}]
	}`)
	_, err := Parse([]byte(document + "}{}"))
	if err == nil || !strings.Contains(err.Error(), "trailing") {
		t.Fatalf("want trailing JSON error, got %v", err)
	}
}
