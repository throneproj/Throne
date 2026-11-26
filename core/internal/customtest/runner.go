package customtest

import (
	"context"
	"crypto/tls"
	"encoding/base64"
	"errors"
	"fmt"
	"io"
	"math"
	"net"
	"net/http"
	"strconv"
	"strings"
	"sync/atomic"
	"time"

	"github.com/dop251/goja"
)

type SortKind int32

const (
	SortNone   SortKind = 0
	SortNumber SortKind = 1
	SortString SortKind = 2
)

const (
	DefaultTimeout      = 5 * time.Second
	defaultMaxBytes     = 2 << 20
	hardMaxBytes        = 8 << 20
	defaultMaxRedirects = 10
	hardMaxRedirects    = 10
	maxTextRunes        = 256
	maxTooltipRunes     = 2048
	validationTimeout   = 200 * time.Millisecond
)

type DialFunc func(ctx context.Context, network, addr string) (net.Conn, error)

type Outcome struct {
	Text       string
	Color      string
	Tooltip    string
	SortKind   SortKind
	SortNumber float64
	SortString string
}

type Program struct {
	compiled *goja.Program
}

// Compile parses the script once and verifies that evaluating it defines a callable test function.
func Compile(script string) (*Program, error) {
	if strings.TrimSpace(script) == "" {
		return nil, errors.New("script is empty")
	}
	compiled, err := goja.Compile("custom_test.js", script, false)
	if err != nil {
		return nil, err
	}

	vm := goja.New()
	installValidationGlobals(vm)
	timer := time.AfterFunc(validationTimeout, func() {
		vm.Interrupt(errors.New("script initialization timed out"))
	})
	_, runErr := vm.RunProgram(compiled)
	timer.Stop()
	if runErr != nil {
		return nil, unwrapJSError(runErr)
	}

	if _, ok := lookupTestFunc(vm); !ok {
		return nil, errors.New("script must define function test()")
	}
	return &Program{compiled: compiled}, nil
}

// Check validates syntax and the presence of a test() function.
func Check(script string) error {
	_, err := Compile(script)
	return err
}

func lookupTestFunc(vm *goja.Runtime) (goja.Callable, bool) {
	if fn, ok := goja.AssertFunction(vm.Get("test")); ok {
		return fn, true
	}
	val, err := vm.RunString("typeof test === 'function' ? test : undefined")
	if err != nil {
		return nil, false
	}
	return goja.AssertFunction(val)
}

func installValidationGlobals(vm *goja.Runtime) {
	noop := func(goja.FunctionCall) goja.Value { return goja.Undefined() }
	console := vm.NewObject()
	_ = console.Set("log", noop)
	_ = console.Set("info", noop)
	_ = console.Set("warn", noop)
	_ = console.Set("error", noop)
	_ = vm.Set("console", console)
	_ = vm.Set("atob", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) == 0 {
			panic(vm.NewGoError(errors.New("atob requires 1 argument")))
		}
		decoded, err := decodeBase64Flexible(call.Arguments[0].String())
		if err != nil {
			panic(vm.NewGoError(fmt.Errorf("atob: %w", err)))
		}
		return vm.ToValue(string(decoded))
	})
	_ = vm.Set("btoa", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) == 0 {
			panic(vm.NewGoError(errors.New("btoa requires 1 argument")))
		}
		return vm.ToValue(base64.StdEncoding.EncodeToString([]byte(call.Arguments[0].String())))
	})
}

// Run executes test(ctx) in an isolated VM whose network helpers dial exclusively through dial.
func Run(ctx context.Context, prog *Program, dial DialFunc, firstTimeout, totalTimeout time.Duration) (Outcome, error) {
	if prog == nil || prog.compiled == nil {
		return Outcome{}, errors.New("custom test program is nil")
	}
	if dial == nil {
		return Outcome{}, errors.New("custom test dialer is nil")
	}
	if totalTimeout <= 0 {
		totalTimeout = DefaultTimeout
	}
	if firstTimeout <= 0 {
		firstTimeout = totalTimeout
	}
	budget := totalTimeout
	if firstTimeout > budget {
		budget = firstTimeout
	}

	runCtx, cancel := context.WithTimeout(ctx, budget)
	defer cancel()

	vm := goja.New()
	done := make(chan struct{})
	defer close(done)
	go func() {
		select {
		case <-runCtx.Done():
			vm.Interrupt(runCtx.Err())
		case <-done:
		}
	}()

	rt := &runtimeEnv{
		ctx:          runCtx,
		vm:           vm,
		dial:         dial,
		firstTimeout: firstTimeout,
		baseTimeout:  totalTimeout,
		firstReq:     true,
		baseTransport: &http.Transport{
			DialContext:       dial,
			DisableKeepAlives: true,
			ForceAttemptHTTP2: true,
		},
		insecureTransport: &http.Transport{
			DialContext:       dial,
			DisableKeepAlives: true,
			ForceAttemptHTTP2: true,
			TLSClientConfig:   &tls.Config{InsecureSkipVerify: true},
		},
	}
	defer rt.close()

	rt.installGlobals()

	if _, err := vm.RunProgram(prog.compiled); err != nil {
		return Outcome{}, rt.finishError(err)
	}
	testFn, ok := lookupTestFunc(vm)
	if !ok {
		return Outcome{}, errors.New("script must define function test()")
	}

	ctxObj := vm.NewObject()
	_ = ctxObj.Set("timeoutMs", totalTimeout.Milliseconds())

	val, err := testFn(goja.Undefined(), ctxObj)
	if err != nil {
		return Outcome{}, rt.finishError(err)
	}
	if runCtx.Err() != nil {
		return Outcome{}, runCtx.Err()
	}

	val, err = unwrapPromise(val)
	if err != nil {
		return Outcome{}, err
	}
	if runCtx.Err() != nil {
		return Outcome{}, runCtx.Err()
	}

	return parseOutcome(vm, val)
}

type runtimeEnv struct {
	ctx               context.Context
	vm                *goja.Runtime
	dial              DialFunc
	firstTimeout      time.Duration
	baseTimeout       time.Duration
	firstReq          bool
	closed            atomic.Bool
	baseTransport     *http.Transport
	insecureTransport *http.Transport
}

func (r *runtimeEnv) close() {
	r.closed.Store(true)
	r.baseTransport.CloseIdleConnections()
	r.insecureTransport.CloseIdleConnections()
}

func (r *runtimeEnv) finishError(err error) error {
	if r.ctx.Err() != nil {
		return r.ctx.Err()
	}
	return unwrapJSError(err)
}

func (r *runtimeEnv) throw(err error) {
	if r.ctx.Err() != nil {
		r.vm.Interrupt(r.ctx.Err())
		return
	}
	panic(r.vm.NewGoError(err))
}

func (r *runtimeEnv) nextTimeout(overrideMs int64) time.Duration {
	if overrideMs > 0 {
		return time.Duration(overrideMs) * time.Millisecond
	}
	if r.firstReq {
		r.firstReq = false
		return r.firstTimeout
	}
	return r.baseTimeout
}

func (r *runtimeEnv) installGlobals() {
	installValidationGlobals(r.vm)

	httpObj := r.vm.NewObject()
	_ = httpObj.Set("request", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) == 0 {
			r.throw(errors.New("http.request requires an options object"))
		}
		return r.doHTTP(call.Arguments[0], "")
	})
	_ = httpObj.Set("get", func(call goja.FunctionCall) goja.Value {
		return r.doHTTPUnderVerb(call, http.MethodGet, false)
	})
	_ = httpObj.Set("head", func(call goja.FunctionCall) goja.Value {
		return r.doHTTPUnderVerb(call, http.MethodHead, false)
	})
	_ = httpObj.Set("post", func(call goja.FunctionCall) goja.Value {
		return r.doHTTPUnderVerb(call, http.MethodPost, true)
	})
	_ = r.vm.Set("http", httpObj)

	_ = r.vm.Set("fetch", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) == 0 {
			r.throw(errors.New("fetch requires a url"))
		}
		var initVal goja.Value
		if len(call.Arguments) > 1 {
			initVal = call.Arguments[1]
		}
		return r.doFetch(call.Arguments[0], initVal)
	})

	_ = r.vm.Set("tcpPing", func(call goja.FunctionCall) goja.Value {
		return r.doTCPPing(call)
	})

	_ = r.vm.Set("sleep", func(call goja.FunctionCall) goja.Value {
		var ms int64
		if len(call.Arguments) > 0 && !goja.IsUndefined(call.Arguments[0]) && !goja.IsNull(call.Arguments[0]) {
			ms = call.Arguments[0].ToInteger()
		}
		if ms <= 0 {
			return goja.Undefined()
		}
		timer := time.NewTimer(time.Duration(ms) * time.Millisecond)
		defer timer.Stop()
		select {
		case <-timer.C:
			return goja.Undefined()
		case <-r.ctx.Done():
			r.vm.Interrupt(r.ctx.Err())
			return goja.Undefined()
		}
	})

	_ = r.vm.Set("atob", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) == 0 {
			r.throw(errors.New("atob requires 1 argument"))
		}
		decoded, err := decodeBase64Flexible(call.Arguments[0].String())
		if err != nil {
			r.throw(fmt.Errorf("atob: %w", err))
		}
		return r.vm.ToValue(string(decoded))
	})

	_ = r.vm.Set("btoa", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) == 0 {
			r.throw(errors.New("btoa requires 1 argument"))
		}
		encoded := base64.StdEncoding.EncodeToString([]byte(call.Arguments[0].String()))
		return r.vm.ToValue(encoded)
	})
}

func decodeBase64Flexible(s string) ([]byte, error) {
	if b, err := base64.StdEncoding.DecodeString(s); err == nil {
		return b, nil
	}
	return base64.RawStdEncoding.DecodeString(strings.TrimRight(s, "="))
}

func (r *runtimeEnv) doHTTPUnderVerb(call goja.FunctionCall, method string, acceptBody bool) goja.Value {
	if len(call.Arguments) == 0 {
		r.throw(fmt.Errorf("http.%s requires a url", strings.ToLower(method)))
	}
	opts := r.vm.NewObject()
	_ = opts.Set("url", call.Arguments[0].String())
	_ = opts.Set("method", method)
	extraIdx := 1
	if acceptBody && len(call.Arguments) > 1 {
		arg1 := call.Arguments[1]
		if !goja.IsUndefined(arg1) && !goja.IsNull(arg1) {
			if obj, isObj := arg1.(*goja.Object); isObj && !hasProperty(obj, "url") && (hasProperty(obj, "headers") || hasProperty(obj, "timeoutMs") || hasProperty(obj, "body") || hasProperty(obj, "followRedirects") || hasProperty(obj, "maxBytes") || hasProperty(obj, "insecure")) {
				mergeOptions(opts, obj)
				extraIdx = 2
			} else {
				_ = opts.Set("body", arg1.String())
				extraIdx = 2
			}
		}
	}
	if len(call.Arguments) > extraIdx {
		if obj, isObj := call.Arguments[extraIdx].(*goja.Object); isObj {
			mergeOptions(opts, obj)
		}
	}
	return r.doHTTP(opts, method)
}

func hasProperty(obj *goja.Object, key string) bool {
	return obj != nil && !goja.IsUndefined(obj.Get(key))
}

func mergeOptions(dst, src *goja.Object) {
	if src == nil {
		return
	}
	for _, k := range src.Keys() {
		_ = dst.Set(k, src.Get(k))
	}
}

type httpResponseData struct {
	status     int
	statusText string
	url        string
	headers    map[string]string
	body       string
	elapsedMs  int64
	truncated  bool
}

func (r *runtimeEnv) execHTTP(optsVal goja.Value, forcedMethod string) httpResponseData {
	optsObj := optsVal.ToObject(r.vm)
	rawURL := strings.TrimSpace(propString(optsObj, "url"))
	if rawURL == "" {
		r.throw(errors.New("http request missing url"))
	}
	method := strings.ToUpper(strings.TrimSpace(forcedMethod))
	if method == "" {
		method = strings.ToUpper(strings.TrimSpace(propString(optsObj, "method")))
	}
	if method == "" {
		method = http.MethodGet
	}

	var bodyReader io.Reader
	if bodyVal := optsObj.Get("body"); bodyVal != nil && !goja.IsUndefined(bodyVal) && !goja.IsNull(bodyVal) {
		bodyReader = strings.NewReader(bodyVal.String())
	}

	timeout := r.nextTimeout(propInt(optsObj, "timeoutMs"))
	reqCtx, cancel := context.WithTimeout(r.ctx, timeout)
	defer cancel()

	req, err := http.NewRequestWithContext(reqCtx, method, rawURL, bodyReader)
	if err != nil {
		r.throw(err)
	}
	if hdrVal := optsObj.Get("headers"); hdrVal != nil && !goja.IsUndefined(hdrVal) && !goja.IsNull(hdrVal) {
		if hdrObj, ok := hdrVal.(*goja.Object); ok {
			for _, k := range hdrObj.Keys() {
				v := hdrObj.Get(k)
				if v != nil && !goja.IsUndefined(v) && !goja.IsNull(v) {
					req.Header.Set(k, v.String())
				}
			}
		}
	}

	follow := true
	if fv := optsObj.Get("followRedirects"); fv != nil && !goja.IsUndefined(fv) && !goja.IsNull(fv) {
		follow = fv.ToBoolean()
	}
	if rv := strings.ToLower(strings.TrimSpace(propString(optsObj, "redirect"))); rv == "manual" {
		follow = false
	}

	maxRedir := int(propInt(optsObj, "maxRedirects"))
	if maxRedir <= 0 || maxRedir > hardMaxRedirects {
		maxRedir = defaultMaxRedirects
	}

	maxBytes := int(propInt(optsObj, "maxBytes"))
	if maxBytes <= 0 {
		maxBytes = defaultMaxBytes
	}
	if maxBytes > hardMaxBytes {
		maxBytes = hardMaxBytes
	}

	transport := r.baseTransport
	if iv := optsObj.Get("insecure"); iv != nil && !goja.IsUndefined(iv) && iv.ToBoolean() {
		transport = r.insecureTransport
	}

	client := &http.Client{
		Transport: transport,
		CheckRedirect: func(_ *http.Request, via []*http.Request) error {
			if !follow {
				return http.ErrUseLastResponse
			}
			if len(via) >= maxRedir {
				return fmt.Errorf("stopped after %d redirects", maxRedir)
			}
			return nil
		},
	}

	started := time.Now()
	resp, err := client.Do(req)
	if err != nil {
		r.throw(err)
	}
	defer resp.Body.Close()

	rawBody, err := io.ReadAll(io.LimitReader(resp.Body, int64(maxBytes)+1))
	elapsed := time.Since(started).Milliseconds()
	if err != nil {
		r.throw(err)
	}
	truncated := false
	if len(rawBody) > maxBytes {
		rawBody = rawBody[:maxBytes]
		truncated = true
	}

	headers := make(map[string]string, len(resp.Header))
	for k, vals := range resp.Header {
		headers[strings.ToLower(k)] = strings.Join(vals, ", ")
	}

	finalURL := rawURL
	if resp.Request != nil && resp.Request.URL != nil {
		finalURL = resp.Request.URL.String()
	}

	return httpResponseData{
		status:     resp.StatusCode,
		statusText: resp.Status,
		url:        finalURL,
		headers:    headers,
		body:       string(rawBody),
		elapsedMs:  elapsed,
		truncated:  truncated,
	}
}

func (r *runtimeEnv) makeJSONMethod(body string) func(goja.FunctionCall) goja.Value {
	return func(goja.FunctionCall) goja.Value {
		parseFn, ok := goja.AssertFunction(r.vm.Get("JSON").ToObject(r.vm).Get("parse"))
		if !ok {
			r.throw(errors.New("JSON.parse is unavailable"))
		}
		res, err := parseFn(goja.Undefined(), r.vm.ToValue(body))
		if err != nil {
			panic(err)
		}
		return res
	}
}

func (r *runtimeEnv) doHTTP(optsVal goja.Value, forcedMethod string) goja.Value {
	data := r.execHTTP(optsVal, forcedMethod)
	obj := r.vm.NewObject()
	_ = obj.Set("status", data.status)
	_ = obj.Set("statusText", data.statusText)
	_ = obj.Set("ok", data.status >= 200 && data.status < 300)
	_ = obj.Set("url", data.url)
	_ = obj.Set("headers", data.headers)
	_ = obj.Set("text", data.body)
	_ = obj.Set("body", data.body)
	_ = obj.Set("elapsedMs", data.elapsedMs)
	_ = obj.Set("truncated", data.truncated)
	_ = obj.Set("json", r.makeJSONMethod(data.body))
	return obj
}

func (r *runtimeEnv) doFetch(urlVal, initVal goja.Value) goja.Value {
	opts := r.vm.NewObject()
	if initVal != nil && !goja.IsUndefined(initVal) && !goja.IsNull(initVal) {
		if initObj, ok := initVal.(*goja.Object); ok {
			mergeOptions(opts, initObj)
		}
	}
	_ = opts.Set("url", urlVal.String())

	data := r.execHTTP(opts, "")

	headersObj := r.vm.NewObject()
	for k, v := range data.headers {
		_ = headersObj.Set(k, v)
	}
	_ = headersObj.Set("get", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) == 0 {
			return goja.Null()
		}
		if v, ok := data.headers[strings.ToLower(call.Arguments[0].String())]; ok {
			return r.vm.ToValue(v)
		}
		return goja.Null()
	})
	_ = headersObj.Set("has", func(call goja.FunctionCall) goja.Value {
		if len(call.Arguments) == 0 {
			return r.vm.ToValue(false)
		}
		_, ok := data.headers[strings.ToLower(call.Arguments[0].String())]
		return r.vm.ToValue(ok)
	})

	respObj := r.vm.NewObject()
	_ = respObj.Set("status", data.status)
	_ = respObj.Set("statusText", data.statusText)
	_ = respObj.Set("ok", data.status >= 200 && data.status < 300)
	_ = respObj.Set("url", data.url)
	_ = respObj.Set("headers", headersObj)
	_ = respObj.Set("elapsedMs", data.elapsedMs)
	_ = respObj.Set("truncated", data.truncated)
	_ = respObj.Set("text", func(goja.FunctionCall) goja.Value {
		return r.vm.ToValue(data.body)
	})
	_ = respObj.Set("json", r.makeJSONMethod(data.body))
	return respObj
}

func (r *runtimeEnv) doTCPPing(call goja.FunctionCall) goja.Value {
	if len(call.Arguments) < 2 {
		r.throw(errors.New("tcpPing requires host and port"))
	}
	host := strings.TrimSpace(call.Arguments[0].String())
	port := int(call.Arguments[1].ToInteger())
	if host == "" || port <= 0 || port > 65535 {
		r.throw(errors.New("tcpPing invalid host or port"))
	}
	var overrideMs int64
	if len(call.Arguments) > 2 && !goja.IsUndefined(call.Arguments[2]) && !goja.IsNull(call.Arguments[2]) {
		overrideMs = call.Arguments[2].ToInteger()
	}
	timeout := r.nextTimeout(overrideMs)
	dialCtx, cancel := context.WithTimeout(r.ctx, timeout)
	defer cancel()

	addr := net.JoinHostPort(host, strconv.Itoa(port))
	started := time.Now()
	conn, err := r.dial(dialCtx, "tcp", addr)
	latencyMs := time.Since(started).Milliseconds()

	out := r.vm.NewObject()
	if err != nil {
		_ = out.Set("ok", false)
		_ = out.Set("latencyMs", int64(-1))
		_ = out.Set("error", err.Error())
		return out
	}
	_ = conn.Close()
	_ = out.Set("ok", true)
	_ = out.Set("latencyMs", latencyMs)
	_ = out.Set("error", "")
	return out
}

func unwrapPromise(val goja.Value) (goja.Value, error) {
	if val == nil || goja.IsUndefined(val) || goja.IsNull(val) {
		return val, nil
	}
	exported := val.Export()
	promise, ok := exported.(*goja.Promise)
	if !ok {
		return val, nil
	}
	switch promise.State() {
	case goja.PromiseStateFulfilled:
		return promise.Result(), nil
	case goja.PromiseStateRejected:
		res := promise.Result()
		if res == nil || goja.IsUndefined(res) {
			return nil, errors.New("test() rejected promise")
		}
		if errObj, ok := res.Export().(error); ok {
			return nil, errObj
		}
		return nil, errors.New(res.String())
	default:
		return nil, errors.New("test() returned an unresolved Promise")
	}
}

func parseOutcome(vm *goja.Runtime, val goja.Value) (Outcome, error) {
	if val == nil || goja.IsUndefined(val) || goja.IsNull(val) {
		return Outcome{}, nil
	}

	// Plain object returned: extract text / result / value, optional color, tooltip, sortKey / sort_key.
	if obj, ok := val.(*goja.Object); ok && val.ExportType() != nil && obj.ClassName() == "Object" {
		out := Outcome{}

		textVal := obj.Get("text")
		if isEmptyJSVal(textVal) {
			textVal = obj.Get("result")
		}
		if isEmptyJSVal(textVal) {
			textVal = obj.Get("value")
		}
		if !isEmptyJSVal(textVal) {
			out.Text = scalarText(vm, textVal)
		}

		if cv := obj.Get("color"); !isEmptyJSVal(cv) {
			out.Color = strings.TrimSpace(cv.String())
		}
		if tv := obj.Get("tooltip"); !isEmptyJSVal(tv) {
			out.Tooltip = truncateRunes(strings.TrimSpace(scalarText(vm, tv)), maxTooltipRunes)
		}

		sortVal := obj.Get("sortKey")
		if isEmptyJSVal(sortVal) {
			sortVal = obj.Get("sort_key")
		}
		if !isEmptyJSVal(sortVal) {
			exported := sortVal.Export()
			switch v := exported.(type) {
			case int64:
				out.SortKind = SortNumber
				out.SortNumber = float64(v)
			case float64:
				if !math.IsNaN(v) && !math.IsInf(v, 0) {
					out.SortKind = SortNumber
					out.SortNumber = v
				}
			case bool:
				out.SortKind = SortNumber
				if v {
					out.SortNumber = 1
				} else {
					out.SortNumber = 0
				}
			default:
				out.SortKind = SortString
				out.SortString = scalarText(vm, sortVal)
			}
		}

		out.Text = truncateRunes(out.Text, maxTextRunes)
		return out, nil
	}

	// Primitive or array/other value.
	exported := val.Export()
	switch v := exported.(type) {
	case int64:
		return Outcome{
			Text:       strconv.FormatInt(v, 10),
			SortKind:   SortNumber,
			SortNumber: float64(v),
		}, nil
	case float64:
		text := strconv.FormatFloat(v, 'f', -1, 64)
		out := Outcome{Text: truncateRunes(text, maxTextRunes)}
		if !math.IsNaN(v) && !math.IsInf(v, 0) {
			out.SortKind = SortNumber
			out.SortNumber = v
		}
		return out, nil
	case bool:
		return Outcome{Text: strconv.FormatBool(v)}, nil
	default:
		return Outcome{Text: truncateRunes(scalarText(vm, val), maxTextRunes)}, nil
	}
}

func isEmptyJSVal(v goja.Value) bool {
	return v == nil || goja.IsUndefined(v) || goja.IsNull(v)
}

func scalarText(vm *goja.Runtime, v goja.Value) string {
	if isEmptyJSVal(v) {
		return ""
	}
	exported := v.Export()
	switch x := exported.(type) {
	case float64:
		return strconv.FormatFloat(x, 'f', -1, 64)
	case int64:
		return strconv.FormatInt(x, 10)
	case string:
		return x
	default:
		if obj, ok := v.(*goja.Object); ok && (obj.ClassName() == "Object" || obj.ClassName() == "Array") {
			if str, err := obj.MarshalJSON(); err == nil {
				return string(str)
			}
		}
		return v.String()
	}
}

func truncateRunes(s string, max int) string {
	if max <= 0 {
		return s
	}
	runes := []rune(s)
	if len(runes) <= max {
		return s
	}
	return string(runes[:max])
}

func propString(obj *goja.Object, key string) string {
	if obj == nil {
		return ""
	}
	v := obj.Get(key)
	if isEmptyJSVal(v) {
		return ""
	}
	return v.String()
}

func propInt(obj *goja.Object, key string) int64 {
	if obj == nil {
		return 0
	}
	v := obj.Get(key)
	if isEmptyJSVal(v) {
		return 0
	}
	return v.ToInteger()
}

func unwrapJSError(err error) error {
	if err == nil {
		return nil
	}
	var exc *goja.Exception
	if errors.As(err, &exc) {
		if unwrapped := exc.Unwrap(); unwrapped != nil {
			return unwrapped
		}
		return errors.New(exc.Value().String())
	}
	var intr *goja.InterruptedError
	if errors.As(err, &intr) {
		if unwrapped, ok := intr.Value().(error); ok {
			return unwrapped
		}
		return errors.New(intr.String())
	}
	return err
}
