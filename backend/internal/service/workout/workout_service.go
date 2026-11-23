package workout

import (
	"context"
	"errors"
	"fmt"

	"ftns-asst/internal/model"
	"ftns-asst/internal/repository"

	"github.com/google/uuid"
)

type WorkoutRepo interface {
	SaveWorkout(ctx context.Context, userID, id uuid.UUID, input model.SaveWorkoutInput) (*model.Workout, bool, error)
	GetWorkout(ctx context.Context, userID, id uuid.UUID) (*model.Workout, error)
	ListWorkouts(ctx context.Context, userID uuid.UUID, programID *uuid.UUID, page model.PageInfo) ([]model.WorkoutSummary, error)
}

type RecordUpdater interface {
	UpdateFromWorkout(ctx context.Context, userID uuid.UUID, workout *model.Workout) error
}

type WorkoutService struct {
	repo    WorkoutRepo
	records RecordUpdater
	tx      model.Transactor
}

func NewService(repo WorkoutRepo, records RecordUpdater, tx model.Transactor) *WorkoutService {
	return &WorkoutService{repo: repo, records: records, tx: tx}
}

// SaveWorkout returns created=false on an identical replay. userID is the authenticated user.
func (s *WorkoutService) SaveWorkout(ctx context.Context, userID, id uuid.UUID, input *model.SaveWorkoutInput) (*model.Workout, bool, error) {
	if id == uuid.Nil {
		return nil, false, fmt.Errorf("workout id is required: %w", model.ErrBadRequest)
	}
	if err := input.Validate(); err != nil {
		return nil, false, err
	}
	var workout *model.Workout
	var created bool
	err := s.tx.WithinTx(ctx, func(ctx context.Context) error {
		var err error
		workout, created, err = s.repo.SaveWorkout(ctx, userID, id, *input)
		if err != nil || !created {
			return err
		}
		return s.records.UpdateFromWorkout(ctx, userID, workout)
	})
	if errors.Is(err, repository.ErrNotFound) {
		return nil, false, model.ErrNotFound
	}
	if err != nil {
		return nil, false, err
	}
	return workout, created, nil
}

func (s *WorkoutService) GetWorkout(ctx context.Context, userID, id uuid.UUID) (*model.Workout, error) {
	workout, err := s.repo.GetWorkout(ctx, userID, id)
	if errors.Is(err, repository.ErrNotFound) {
		return nil, model.ErrNotFound
	}
	return workout, err
}

func (s *WorkoutService) ListWorkouts(ctx context.Context, userID uuid.UUID, programID *uuid.UUID, page model.PageInfo) ([]model.WorkoutSummary, error) {
	return s.repo.ListWorkouts(ctx, userID, programID, page)
}
