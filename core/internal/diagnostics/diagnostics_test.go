package diagnostics

import (
	"archive/zip"
	"bytes"
	"encoding/json"
	"io"
	"testing"
	"time"
)

func TestCaptureWritesTheArchive(t *testing.T) {
	var out bytes.Buffer
	err := Capture(&out, Request{
		Window:      50 * time.Millisecond,
		Attachments: []Attachment{{Name: `..\..\client.json`, Data: []byte(`{}`)}},
		BoxRunning:  func() bool { return true },
	})
	if err != nil {
		t.Fatalf("Capture: %v", err)
	}
	reader, err := zip.NewReader(bytes.NewReader(out.Bytes()), int64(out.Len()))
	if err != nil {
		t.Fatalf("archive does not open: %v", err)
	}
	files := make(map[string]*zip.File, len(reader.File))
	for _, file := range reader.File {
		files[file.Name] = file
	}
	for _, name := range []string{"README.txt", "cpu.pprof", "heap-start.pprof", "heap-end.pprof", "goroutines.txt", "meta.json", "app/client.json"} {
		if files[name] == nil {
			t.Errorf("archive lacks %s", name)
		}
	}
	if files["meta.json"] == nil {
		return
	}
	rc, err := files["meta.json"].Open()
	if err != nil {
		t.Fatal(err)
	}
	defer rc.Close()
	raw, err := io.ReadAll(rc)
	if err != nil {
		t.Fatal(err)
	}
	var info meta
	if err = json.Unmarshal(raw, &info); err != nil {
		t.Fatalf("meta.json: %v", err)
	}
	if !info.Box.Running || info.Capture.RequestedMs != 50 {
		t.Errorf("meta.json lost the request: %+v", info)
	}
}

func TestCaptureRejectsAnEmptyWindow(t *testing.T) {
	if err := Capture(io.Discard, Request{}); err == nil {
		t.Fatal("expected an error for a zero window")
	}
}
