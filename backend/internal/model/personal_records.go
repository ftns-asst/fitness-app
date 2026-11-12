package model

import (
	"fmt"
	"time"

	"github.com/google/uuid"
)

const (
	MaxPersonalRecordWeightKg = 10000
	MaxPersonalRecordsInBulk  = 100
)

// PersonalRecord is a personal record (PR) of the user in the exercise, new entries are added and old ones are kept,
// the latest entry is the current record
type PersonalRecord struct {
	ID           uuid.UUID `db:"id"`
	UserID       uuid.UUID `db:"user_id"`
	ExerciseID   uuid.UUID `db:"exercise_id"`
	ExerciseName string    `db:"exercise_name"`
	WeightKg     float64   `db:"weight_kg"`
	IsCalculated bool      `db:"is_calculated"` // estimated by a formula, not lifted for one rep
	CreatedAt    time.Time `db:"created_at"`
}

// CreatePersonalRecordInput is a record to add; with Reps the weight is a working set and the max is estimated by a formula
type CreatePersonalRecordInput struct {
	ExerciseID uuid.UUID
	WeightKg   float64
	Reps       *int
}

func (m *PersonalRecord) Validate() error {
	if m.ExerciseID == uuid.Nil {
		return fmt.Errorf("exercise_id is required: %w", ErrBadRequest)
	}
	if m.WeightKg <= 0 || m.WeightKg >= MaxPersonalRecordWeightKg {
		return fmt.Errorf("weight_kg must be greater than 0 and less than %d: %w", MaxPersonalRecordWeightKg, ErrBadRequest)
	}
	return nil
}
