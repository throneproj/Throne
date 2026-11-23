//go:build !with_psiphon

package mobile

import "context"

func withPsiphonServerList(ctx context.Context) context.Context {
	return ctx
}
