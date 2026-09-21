package util

import (
	"context"
	"crypto/rand"
	"ftns-asst/internal/model"

	"github.com/google/uuid"
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

func GetUserIDFromCtx(ctx context.Context) (uuid.UUID, bool) {
	u, ok := ctx.Value(model.ContextKeyUserID).(uuid.UUID)
	return u, ok
}
