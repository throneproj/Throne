//go:build !with_psiphon

package boxmain

import "context"

func withPsiphonServerList(ctx context.Context) context.Context {
	return ctx
}
