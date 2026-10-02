package util

import (
	"crypto/rand"
)

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

func Dedup[T any, K comparable](slice []T, keyFunc func(t T) K) []T {
	if len(slice) == 0 {
		return slice
	}
	passed := make(map[K]struct{}, len(slice))
	res := make([]T, 0, len(slice))
	for _, item := range slice {
		key := keyFunc(item)
		if _, ok := passed[key]; ok {
			continue
		}
		passed[key] = struct{}{}
		res = append(res, item)
	}
	return res
}
