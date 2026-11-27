package exercise

import (
	"context"
	"ftns-asst/internal/model"

	"github.com/google/uuid"
)

type ExerciseRepo interface {
	CreateExercise(ctx context.Context, exercise *model.Exercise) (*model.Exercise, error)
	FillExercises(ctx context.Context, exercises []*model.Exercise) error
	GetExerciseByID(ctx context.Context, id uuid.UUID) (*model.Exercise, error)
	GetExercises(ctx context.Context, filters *model.ExerciseFilters, page *model.PageInfo) ([]*model.Exercise, error)
	UpdateExercise(ctx context.Context, input *model.UpdateExerciseInput) (*model.Exercise, error)
	DeleteExercise(ctx context.Context, id uuid.UUID) error
	UpdateMediaID(ctx context.Context, id uuid.UUID, mediaID uuid.UUID) error
	IsUserOwnerCheck(ctx context.Context, userID uuid.UUID, exerciseID uuid.UUID) (bool, error)
}

// provides media with get urls, implemented by file service
type MediaProvider interface {
	GetMediaList(ctx context.Context, ids []uuid.UUID) (map[uuid.UUID]*model.Media, error)
	DeleteMedia(ctx context.Context, id uuid.UUID) error
	UpsertMedia(ctx context.Context, id *uuid.UUID, mediaType model.MediaType) (*model.Media, string, error)
}
