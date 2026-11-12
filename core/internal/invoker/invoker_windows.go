package invoker

import (
	"errors"
	"fmt"
	"sync"
	"unsafe"

	"ThroneCore/internal/elevation"
	"ThroneCore/internal/process"

	"golang.org/x/sys/windows"
)

type identity struct{}

func boundary() bool { return elevation.IsAttached() }

func invokingUser() (identity, bool) { return identity{}, elevation.IsAttached() }

var (
	tokenMu sync.Mutex
	token   windows.Token
)

func userToken() (windows.Token, error) {
	tokenMu.Lock()
	defer tokenMu.Unlock()
	if token == 0 {
		tok, err := process.UserImpersonationToken()
		if err != nil {
			return 0, err
		}
		token = tok
	}
	return token, nil
}

func threadToken() (windows.Token, error) {
	var tok windows.Token
	err := windows.OpenThreadToken(windows.CurrentThread(), windows.TOKEN_QUERY, true, &tok)
	return tok, err
}

func (identity) assume() error {
	tok, err := userToken()
	if err != nil {
		return fmt.Errorf("build the user's token: %w", err)
	}
	if err = windows.SetThreadToken(nil, tok); err != nil {
		return fmt.Errorf("SetThreadToken: %w", err)
	}
	current, err := threadToken()
	if err != nil {
		return fmt.Errorf("open the thread token: %w", err)
	}
	defer current.Close()
	// Without SeImpersonatePrivilege the kernel silently degrades the token to identification level.
	var level, n uint32
	if err = windows.GetTokenInformation(current, windows.TokenImpersonationLevel,
		(*byte)(unsafe.Pointer(&level)), uint32(unsafe.Sizeof(level)), &n); err != nil {
		return fmt.Errorf("read the impersonation level: %w", err)
	}
	if level < windows.SecurityImpersonation {
		return fmt.Errorf("impersonation level %d is too low", level)
	}
	if current.IsElevated() {
		return errors.New("the thread token is elevated")
	}
	return nil
}

func (identity) revert() error {
	if err := windows.RevertToSelf(); err != nil {
		return err
	}
	tok, err := threadToken()
	if err == nil {
		tok.Close()
		return errors.New("the thread still impersonates")
	}
	if !errors.Is(err, windows.ERROR_NO_TOKEN) {
		return fmt.Errorf("open the thread token: %w", err)
	}
	return nil
}
