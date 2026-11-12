package servicecheck

import "testing"

func TestNormalizeRegion(t *testing.T) {
	cases := []struct {
		name string
		code string
		want string
	}{
		{"alpha-2 lower", "us", "US"},
		{"alpha-3", "USA", "US"},
		{"alpha-3 DE", "deu", "DE"},
		{"not a country", "XX", ""},
		{"trim spaces", "  gb  ", "GB"},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			if got := NormalizeRegion(tc.code); got != tc.want {
				t.Fatalf("NormalizeRegion(%q) = %q, want %q", tc.code, got, tc.want)
			}
		})
	}
}
