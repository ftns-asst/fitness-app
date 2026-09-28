package ctxt

import (
	"context"
	"ftns-asst/internal/model"

	"github.com/google/uuid"
)

func GetUserIDFromCtx(ctx context.Context) (uuid.UUID, bool) {
	u, ok := ctx.Value(model.ContextKeyUserID).(uuid.UUID)
	return u, ok
}
