package personalrecord

import (
	"context"
	"fmt"
	"ftns-asst/internal/model"
	"ftns-asst/internal/util"

	"github.com/google/uuid"
)

type PersonalRecordService struct {
	repo      PersonalRecordRepo
	refresher ProgramStateRefresher
	tx        model.Transactor
}

func NewService(repo PersonalRecordRepo, refresher ProgramStateRefresher, transactor model.Transactor) *PersonalRecordService {
	return &PersonalRecordService{
		repo:      repo,
		refresher: refresher,
		tx:        transactor,
	}
}

// CreateRecord adds a record; with reps the weight is a working set and the record is estimated by a formula
func (s *PersonalRecordService) CreateRecord(ctx context.Context, requesterID uuid.UUID, userID uuid.UUID, input *model.CreatePersonalRecordInput) (*model.PersonalRecord, error) {
	if input == nil {
		return nil, fmt.Errorf("create personal record input can not be nil")
	}

	records, err := s.CreateRecords(ctx, requesterID, userID, []*model.CreatePersonalRecordInput{input})
	if err != nil {
		return nil, err
	}
	return records[0], nil
}

// CreateRecords adds records of different exercises in one transaction: either all of them or none,
// the order of the result is the order of the inputs
func (s *PersonalRecordService) CreateRecords(ctx context.Context, requesterID uuid.UUID, userID uuid.UUID, inputs []*model.CreatePersonalRecordInput) ([]*model.PersonalRecord, error) {
	if requesterID != userID {
		return nil, model.ErrForbidden
	}
	if len(inputs) == 0 || len(inputs) > model.MaxPersonalRecordsInBulk {
		return nil, fmt.Errorf("number of records must be 1..%d: %w", model.MaxPersonalRecordsInBulk, model.ErrBadRequest)
	}

	// records of one exercise would have the same time, so the latest one would be undefined
	records := make([]*model.PersonalRecord, 0, len(inputs))
	exerciseIDs := make([]uuid.UUID, 0, len(inputs))
	seen := make(model.Set[uuid.UUID], len(inputs))
	for i, input := range inputs {
		if input == nil {
			return nil, fmt.Errorf("record %d is empty: %w", i, model.ErrBadRequest)
		}
		if _, ok := seen[input.ExerciseID]; ok {
			return nil, fmt.Errorf("record %d: exercise %s is repeated: %w", i, input.ExerciseID, model.ErrBadRequest)
		}
		seen[input.ExerciseID] = struct{}{}

		record, err := newRecord(userID, input)
		if err != nil {
			return nil, fmt.Errorf("record %d: %w", i, err)
		}
		records = append(records, record)
		exerciseIDs = append(exerciseIDs, input.ExerciseID)
	}

	res := make([]*model.PersonalRecord, 0, len(records))
	err := s.inTx(ctx, func(ctx context.Context) error {
		ok, err := s.repo.AreExercisesAvailable(ctx, userID, exerciseIDs)
		if err != nil {
			return fmt.Errorf("failed to check exercises: %w", err)
		}
		if !ok {
			return model.ProgramExerciseNotAvailable()
		}

		for _, record := range records {
			created, err := s.repo.CreateRecord(ctx, record)
			if err != nil {
				return fmt.Errorf("failed to create record: %w", err)
			}
			res = append(res, created)
		}

		// programs which waited for these records can start now
		if err = s.refresher.RefreshMeasurementStates(ctx, userID); err != nil {
			return fmt.Errorf("failed to refresh programs: %w", err)
		}
		return nil
	})
	if err != nil {
		return nil, err
	}
	return res, nil
}

// newRecord builds a record; with reps the weight is a working set and the record is estimated by a formula
func newRecord(userID uuid.UUID, input *model.CreatePersonalRecordInput) (*model.PersonalRecord, error) {
	record := &model.PersonalRecord{
		UserID:     userID,
		ExerciseID: input.ExerciseID,
		WeightKg:   input.WeightKg,
	}
	if input.Reps != nil {
		estimated, err := util.EstimateOneRepMax(input.WeightKg, *input.Reps)
		if err != nil {
			return nil, fmt.Errorf("%s: %w", err.Error(), model.ErrBadRequest)
		}
		record.WeightKg = estimated
		record.IsCalculated = *input.Reps > 1
	}
	if err := record.Validate(); err != nil {
		return nil, err
	}
	return record, nil
}

// run fn in a transaction, repositories take it from ctx
func (s *PersonalRecordService) inTx(ctx context.Context, fn func(ctx context.Context) error) error {
	tx, err := s.tx.Begin(ctx)
	if err != nil {
		return fmt.Errorf("failed begin transaction: %w", err)
	}
	defer model.CheckRollback(ctx, tx)
	ctx = context.WithValue(ctx, model.ContextKeyTx, tx)

	if err = fn(ctx); err != nil {
		return err
	}

	if err = tx.Commit(ctx); err != nil {
		return fmt.Errorf("failed commit transaction: %w", err)
	}
	return nil
}

// GetLatestRecords returns the current record of the user in each exercise
func (s *PersonalRecordService) GetLatestRecords(ctx context.Context, requesterID uuid.UUID, userID uuid.UUID) ([]*model.PersonalRecord, error) {
	if requesterID != userID {
		return nil, model.ErrForbidden
	}

	list, err := s.repo.GetLatestRecords(ctx, userID, nil)
	if err != nil {
		return nil, fmt.Errorf("failed to get latest records of user %s: %w", userID, err)
	}
	return list, nil
}

// GetRecordHistory returns all records of the user in the exercise, the newest first
func (s *PersonalRecordService) GetRecordHistory(ctx context.Context, requesterID uuid.UUID, userID uuid.UUID, exerciseID uuid.UUID) ([]*model.PersonalRecord, error) {
	if requesterID != userID {
		return nil, model.ErrForbidden
	}

	list, err := s.repo.GetRecordHistory(ctx, userID, exerciseID)
	if err != nil {
		return nil, fmt.Errorf("failed to get record history of user %s in exercise %s: %w", userID, exerciseID, err)
	}
	return list, nil
}
