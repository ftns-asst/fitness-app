package exercise

import (
	"context"
	"errors"
	"fmt"
	"ftns-asst/internal/config"
	"ftns-asst/internal/model"
	"ftns-asst/internal/repository"

	"github.com/google/uuid"
)

type ExerciseService struct {
	cfg                 *config.Config
	repo                ExerciseRepo
	media               MediaProvider
	muscleGroupProvider model.MuscleGroupProvider
	equipmentProvider   model.EquipmentProvider
	tx                  model.Transactor
}

func NewService(cfg *config.Config,
	repo ExerciseRepo,
	media MediaProvider,
	transactor model.Transactor,
) *ExerciseService {
	return &ExerciseService{
		cfg:   cfg,
		repo:  repo,
		media: media,
		tx:    transactor,
	}
}

func (s *ExerciseService) Start(ctx context.Context) error {
	// TODO: add loading from file
	err := s.fillExercises(ctx)
	if err != nil {
		return fmt.Errorf("failed to fill exercises: %w", err)
	}

	return nil
}

func (s *ExerciseService) GetMuscleGroups() []model.MuscleGroup {
	return s.muscleGroupProvider.List()
}

func (s *ExerciseService) GetEquipment() []model.Equipment {
	return s.equipmentProvider.List()
}

func (s *ExerciseService) CreateExercise(ctx context.Context, userID uuid.UUID, exercise *model.Exercise) (*model.Exercise, error) {
	if err := exercise.Validate(s.muscleGroupProvider, s.equipmentProvider); err != nil {
		return nil, err
	}

	created, err := s.repo.CreateExercise(ctx, exercise)
	if err != nil {
		return nil, fmt.Errorf("failed to create exercise: %w", err)
	}
	return created, nil
}

func (s *ExerciseService) fillExercises(ctx context.Context) error {

	loaded, err := s.loadExercises(ctx)
	if err != nil {
		return fmt.Errorf("failed to load exercises: %w", err)
	}
	s.muscleGroupProvider = loaded.muscleGroups
	s.equipmentProvider = loaded.equipment

	err = s.repo.FillExercises(ctx, loaded.exercises)
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

	if !checkUserCanViewExercise(userID, exercise) {
		return nil, model.ErrForbidden
	}

	if err = s.fillMedia(ctx, exercise); err != nil {
		return nil, err
	}
	return exercise, nil
}

func checkUserCanViewExercise(userID uuid.UUID, exercise *model.Exercise) bool {
	if exercise.OwnerID != nil && !exercise.IsPublic && userID != *exercise.OwnerID {
		return false
	}
	return true
}

func (s *ExerciseService) getExerciseByID(ctx context.Context, id uuid.UUID) (*model.Exercise, error) {
	exercise, err := s.repo.GetExerciseByID(ctx, id)
	if errors.Is(err, repository.ErrNotFound) {
		return nil, model.ErrNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("failed to get exercise with id %s: %w", id.String(), err)
	}
	return exercise, nil
}

// fills Media of exercises using file service, exercises without media_id are skipped
func (s *ExerciseService) fillMedia(ctx context.Context, exercises ...*model.Exercise) error {
	ids := make([]uuid.UUID, 0, len(exercises))
	for _, ex := range exercises {
		if ex.MediaID != nil {
			ids = append(ids, *ex.MediaID)
		}
	}
	if len(ids) == 0 {
		return nil
	}

	media, err := s.media.GetMediaList(ctx, ids)
	if err != nil {
		return fmt.Errorf("failed to get exercises media: %w", err)
	}
	for _, ex := range exercises {
		if ex.MediaID != nil {
			ex.Media = media[*ex.MediaID]
		}
	}
	return nil
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
	if err = s.fillMedia(ctx, list...); err != nil {
		return nil, err
	}
	return list, nil
}

func (s *ExerciseService) UpdateExercise(ctx context.Context, userID uuid.UUID, input *model.UpdateExerciseInput) (*model.Exercise, error) {
	if input == nil {
		return nil, fmt.Errorf("update exercise input can't be nil")
	}
	if err := input.Validate(s.muscleGroupProvider, s.equipmentProvider); err != nil {
		return nil, err
	}

	ok, err := s.repo.IsUserOwnerCheck(ctx, userID, input.ID)
	if errors.Is(err, repository.ErrNotFound) {
		return nil, model.ErrNotFound
	}
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

	if err = s.fillMedia(ctx, updated); err != nil {
		return nil, err
	}
	return updated, nil
}

func (s *ExerciseService) DeleteExercise(ctx context.Context, userID uuid.UUID, id uuid.UUID) error {
	ok, err := s.repo.IsUserOwnerCheck(ctx, userID, id)
	if errors.Is(err, repository.ErrNotFound) {
		return model.ErrNotFound
	}
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

// returns url for uploading media of the exercise, creates media if exercise has none
func (s *ExerciseService) UpdateMedia(ctx context.Context, exerciseID uuid.UUID, mediaType model.MediaType) (*model.Media, string, error) {
	exercise, err := s.getExerciseByID(ctx, exerciseID)
	if err != nil {
		return nil, "", err
	}

	tx, err := s.tx.Begin(ctx)
	if err != nil {
		return nil, "", fmt.Errorf("failed begin transaction: %w", err)
	}
	defer model.CheckRollback(ctx, tx)
	ctx = context.WithValue(ctx, model.ContextKeyTx, tx)

	media, putURL, err := s.media.UpsertMedia(ctx, exercise.MediaID, mediaType)
	if err != nil {
		return nil, "", fmt.Errorf("failed to upsert media of exercise: %w", err)
	}

	if exercise.MediaID == nil {
		err = s.repo.UpdateMediaID(ctx, exerciseID, media.ID)
		if errors.Is(err, repository.ErrNoAffectedRows) {
			return nil, "", fmt.Errorf("not found exercise to set media with id: %s, err: %w", exerciseID, model.ErrNotFound)
		}
		if err != nil {
			return nil, "", fmt.Errorf("failed to set media of exercise: %w", err)
		}
	}

	if err = tx.Commit(ctx); err != nil {
		return nil, "", fmt.Errorf("failed to commit transaction: %w", err)
	}
	return media, putURL, nil
}

func (s *ExerciseService) DeleteMedia(ctx context.Context, exerciseID uuid.UUID) error {
	exercise, err := s.getExerciseByID(ctx, exerciseID)
	if err != nil {
		return err
	}

	if exercise.MediaID == nil {
		return nil
	}

	err = s.media.DeleteMedia(ctx, *exercise.MediaID)
	if err != nil && !errors.Is(err, model.ErrNotFound) {
		return fmt.Errorf("failed to delete media of exercise: %w", err)
	}
	return nil
}
