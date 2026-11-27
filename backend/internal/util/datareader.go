package util

import (
	"maps"
	"slices"
)

type (
	DataReader[T any] interface {
		Get(string) (T, bool)
		List() []T
	}
	dataReader[T any] struct {
		data map[string]T
	}
)

func NewDataReader[T any](items []T, keyFunc func(T) string) *dataReader[T] {
	data := make(map[string]T, len(items))
	for _, item := range items {
		data[keyFunc(item)] = item
	}
	return &dataReader[T]{
		data: data,
	}
}

func (r *dataReader[T]) Get(key string) (T, bool) {
	v, ok := r.data[key]
	return v, ok
}

func (r *dataReader[T]) List() []T {
	return slices.Collect(maps.Values(r.data))
}
