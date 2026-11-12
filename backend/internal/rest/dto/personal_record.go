package dto

import (
	"time"

	"github.com/google/uuid"
)

type PersonalRecordDTO struct {
	ID           uuid.UUID `json:"id"`
	ExerciseID   uuid.UUID `json:"exercise_id"`
	ExerciseName string    `json:"exercise_name"`
	WeightKg     float64   `json:"weight_kg"`
	IsCalculated bool      `json:"is_calculated"` // estimated by a formula, not lifted for one rep
	CreatedAt    time.Time `json:"created_at"`
} // @Name PersonalRecord

type CreatePersonalRecordRequestBody struct {
	ExerciseID uuid.UUID `json:"exercise_id" binding:"required"`
	WeightKg   float64   `json:"weight_kg" binding:"required"`
	// repetitions of the set; with them weight_kg is the weight of the set and the record is estimated by a formula
	Reps *int `json:"reps,omitempty"`
} // @Name CreatePersonalRecordRequestBody

type CreatePersonalRecordsRequestBody struct {
	// records of different exercises, from 1 to 100
	Records []CreatePersonalRecordRequestBody `json:"records" binding:"required,min=1,max=100,dive"`
} // @Name CreatePersonalRecordsRequestBody
