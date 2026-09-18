package util

import "crypto/rand"

// returns pointer to value
func Ptr[T any](value T) *T {
	return &value
}

func UnPtr[T any](ptr *T) T {
	if ptr == nil {
		var zero T
		return zero
	}
	return *ptr
}

func RandStr() string {
	return rand.Text()
}
