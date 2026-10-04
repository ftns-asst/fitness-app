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
}
