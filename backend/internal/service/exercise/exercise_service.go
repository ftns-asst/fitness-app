package exercise

import (
	"context"
	"errors"
	"fmt"
	"ftns-asst/internal/config"
	"ftns-asst/internal/model"
	"ftns-asst/internal/repository"
	"maps"
	"slices"

	"github.com/google/uuid"
)

type ExerciseService struct {
	cfg  *config.Config
	repo ExerciseRepo
	tx   model.Transactor
}

func NewService(cfg *config.Config,
	repo ExerciseRepo,
	transactor model.Transactor,
) *ExerciseService {
	return &ExerciseService{
		cfg:  cfg,
		repo: repo,
		tx:   transactor,
	}
}

func (r *ExerciseService) Start(ctx context.Context) error {
	// TODO: add loading from file
	err := r.fillExercises(ctx, model.TestExercises)
	if err != nil {
		return fmt.Errorf("failed to fill exercises: %w", err)
	}

	return nil
}

func (s *ExerciseService) GetMuscleGroups() []model.MuscleGroup {
	mg := model.MuscleGroupList
	return slices.Collect(maps.Keys(mg))
}

func (s *ExerciseService) GetEquipment() []model.Equipment {
	mg := model.EquipmentList
	return slices.Collect(maps.Keys(mg))
}

func (s *ExerciseService) CreateExercise(ctx context.Context, userID uuid.UUID, exercise *model.Exercise) (*model.Exercise, error) {
	created, err := s.repo.CreateExercise(ctx, exercise)
	if err != nil {
		return nil, fmt.Errorf("failed to create exercise: %w", err)
	}
	return created, nil
}

func (s *ExerciseService) fillExercises(ctx context.Context, exercises []*model.Exercise) error {
	err := s.repo.FillExercises(ctx, exercises)
	if err != nil {
		return fmt.Errorf("failed to fill exercises: %w", err)
	}
	return nil
}

func (s *ExerciseService) GetExerciseByID(ctx context.Context, userID uuid.UUID, exerciseID uuid.UUID) (*model.Exercise, error) {
	exercise, err := s.getExerciseByID(ctx, exerciseID)
	if err != nil {
		return nil, err
	}

	if exercise.OwnerID != nil && userID != *exercise.OwnerID {
		return nil, model.ErrForbidden
	}

	return exercise, nil
}

func (s *ExerciseService) getExerciseByID(ctx context.Context, id uuid.UUID) (*model.Exercise, error) {
	exercise, err := s.repo.GetExerciseByID(ctx, id)
	if err != nil {
		return nil, fmt.Errorf("failed to get exercise with id %s: %w", id.String(), err)
	}
	return exercise, nil
}

func (s *ExerciseService) GetExercises(ctx context.Context, filters *model.ExerciseFilters, page *model.PageInfo) ([]*model.Exercise, error) {
	err := filters.ValidateAndDedup()
	if errors.Is(err, model.ErrUserIDRequired) {
		if filters.ListType == model.ExerciseListTypeOwn {
			return nil, model.ExercisesFilterUserIDRequired()
		}
		return nil, model.ExercisesFilterUserIDRequired()
	}
	if err != nil {
		return nil, fmt.Errorf("failed to validate filters: %w", err)
	}

	page = page.Normalize()

	list, err := s.repo.GetExercises(ctx, filters, page)
	if err != nil {
		return nil, fmt.Errorf("failed to get exercises: %w", err)
	}
	return list, nil
}

func (s *ExerciseService) UpdateExercise(ctx context.Context, userID uuid.UUID, input *model.UpdateExerciseInput) (*model.Exercise, error) {
	if input == nil {
		return nil, fmt.Errorf("update exercise input can't be nil")
	}
	ok, err := s.repo.IsUserOwnerCheck(ctx, userID, input.ID)
	if err != nil {
		return nil, fmt.Errorf("failed to check owner of exercise: %w", err)
	}
	if !ok {
		return nil, model.ErrForbidden
	}

	return s.updateExercise(ctx, input)
}

func (s *ExerciseService) updateExercise(ctx context.Context, input *model.UpdateExerciseInput) (*model.Exercise, error) {
	if input == nil {
		return nil, fmt.Errorf("update exercise input can't be nil")
	}

	updated, err := s.repo.UpdateExercise(ctx, input)
	if errors.Is(err, repository.ErrNotFound) {
		return nil, fmt.Errorf("not found exercise to update with id: %s, err: %w", input.ID, model.ErrNotFound)
	}
	if err != nil {
		return nil, fmt.Errorf("failed to update exercise with id: %s, err: %w", input.ID, err)
	}

	return updated, nil
}

func (s *ExerciseService) DeleteExercise(ctx context.Context, userID uuid.UUID, id uuid.UUID) error {
	ok, err := s.repo.IsUserOwnerCheck(ctx, userID, id)
	if err != nil {
		return fmt.Errorf("failed to check owner of exercise: %w", err)
	}
	if !ok {
		return model.ErrForbidden
	}

	return s.deleteExercise(ctx, id)
}

func (s *ExerciseService) deleteExercise(ctx context.Context, id uuid.UUID) error {
	err := s.repo.DeleteExercise(ctx, id)
	if errors.Is(err, repository.ErrNoAffectedRows) {
		return fmt.Errorf("not found exercise to delete with id: %s, err: %w", id, model.ErrNotFound)
	}
	if err != nil {
		return fmt.Errorf("failed to delete exercise with id: %s, err: %w", id, err)
	}

	return nil
}
