package rpc

func To[T any](v T) *T {
	return &v
}
