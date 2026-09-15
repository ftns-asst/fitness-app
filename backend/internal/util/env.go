package util

import "github.com/caarlos0/env/v11"

// reads struct type from environment variables,
// panics on error
func MustReadFromEnv[T any]() T {
	return env.Must(env.ParseAs[T]())
}
