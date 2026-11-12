package dto

import (
	"time"

	"github.com/google/uuid"
)

type (
	WorkoutTemplateExerciseDTO struct {
		ExerciseID   uuid.UUID `json:"exercise_id"`
		ExerciseName string    `json:"exercise_name"`
		OrderIndex   int       `json:"order_index"`
		Sets         int       `json:"sets"`
		TargetReps   int       `json:"target_reps"`
		// load as percent of the personal record
		TargetPercent1RM *float64 `json:"target_percent_1rm,omitempty"`
		RestSeconds      *int     `json:"rest_seconds,omitempty"`
	} // @Name WorkoutTemplateExercise

	WorkoutTemplateDTO struct {
		ID        uuid.UUID                    `json:"id"`
		Name      string                       `json:"name"`
		Focus     string                       `json:"focus"`
		Equipment []string                     `json:"equipment"`
		Exercises []WorkoutTemplateExerciseDTO `json:"exercises"`
	} // @Name WorkoutTemplate

	// DefaultProgramDayDTO is a workout of a week, order_index is its position in the week
	DefaultProgramDayDTO struct {
		WeekNumber      int                 `json:"week_number"`
		OrderIndex      int                 `json:"order_index"`
		WorkoutTemplate *WorkoutTemplateDTO `json:"workout_template"`
	} // @Name DefaultProgramDay

	// DefaultProgramSummaryDTO is a default program without days
	DefaultProgramSummaryDTO struct {
		ID          uuid.UUID `json:"id"`
		Name        string    `json:"name"`
		Description *string   `json:"description,omitempty"`
		Goal        string    `json:"goal"`
		Level       string    `json:"level"`
		DaysPerWeek int       `json:"days_per_week"`
		WeeksCount  int       `json:"weeks_count"`
	} // @Name DefaultProgramSummary

	DefaultProgramDTO struct {
		DefaultProgramSummaryDTO
		Days []DefaultProgramDayDTO `json:"days"`
	} // @Name DefaultProgram

	ProgramDayExerciseDTO struct {
		ID             uuid.UUID `json:"id"`
		ExerciseID     uuid.UUID `json:"exercise_id"`
		ExerciseName   string    `json:"exercise_name"`
		OrderIndex     int       `json:"order_index"`
		Sets           int       `json:"sets"`
		TargetReps     int       `json:"target_reps"`
		TargetWeightKg *float64  `json:"target_weight_kg,omitempty"`
		// load as percent of the personal record, either weight or percent is set
		TargetPercent1RM *float64 `json:"target_percent_1rm,omitempty"`
		RestSeconds      *int     `json:"rest_seconds,omitempty"`
	} // @Name ProgramDayExercise

	ProgramDayDTO struct {
		DayOfWeek int                     `json:"day_of_week"`
		Exercises []ProgramDayExerciseDTO `json:"exercises"`
	} // @Name ProgramDay

	ProgramWeekDTO struct {
		WeekNumber int             `json:"week_number"`
		Days       []ProgramDayDTO `json:"days"`
	} // @Name ProgramWeek

	// ProgramSummaryDTO is a program without weeks
	ProgramSummaryDTO struct {
		ID               uuid.UUID  `json:"id"`
		Name             string     `json:"name"`
		SourceKind       string     `json:"source_kind"`
		DefaultProgramID *uuid.UUID `json:"default_program_id,omitempty"`
		Goal             string     `json:"goal"`
		Level            string     `json:"level"`
		State            string     `json:"state"`
		DaysOfWeek       []int      `json:"days_of_week"`
		WeeksCount       int        `json:"weeks_count"`
		StartDate        *string    `json:"start_date,omitempty" format:"date"`
		CreatedAt        time.Time  `json:"created_at"`
		UpdatedAt        time.Time  `json:"updated_at"`
	} // @Name ProgramSummary

	// MeasurementExerciseDTO is an exercise which personal record must be added before the program starts
	MeasurementExerciseDTO struct {
		ExerciseID   uuid.UUID `json:"exercise_id"`
		ExerciseName string    `json:"exercise_name"`
	} // @Name MeasurementExercise

	ProgramDTO struct {
		ProgramSummaryDTO
		Weeks []ProgramWeekDTO `json:"weeks"`
		// exercises with percent load and without a personal record, empty if the program can start or was started
		RequiredMeasurements []MeasurementExerciseDTO `json:"required_measurements"`
	} // @Name Program

	StartProgramResponse struct {
		ProgramDTO
		FinishedProgramID *uuid.UUID `json:"finished_program_id,omitempty"`
	} // @Name StartProgramResponse

	ListDefaultProgramsQueryParams struct {
		DaysOfWeek []int   `form:"day_of_week" binding:"omitempty,dive,min=1,max=7"`
		Goal       *string `form:"goal"`
		Level      *string `form:"level"`
	} // @Name ListDefaultProgramsQueryParams

	ListProgramsQueryParams struct {
		State *string `form:"state"`
	} // @Name ListProgramsQueryParams

	CreateProgramFromDefaultRequestBody struct {
		DefaultProgramID uuid.UUID `json:"default_program_id" binding:"required"`
		DaysOfWeek       []int     `json:"days_of_week" binding:"required"`
	} // @Name CreateProgramFromDefaultRequestBody

	CreateProgramRequestBody struct {
		Name       string `json:"name" binding:"required"`
		Goal       string `json:"goal" binding:"required"`
		Level      string `json:"level" binding:"required"`
		DaysOfWeek []int  `json:"days_of_week,omitempty"`
		WeeksCount *int   `json:"weeks_count,omitempty"`
	} // @Name CreateProgramRequestBody

	UpdateProgramRequestBody struct {
		Name       *string `json:"name,omitempty"`
		Goal       *string `json:"goal,omitempty"`
		Level      *string `json:"level,omitempty"`
		DaysOfWeek []int   `json:"days_of_week"` // an empty list removes all days, null or no field changes nothing
		WeeksCount *int    `json:"weeks_count,omitempty"`
	} // @Name UpdateProgramRequestBody

	ProgramDayExerciseInput struct {
		ExerciseID     uuid.UUID `json:"exercise_id" binding:"required"`
		Sets           int       `json:"sets"`
		TargetReps     int       `json:"target_reps"`
		TargetWeightKg *float64  `json:"target_weight_kg,omitempty"`
		// load as percent of the personal record, either weight or percent is set
		TargetPercent1RM *float64 `json:"target_percent_1rm,omitempty"`
		RestSeconds      *int     `json:"rest_seconds,omitempty"`
	} // @Name ProgramDayExerciseInput

	ProgramDayInput struct {
		DayOfWeek int                       `json:"day_of_week"`
		Exercises []ProgramDayExerciseInput `json:"exercises"`
	} // @Name ProgramDayInput

	ProgramWeekInput struct {
		Days []ProgramDayInput `json:"days"`
	} // @Name ProgramWeekInput

	ReplaceProgramWeekRequestBody struct {
		Days []ProgramDayInput `json:"days"`
	} // @Name ReplaceProgramWeekRequestBody

	ReplaceProgramStructureRequestBody struct {
		DaysOfWeek []int              `json:"days_of_week"`
		Weeks      []ProgramWeekInput `json:"weeks"` // position is the week number, weeks_count becomes their number
	} // @Name ReplaceProgramStructureRequestBody
)
