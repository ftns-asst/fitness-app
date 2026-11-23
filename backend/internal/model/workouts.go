package model

import (
	"fmt"
	"math"
	"time"

	"github.com/google/uuid"
)

type WorkoutSummary struct {
	ID          uuid.UUID  `db:"id"`
	ProgramID   *uuid.UUID `db:"program_id"`
	WeekNumber  *int       `db:"week_number"`
	DayOfWeek   *int       `db:"day_of_week"`
	StartedAt   time.Time  `db:"started_at"`
	CompletedAt time.Time  `db:"completed_at"`
}

type Workout struct {
	WorkoutSummary
	Exercises []WorkoutExercise
}

type WorkoutExercise struct {
	ID           uuid.UUID
	ExerciseName string
	WorkoutExerciseTargets
	Sets []WorkoutSet
}

type WorkoutExerciseTargets struct {
	ExerciseID     uuid.UUID
	TargetSets     *int
	TargetReps     *int
	TargetWeightKg *float64
}

type WorkoutSet struct {
	ID        uuid.UUID
	SetNumber int
	WorkoutSetInput
}

type WorkoutSetInput struct {
	Reps     int
	WeightKg *float64
}

type WorkoutExerciseInput struct {
	WorkoutExerciseTargets
	Sets []WorkoutSetInput
}

// Array positions determine exercise order and set numbers.
type SaveWorkoutInput struct {
	ProgramID   *uuid.UUID
	WeekNumber  *int
	DayOfWeek   *int
	StartedAt   time.Time
	CompletedAt time.Time
	Exercises   []WorkoutExerciseInput
}

func (w *SaveWorkoutInput) Validate() error {
	if w == nil || w.StartedAt.IsZero() || w.CompletedAt.IsZero() || w.CompletedAt.Before(w.StartedAt) {
		return fmt.Errorf("invalid workout time interval: %w", ErrBadRequest)
	}
	if w.ProgramID != nil {
		if *w.ProgramID == uuid.Nil || w.WeekNumber == nil || *w.WeekNumber < 1 || *w.WeekNumber > MaxProgramWeeks ||
			w.DayOfWeek == nil || *w.DayOfWeek < MinDayOfWeek || *w.DayOfWeek > MaxDayOfWeek {
			return fmt.Errorf("invalid program, week or weekday: %w", ErrBadRequest)
		}
	} else if w.WeekNumber != nil || w.DayOfWeek != nil {
		return fmt.Errorf("program is required with a week or weekday: %w", ErrBadRequest)
	}
	if len(w.Exercises) == 0 {
		return fmt.Errorf("at least one completed exercise is required: %w", ErrBadRequest)
	}
	for i, e := range w.Exercises {
		if e.ExerciseID == uuid.Nil || len(e.Sets) == 0 {
			return fmt.Errorf("exercise %d requires an exercise id and completed sets: %w", i, ErrBadRequest)
		}
		if w.ProgramID == nil && (e.TargetSets != nil || e.TargetReps != nil || e.TargetWeightKg != nil) {
			return fmt.Errorf("exercise %d has program targets without a program: %w", i, ErrBadRequest)
		}
		if e.TargetSets != nil && (*e.TargetSets < MinExerciseSets || *e.TargetSets > MaxExerciseSets) ||
			e.TargetReps != nil && (*e.TargetReps <= 0 || *e.TargetReps > math.MaxInt32) || !validWorkoutWeight(e.TargetWeightKg) {
			return fmt.Errorf("invalid targets for exercise %d: %w", i, ErrBadRequest)
		}
		for j, set := range e.Sets {
			if set.Reps <= 0 || set.Reps > math.MaxInt32 || !validWorkoutWeight(set.WeightKg) {
				return fmt.Errorf("invalid reps or weight for exercise %d set %d: %w", i, j+1, ErrBadRequest)
			}
		}
	}
	return nil
}

func validWorkoutWeight(kg *float64) bool {
	return kg == nil || (*kg >= 0 && *kg < 10000 && math.Round(*kg*100)/100 == *kg)
}
