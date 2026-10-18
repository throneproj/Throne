package guard

import "testing"

func TestParseConfigAPIPort(t *testing.T) {
	for _, test := range []struct {
		line       string
		want       uint16
		keepStates bool
	}{
		{`{"allow_lan":true}`, 0, false},
		{`{"api_port":0}`, 0, false},
		{`{"api_port":9095}`, 9095, false},
		{`{"api_port":65535}`, 65535, false},
		{`{"api_port":9095,"keep_states":true}`, 9095, true},
		{`{"keep_states":false}`, 0, false},
	} {
		cfg, err := parseConfig(test.line)
		if err != nil {
			t.Fatalf("parseConfig(%s): %v", test.line, err)
		}
		if cfg.APIPort != test.want {
			t.Fatalf("parseConfig(%s): api port %d, want %d", test.line, cfg.APIPort, test.want)
		}
		if cfg.KeepStates != test.keepStates {
			t.Fatalf("parseConfig(%s): keep states %v, want %v", test.line, cfg.KeepStates, test.keepStates)
		}
	}
	for _, line := range []string{
		`{"api_port":65536}`, `{"api_port":-1}`, `{"api_port":1.5}`, `{"api_port":"9095"}`,
		`{"keep_states":1}`, `{"keep_states":"true"}`,
	} {
		if _, err := parseConfig(line); errorCode(err) != CodeConfig {
			t.Fatalf("parseConfig(%s): error %v, want a %s error", line, err, CodeConfig)
		}
	}
}
