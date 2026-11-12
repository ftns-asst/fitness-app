package model

import (
	"fmt"
	"slices"
	"strings"
	"time"
	"unicode/utf8"

	"github.com/google/uuid"
)

type (
	ProgramGoal       string
	ProgramLevel      string
	WorkoutFocus      string
	ProgramSourceKind string
	ProgramState      string

	// template for default programs
	WorkoutTemplate struct {
		ID         uuid.UUID                  `db:"id"`
		Name       string                     `db:"name"`
		Focus      WorkoutFocus               `db:"focus"`
		Level      ProgramLevel               `db:"level"`
		Equipments []EquipmentKey             `db:"equipment"`
		CreatedAt  time.Time                  `db:"created_at"`
		Exercises  []*WorkoutTemplateExercise `db:"-"`
	}

	WorkoutTemplateExercise struct {
		ID                uuid.UUID `db:"id"`
		WorkoutTemplateID uuid.UUID `db:"workout_template_id"`
		ExerciseID        uuid.UUID `db:"exercise_id"`
		ExerciseName      string    `db:"exercise_name"`
		OrderIndex        int       `db:"order_index"`
		Sets              int       `db:"sets"`
		TargetReps        int       `db:"target_reps"`
		TargetPercent1RM  *float64  `db:"target_percent_1rm"` // load as percent of the personal record
		RestSeconds       *int      `db:"rest_seconds"`
	}

	// default built-in program
	DefaultProgram struct {
		ID          uuid.UUID            `db:"id"`
		Name        string               `db:"name"`
		Description *string              `db:"description"`
		Goal        ProgramGoal          `db:"goal"`
		Level       ProgramLevel         `db:"level"`
		DaysPerWeek int                  `db:"days_per_week"`
		WeeksCount  int                  `db:"weeks_count"`
		CreatedAt   time.Time            `db:"created_at"`
		Days        []*DefaultProgramDay `db:"-"`
	}

	// DefaultProgramDay is a workout of a week, order index is its position in the week
	DefaultProgramDay struct {
		ID                uuid.UUID        `db:"id"`
		DefaultProgramID  uuid.UUID        `db:"default_program_id"`
		WeekNumber        int              `db:"week_number"`
		OrderIndex        int              `db:"order_index"`
		WorkoutTemplateID uuid.UUID        `db:"workout_template_id"`
		WorkoutTemplate   *WorkoutTemplate `db:"-"`
	}

	// filters for default programs
	DefaultProgramFilters struct {
		DaysPerWeek *int
		Goal        *ProgramGoal
		Level       *ProgramLevel
	}

	// user program, may be created from scratch, or choosen from default programs
	UserProgram struct {
		ID               uuid.UUID              `db:"id"`
		UserID           uuid.UUID              `db:"user_id"`
		Name             string                 `db:"name"`
		SourceKind       ProgramSourceKind      `db:"source_kind"`
		DefaultProgramID *uuid.UUID             `db:"default_program_id"`
		Goal             ProgramGoal            `db:"goal"`
		Level            ProgramLevel           `db:"level"`
		State            ProgramState           `db:"state"`
		DaysOfWeek       []int                  `db:"days_of_week"` // training days, the same in every week
		WeeksCount       int                    `db:"weeks_count"`
		StartDate        *time.Time             `db:"start_date"`
		CreatedAt        time.Time              `db:"created_at"`
		UpdatedAt        time.Time              `db:"updated_at"`
		Exercises        []*UserProgramExercise `db:"-"`
		// exercises with percent load and without a personal record, only for draft and needs_measurement
		RequiredMeasurements []*MeasurementExercise `db:"-"`
	}

	// MeasurementExercise is an exercise which personal record must be measured before the program starts
	MeasurementExercise struct {
		ExerciseID   uuid.UUID `db:"exercise_id"`
		ExerciseName string    `db:"exercise_name"`
	}

	UserProgramExercise struct {
		ID             uuid.UUID `db:"id"`
		ProgramID      uuid.UUID `db:"program_id"`
		WeekNumber     int       `db:"week_number"`
		DayOfWeek      int       `db:"day_of_week"`
		ExerciseID     uuid.UUID `db:"exercise_id"`
		ExerciseName   string    `db:"exercise_name"`
		OrderIndex     int       `db:"order_index"`
		Sets           int       `db:"sets"`
		TargetReps     int       `db:"target_reps"`
		TargetWeightKg *float64  `db:"target_weight_kg"`
		// load as percent of the personal record, either weight or percent is set
		TargetPercent1RM *float64 `db:"target_percent_1rm"`
		RestSeconds      *int     `db:"rest_seconds"`
	}

	// UserProgramDay is exercises of one day of week in a week to save
	UserProgramDay struct {
		DayOfWeek int
		Exercises []*UserProgramExercise
	}

	// UserProgramWeek is days of one week to save
	UserProgramWeek struct {
		Days []*UserProgramDay
	}

	// UserProgramStructure is the whole program to save, position in Weeks is week number - 1
	UserProgramStructure struct {
		DaysOfWeek []int
		Weeks      []*UserProgramWeek
	}

	CreateUserProgramInput struct {
		Name       string
		Goal       ProgramGoal
		Level      ProgramLevel
		DaysOfWeek []int
		WeeksCount *int
	}

	// UpdateUserProgramInput changes only set fields; DaysOfWeek nil is unchanged, empty clears days
	UpdateUserProgramInput struct {
		ID         uuid.UUID
		Name       *string
		Goal       *ProgramGoal
		Level      *ProgramLevel
		DaysOfWeek []int
		WeeksCount *int
	}

	StartProgramResult struct {
		Program           *UserProgram
		FinishedProgramID *uuid.UUID
	}
)

const (
	ProgramGoalStrength  ProgramGoal = "strength"
	ProgramGoalMuscle    ProgramGoal = "muscle"
	ProgramGoalFitness   ProgramGoal = "fitness"
	ProgramGoalFatLoss   ProgramGoal = "fat_loss"
	ProgramGoalEndurance ProgramGoal = "endurance"

	ProgramLevelBeginner     ProgramLevel = "beginner"
	ProgramLevelIntermediate ProgramLevel = "intermediate"
	ProgramLevelAdvanced     ProgramLevel = "advanced"

	WorkoutFocusFullBody WorkoutFocus = "full_body"
	WorkoutFocusUpper    WorkoutFocus = "upper"
	WorkoutFocusLower    WorkoutFocus = "lower"
	WorkoutFocusPush     WorkoutFocus = "push"
	WorkoutFocusPull     WorkoutFocus = "pull"
	WorkoutFocusLegs     WorkoutFocus = "legs"

	ProgramSourceKindDefault ProgramSourceKind = "default"
	ProgramSourceKindCustom  ProgramSourceKind = "custom"

	ProgramStateDraft            ProgramState = "draft"
	ProgramStateNeedsMeasurement ProgramState = "needs_measurement" // draft which can not start without personal records
	ProgramStateInProgress       ProgramState = "in_progress"
	ProgramStateFinished         ProgramState = "finished"
)

const (
	MaxProgramNameLength = 255
	MaxProgramWeeks      = 52
	MaxTargetPercent1RM  = 100
	MinDayOfWeek         = 1
	MaxDayOfWeek         = 7
	MinExerciseSets      = 1
	MaxExerciseSets      = 20
)

func (g ProgramGoal) IsValid() bool {
	switch g {
	case ProgramGoalStrength, ProgramGoalMuscle, ProgramGoalFitness, ProgramGoalFatLoss, ProgramGoalEndurance:
		return true
	}
	return false
}

func (l ProgramLevel) IsValid() bool {
	switch l {
	case ProgramLevelBeginner, ProgramLevelIntermediate, ProgramLevelAdvanced:
		return true
	}
	return false
}

func (s ProgramState) IsValid() bool {
	switch s {
	case ProgramStateDraft, ProgramStateNeedsMeasurement, ProgramStateInProgress, ProgramStateFinished:
		return true
	}
	return false
}

func (f WorkoutFocus) IsValid() bool {
	switch f {
	case WorkoutFocusFullBody, WorkoutFocusUpper, WorkoutFocusLower, WorkoutFocusPush, WorkoutFocusPull, WorkoutFocusLegs:
		return true
	}
	return false
}

func validateProgramName(name string) error {
	name = strings.TrimSpace(name)
	if name == "" || utf8.RuneCountInString(name) > MaxProgramNameLength {
		return fmt.Errorf("program name must be from 1 to %d characters: %w", MaxProgramNameLength, ErrBadRequest)
	}
	return nil
}

func validateProgramGoalLevel(goal ProgramGoal, level ProgramLevel) error {
	if !goal.IsValid() {
		return fmt.Errorf("invalid goal %q: %w", goal, ErrBadRequest)
	}
	if !level.IsValid() {
		return fmt.Errorf("invalid level %q: %w", level, ErrBadRequest)
	}
	return nil
}

func validateWeeksCount(count int) error {
	if count < 1 || count > MaxProgramWeeks {
		return fmt.Errorf("weeks_count must be 1..%d: %w", MaxProgramWeeks, ErrBadRequest)
	}
	return nil
}

// validateDaysOfWeekList checks ISO weekdays: in range and without repeats, the list may be empty
func validateDaysOfWeekList(days []int) error {
	seen := make(map[int]struct{}, len(days))
	for _, d := range days {
		if d < MinDayOfWeek || d > MaxDayOfWeek {
			return fmt.Errorf("day of week %d is out of range %d..%d: %w", d, MinDayOfWeek, MaxDayOfWeek, ErrBadRequest)
		}
		if _, ok := seen[d]; ok {
			return fmt.Errorf("day of week %d is repeated: %w", d, ErrBadRequest)
		}
		seen[d] = struct{}{}
	}
	return nil
}

// ValidateDaysOfWeek is validateDaysOfWeekList for a list which must not be empty
func ValidateDaysOfWeek(days []int) error {
	if len(days) == 0 {
		return fmt.Errorf("days_of_week must not be empty: %w", ErrBadRequest)
	}
	return validateDaysOfWeekList(days)
}

func (i *CreateUserProgramInput) Validate() error {
	if err := validateProgramName(i.Name); err != nil {
		return err
	}
	if err := validateProgramGoalLevel(i.Goal, i.Level); err != nil {
		return err
	}
	if err := validateDaysOfWeekList(i.DaysOfWeek); err != nil {
		return err
	}
	if i.WeeksCount != nil {
		return validateWeeksCount(*i.WeeksCount)
	}
	return nil
}

func (i *UpdateUserProgramInput) Validate() error {
	if i.Name != nil {
		if err := validateProgramName(*i.Name); err != nil {
			return err
		}
	}
	if i.Goal != nil && !i.Goal.IsValid() {
		return fmt.Errorf("invalid goal %q: %w", *i.Goal, ErrBadRequest)
	}
	if i.Level != nil && !i.Level.IsValid() {
		return fmt.Errorf("invalid level %q: %w", *i.Level, ErrBadRequest)
	}
	if err := validateDaysOfWeekList(i.DaysOfWeek); err != nil {
		return err
	}
	if i.WeeksCount != nil {
		return validateWeeksCount(*i.WeeksCount)
	}
	return nil
}

// Validate checks days of week and exercises of the week, days must be among allowed ones
func (w *UserProgramWeek) Validate(allowedDays []int) error {
	seen := make(map[int]struct{}, len(w.Days))
	for i, day := range w.Days {
		if !slices.Contains(allowedDays, day.DayOfWeek) {
			return fmt.Errorf("day %d: day of week %d is not in days_of_week of program: %w", i, day.DayOfWeek, ErrBadRequest)
		}
		if _, ok := seen[day.DayOfWeek]; ok {
			return fmt.Errorf("day %d: day of week %d is repeated in week: %w", i, day.DayOfWeek, ErrBadRequest)
		}
		seen[day.DayOfWeek] = struct{}{}

		for j, ex := range day.Exercises {
			if err := ex.validate(); err != nil {
				return fmt.Errorf("day %d exercise %d: %w", i, j, err)
			}
		}
	}
	return nil
}

func (e *UserProgramExercise) validate() error {
	if e.ExerciseID == uuid.Nil {
		return fmt.Errorf("exercise_id is required: %w", ErrBadRequest)
	}
	if e.Sets < MinExerciseSets || e.Sets > MaxExerciseSets {
		return fmt.Errorf("sets must be %d..%d: %w", MinExerciseSets, MaxExerciseSets, ErrBadRequest)
	}
	if e.TargetReps < 1 {
		return fmt.Errorf("target_reps must be positive: %w", ErrBadRequest)
	}
	if e.TargetWeightKg != nil && (*e.TargetWeightKg < 0 || *e.TargetWeightKg >= 10000) {
		return fmt.Errorf("target_weight_kg is out of range: %w", ErrBadRequest)
	}
	if e.TargetPercent1RM != nil && (*e.TargetPercent1RM <= 0 || *e.TargetPercent1RM > MaxTargetPercent1RM) {
		return fmt.Errorf("target_percent_1rm must be greater than 0 and not greater than %d: %w", MaxTargetPercent1RM, ErrBadRequest)
	}
	if e.TargetWeightKg != nil && e.TargetPercent1RM != nil {
		return fmt.Errorf("target_weight_kg and target_percent_1rm can not be set together: %w", ErrBadRequest)
	}
	if e.RestSeconds != nil && *e.RestSeconds < 0 {
		return fmt.Errorf("rest_seconds must not be negative: %w", ErrBadRequest)
	}
	return nil
}

// Validate checks days of week, number of weeks, days and exercises of every week
func (s *UserProgramStructure) Validate() error {
	if err := validateDaysOfWeekList(s.DaysOfWeek); err != nil {
		return err
	}
	if len(s.Weeks) > MaxProgramWeeks {
		return fmt.Errorf("program can have up to %d weeks: %w", MaxProgramWeeks, ErrBadRequest)
	}
	for i, week := range s.Weeks {
		if err := week.Validate(s.DaysOfWeek); err != nil {
			return fmt.Errorf("week %d: %w", i+1, err)
		}
	}
	return nil
}

// WeeksCount returns the number of weeks of the structure, an empty structure is one week
func (s *UserProgramStructure) WeeksCount() int {
	return max(len(s.Weeks), 1)
}

// ExerciseIDs returns unique ids of exercises used in the weeks
func ExerciseIDs(weeks ...*UserProgramWeek) []uuid.UUID {
	seen := make(Set[uuid.UUID])
	res := []uuid.UUID{}
	for _, week := range weeks {
		for _, day := range week.Days {
			for _, ex := range day.Exercises {
				if _, ok := seen[ex.ExerciseID]; !ok {
					seen[ex.ExerciseID] = struct{}{}
					res = append(res, ex.ExerciseID)
				}
			}
		}
	}
	return res
}
