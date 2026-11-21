package personalrecord

import (
	"context"
	"ftns-asst/internal/model"

	"github.com/google/uuid"
)

type PersonalRecordRepo interface {
	CreateRecord(ctx context.Context, entry *model.PersonalRecord) (*model.PersonalRecord, error)
	GetLatestRecords(ctx context.Context, userID uuid.UUID, exerciseIDs []uuid.UUID) ([]*model.PersonalRecord, error)
	GetRecordHistory(ctx context.Context, userID uuid.UUID, exerciseID uuid.UUID) ([]*model.PersonalRecord, error)
	AreExercisesAvailable(ctx context.Context, userID uuid.UUID, exerciseIDs []uuid.UUID) (bool, error)
}

// ProgramStateRefresher updates states of programs which wait for personal records
type ProgramStateRefresher interface {
	RefreshMeasurementStates(ctx context.Context, userID uuid.UUID) error
}
