package dto

import (
	"time"

	"github.com/google/uuid"
)

type SaveWorkoutRequestBody struct {
	ProgramID   *uuid.UUID             `json:"program_id,omitempty"`
	WeekNumber  *int                   `json:"week_number,omitempty"`
	DayOfWeek   *int                   `json:"day_of_week,omitempty"`
	StartedAt   time.Time              `json:"started_at" binding:"required"`
	CompletedAt time.Time              `json:"completed_at" binding:"required"`
	Exercises   []WorkoutExerciseInput `json:"exercises" binding:"required,min=1,dive"`
} // @Name SaveWorkoutRequestBody

type WorkoutExerciseTargets struct {
	ExerciseID     uuid.UUID `json:"exercise_id" binding:"required"`
	TargetSets     *int      `json:"target_sets,omitempty"`
	TargetReps     *int      `json:"target_reps,omitempty"`
	TargetWeightKg *float64  `json:"target_weight_kg,omitempty"`
} // @Name WorkoutExerciseTargets

type WorkoutExerciseInput struct {
	WorkoutExerciseTargets
	Sets []WorkoutSetInput `json:"sets" binding:"required,min=1,dive"`
} // @Name WorkoutExerciseInput

type WorkoutSetInput struct {
	Reps     int      `json:"reps" binding:"required"`
	WeightKg *float64 `json:"weight_kg,omitempty"`
} // @Name WorkoutSetInput

type WorkoutSummaryDTO struct {
	ID          uuid.UUID  `json:"id"`
	ProgramID   *uuid.UUID `json:"program_id,omitempty"`
	WeekNumber  *int       `json:"week_number,omitempty"`
	DayOfWeek   *int       `json:"day_of_week,omitempty"`
	StartedAt   time.Time  `json:"started_at"`
	CompletedAt time.Time  `json:"completed_at"`
} // @Name WorkoutSummary

type WorkoutDTO struct {
	WorkoutSummaryDTO
	Exercises []WorkoutExerciseDTO `json:"exercises"`
} // @Name Workout

type WorkoutExerciseDTO struct {
	ID uuid.UUID `json:"id"`
	WorkoutExerciseTargets
	ExerciseName string          `json:"exercise_name"`
	Sets         []WorkoutSetDTO `json:"sets"`
} // @Name WorkoutExercise

type WorkoutSetDTO struct {
	ID        uuid.UUID `json:"id"`
	SetNumber int       `json:"set_number"`
	WorkoutSetInput
} // @Name WorkoutSet

type ListWorkoutsQueryParams struct {
	ProgramID *string `form:"program_id"`
	Limit     *int    `form:"limit" binding:"omitempty,min=1,max=1000"`
	Offset    *int    `form:"offset" binding:"omitempty,min=0"`
}
