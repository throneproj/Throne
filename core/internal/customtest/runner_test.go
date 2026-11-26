package customtest

import (
	"context"
	"fmt"
	"net"
	"net/http"
	"net/http/httptest"
	"strings"
	"sync/atomic"
	"testing"
	"time"
)

func mockDialer(hits *atomic.Int32) DialFunc {
	return func(ctx context.Context, network, addr string) (net.Conn, error) {
		if hits != nil {
			hits.Add(1)
		}
		var d net.Dialer
		return d.DialContext(ctx, network, addr)
	}
}

func TestCheckAndCompile(t *testing.T) {
	if err := Check("function helper() {}"); err == nil {
		t.Fatal("expected missing test() error")
	}
	if err := Check("function test( {"); err == nil {
		t.Fatal("expected syntax error")
	}
	if err := Check("   \n\t  "); err == nil {
		t.Fatal("expected empty script error")
	}
	if err := Check("async function test(ctx) { return 'ok'; }"); err != nil {
		t.Fatalf("unexpected error for valid async script: %v", err)
	}
}

func TestPrimitiveReturns(t *testing.T) {
	dial := mockDialer(nil)
	cases := []struct {
		name     string
		script   string
		wantText string
		wantKind SortKind
		wantNum  float64
	}{
		{
			name:     "string",
			script:   `function test() { return "HK"; }`,
			wantText: "HK",
			wantKind: SortNone,
		},
		{
			name:     "int",
			script:   `function test() { return 42; }`,
			wantText: "42",
			wantKind: SortNumber,
			wantNum:  42,
		},
		{
			name:     "float",
			script:   `function test() { return 12.5; }`,
			wantText: "12.5",
			wantKind: SortNumber,
			wantNum:  12.5,
		},
		{
			name:     "bool",
			script:   `function test() { return true; }`,
			wantText: "true",
			wantKind: SortNone,
		},
	}

	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			prog, err := Compile(tc.script)
			if err != nil {
				t.Fatalf("Compile failed: %v", err)
			}
			out, err := Run(context.Background(), prog, dial, 2*time.Second, 2*time.Second)
			if err != nil {
				t.Fatalf("Run failed: %v", err)
			}
			if out.Text != tc.wantText || out.SortKind != tc.wantKind || out.SortNumber != tc.wantNum {
				t.Fatalf("got %+v, want text=%q kind=%v num=%v", out, tc.wantText, tc.wantKind, tc.wantNum)
			}
		})
	}
}

func TestObjectAndFallbackReturns(t *testing.T) {
	dial := mockDialer(nil)

	prog, err := Compile(`
		function test() {
			return {
				text: "Unlock",
				color: "green",
				sortKey: 1,
				tooltip: "Full access"
			};
		}
	`)
	if err != nil {
		t.Fatalf("Compile failed: %v", err)
	}
	out, err := Run(context.Background(), prog, dial, 2*time.Second, 2*time.Second)
	if err != nil {
		t.Fatalf("Run failed: %v", err)
	}
	if out.Text != "Unlock" || out.Color != "green" || out.Tooltip != "Full access" || out.SortKind != SortNumber || out.SortNumber != 1 {
		t.Fatalf("unexpected outcome: %+v", out)
	}

	// Fallback keys: result / sort_key as string, optional color/tooltip omitted.
	prog2, err := Compile(`
		function test() {
			return { result: "US", sort_key: "01-us" };
		}
	`)
	if err != nil {
		t.Fatalf("Compile failed: %v", err)
	}
	out2, err := Run(context.Background(), prog2, dial, 2*time.Second, 2*time.Second)
	if err != nil {
		t.Fatalf("Run failed: %v", err)
	}
	if out2.Text != "US" || out2.Color != "" || out2.SortKind != SortString || out2.SortString != "01-us" {
		t.Fatalf("unexpected outcome2: %+v", out2)
	}
}

func TestAsyncFetchAndHTTPThroughDialer(t *testing.T) {
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		switch r.URL.Path {
		case "/json":
			if r.Header.Get("X-Test") != "throne" {
				http.Error(w, "missing header", http.StatusBadRequest)
				return
			}
			w.Header().Set("Content-Type", "application/json")
			_, _ = w.Write([]byte(`{"country":"JP","score":99}`))
		case "/redirect":
			http.Redirect(w, r, "/json", http.StatusFound)
		case "/echo":
			_, _ = fmt.Fprintf(w, "%s:%s", r.Method, r.Header.Get("X-Mode"))
		case "/big":
			_, _ = w.Write([]byte(strings.Repeat("a", 128)))
		default:
			http.NotFound(w, r)
		}
	}))
	defer srv.Close()

	var dialHits atomic.Int32
	dial := mockDialer(&dialHits)

	script := fmt.Sprintf(`
		async function test(ctx) {
			const res = await fetch(%q, {
				headers: { "X-Test": "throne" }
			});
			const data = await res.json();
			const postRes = http.post(%q, "payload", { headers: { "X-Mode": "fast" } });
			const manual = http.request({ url: %q, followRedirects: false });
			const capped = http.get(%q, { maxBytes: 16 });
			return {
				text: data.country + "-" + postRes.text + "-" + manual.status + "-" + capped.text.length + "-" + capped.truncated,
				color: res.ok ? "green" : "red",
				sortKey: data.score,
				tooltip: "timeout=" + ctx.timeoutMs + ", ct=" + res.headers.get("content-type")
			};
		}
	`, srv.URL+"/json", srv.URL+"/echo", srv.URL+"/redirect", srv.URL+"/big")

	prog, err := Compile(script)
	if err != nil {
		t.Fatalf("Compile failed: %v", err)
	}
	out, err := Run(context.Background(), prog, dial, 3*time.Second, 3*time.Second)
	if err != nil {
		t.Fatalf("Run failed: %v", err)
	}
	if dialHits.Load() < 4 {
		t.Fatalf("expected HTTP calls to use dialer, hits=%d", dialHits.Load())
	}
	if out.Text != "JP-POST:fast-302-16-true" {
		t.Fatalf("unexpected text: %q", out.Text)
	}
	if out.Color != "green" || out.SortKind != SortNumber || out.SortNumber != 99 {
		t.Fatalf("unexpected fields: %+v", out)
	}
	if !strings.Contains(out.Tooltip, "timeout=3000") || !strings.Contains(out.Tooltip, "application/json") {
		t.Fatalf("unexpected tooltip: %q", out.Tooltip)
	}
}

func TestTCPPingAndHelpers(t *testing.T) {
	ln, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		t.Fatalf("listen failed: %v", err)
	}
	defer ln.Close()
	go func() {
		for {
			c, err := ln.Accept()
			if err != nil {
				return
			}
			_ = c.Close()
		}
	}()

	tcpAddr := ln.Addr().(*net.TCPAddr)
	var hits atomic.Int32
	dial := mockDialer(&hits)

	script := fmt.Sprintf(`
		function test() {
			const ping = tcpPing("127.0.0.1", %d, 1000);
			const round = atob(btoa("hello"));
			return {
				text: (ping.ok ? "up" : "down") + ":" + round,
				sortKey: ping.latencyMs
			};
		}
	`, tcpAddr.Port)

	prog, err := Compile(script)
	if err != nil {
		t.Fatalf("Compile failed: %v", err)
	}
	out, err := Run(context.Background(), prog, dial, 2*time.Second, 2*time.Second)
	if err != nil {
		t.Fatalf("Run failed: %v", err)
	}
	if hits.Load() != 1 || out.Text != "up:hello" || out.SortKind != SortNumber {
		t.Fatalf("unexpected tcpPing result: hits=%d out=%+v", hits.Load(), out)
	}
}

func TestInfiniteLoopCancelledByContext(t *testing.T) {
	prog, err := Compile(`function test() { while (true) {} }`)
	if err != nil {
		t.Fatalf("Compile failed: %v", err)
	}
	start := time.Now()
	_, err = Run(context.Background(), prog, mockDialer(nil), 120*time.Millisecond, 120*time.Millisecond)
	if err == nil {
		t.Fatal("expected timeout error")
	}
	if time.Since(start) > 2*time.Second {
		t.Fatalf("loop took too long to abort: %v", time.Since(start))
	}
}

func TestMultipleFunctionsAndLexicalTest(t *testing.T) {
	srv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		_, _ = w.Write([]byte(`{"region":"SG","score":12}`))
	}))
	defer srv.Close()

	dial := mockDialer(nil)

	script1 := fmt.Sprintf(`
		const DEFAULT_LABEL = atob(btoa("OK"));

		function formatOutcome(region, score) {
			return {
				text: region + " (" + DEFAULT_LABEL + ")",
				color: "green",
				sortKey: score
			};
		}

		async function fetchMeta(url) {
			const res = await fetch(url);
			return await res.json();
		}

		async function test(ctx) {
			const meta = await fetchMeta(%q);
			return formatOutcome(meta.region, meta.score);
		}
	`, srv.URL)

	prog1, err := Compile(script1)
	if err != nil {
		t.Fatalf("Compile script1 failed: %v", err)
	}
	out1, err := Run(context.Background(), prog1, dial, 2*time.Second, 2*time.Second)
	if err != nil {
		t.Fatalf("Run script1 failed: %v", err)
	}
	if out1.Text != "SG (OK)" || out1.Color != "green" || out1.SortKind != SortNumber || out1.SortNumber != 12 {
		t.Fatalf("unexpected script1 outcome: %+v", out1)
	}

	script2 := fmt.Sprintf(`
		function helperTag(r) {
			return r.status === 200 ? "pass" : "fail";
		}

		const test = async (ctx) => {
			const res = await fetch(%q);
			return helperTag(res);
		};
	`, srv.URL)

	prog2, err := Compile(script2)
	if err != nil {
		t.Fatalf("Compile script2 failed: %v", err)
	}
	out2, err := Run(context.Background(), prog2, dial, 2*time.Second, 2*time.Second)
	if err != nil {
		t.Fatalf("Run script2 failed: %v", err)
	}
	if out2.Text != "pass" {
		t.Fatalf("unexpected script2 outcome: %+v", out2)
	}
}
