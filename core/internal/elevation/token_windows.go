package elevation

import "golang.org/x/sys/windows"

// Not GetCurrentProcessToken: that pseudo-handle needs Windows 8, and on Windows 7 every query through it fails.
func ownToken() (windows.Token, error) {
	var token windows.Token
	err := windows.OpenProcessToken(windows.CurrentProcess(), windows.TOKEN_QUERY, &token)
	return token, err
}

// Elevated reports whether this process runs with an elevated token.
func Elevated() bool {
	token, err := ownToken()
	if err != nil {
		return false
	}
	defer token.Close()
	return token.IsElevated()
}

func ownTokenUser() (*windows.Tokenuser, error) {
	token, err := ownToken()
	if err != nil {
		return nil, err
	}
	defer token.Close()
	return token.GetTokenUser()
}
