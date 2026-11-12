package servicecheck

import (
	"fmt"
	"os"
	"path/filepath"
	"testing"
)

const testCategoryJSON = `{"id": "t", "name": "T", "icon": "x"}`

func testDocument(checkJSON string) string {
	return fmt.Sprintf(
		`{"version": 1, "revision": 1, "categories": [%s], "checks": [%s]}`,
		testCategoryJSON,
		checkJSON,
	)
}

func readCheckJSON(t *testing.T, fileName string) string {
	t.Helper()
	path := filepath.Join("testdata", fileName)
	bytes, err := os.ReadFile(path)
	if err != nil {
		t.Fatalf("read %s: %v", fileName, err)
	}
	return string(bytes)
}

func parseCheckFromFile(t *testing.T, fileName string) *Check {
	t.Helper()
	return parseCheckFromJSON(t, readCheckJSON(t, fileName))
}

func parseCheckFromJSON(t *testing.T, checkJSON string) *Check {
	t.Helper()
	defs, err := Parse([]byte(testDocument(checkJSON)))
	if err != nil {
		t.Fatalf("parse: %v", err)
	}
	if len(defs.Checks) != 1 {
		t.Fatalf("got %d checks, want 1", len(defs.Checks))
	}
	return defs.Checks[0]
}
