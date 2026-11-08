package model

import (
	"fmt"

	"github.com/google/uuid"
)

type (
	MediaType string
	Media     struct {
		ID        uuid.UUID `db:"id"`
		Type      MediaType `db:"type"`
		ObjectKey string    `db:"object_key"`
		URL       string    `db:"-"` // temporary get url, filled by file service
	}
)

const (
	MediaTypeImage MediaType = "image"
	MediaTypeVideo MediaType = "video"
)

func ExerciseMediaObjectName(exerciseID uuid.UUID) string {
	return fmt.Sprintf("exercise_%s", exerciseID.String())
}
