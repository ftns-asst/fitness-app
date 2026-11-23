package mapper

import (
	"ftns-asst/internal/model"
	"ftns-asst/internal/rest/dto"
)

func ConvertSaveWorkoutInputFromDTO(body *dto.SaveWorkoutRequestBody) *model.SaveWorkoutInput {
	input := &model.SaveWorkoutInput{
		ProgramID: body.ProgramID, WeekNumber: body.WeekNumber, DayOfWeek: body.DayOfWeek,
		StartedAt: body.StartedAt, CompletedAt: body.CompletedAt,
		Exercises: make([]model.WorkoutExerciseInput, len(body.Exercises)),
	}
	for i, ex := range body.Exercises {
		sets := make([]model.WorkoutSetInput, len(ex.Sets))
		for j, set := range ex.Sets {
			sets[j] = model.WorkoutSetInput(set)
		}
		input.Exercises[i] = model.WorkoutExerciseInput{
			WorkoutExerciseTargets: model.WorkoutExerciseTargets(ex.WorkoutExerciseTargets),
			Sets:                   sets,
		}
	}
	return input
}

func ConvertWorkoutToDTO(workout *model.Workout) dto.WorkoutDTO {
	result := dto.WorkoutDTO{
		WorkoutSummaryDTO: dto.WorkoutSummaryDTO(workout.WorkoutSummary),
		Exercises:         make([]dto.WorkoutExerciseDTO, len(workout.Exercises)),
	}
	for i, ex := range workout.Exercises {
		sets := make([]dto.WorkoutSetDTO, len(ex.Sets))
		for j, set := range ex.Sets {
			sets[j] = dto.WorkoutSetDTO{
				ID: set.ID, SetNumber: set.SetNumber, WorkoutSetInput: dto.WorkoutSetInput(set.WorkoutSetInput),
			}
		}
		result.Exercises[i] = dto.WorkoutExerciseDTO{
			ID: ex.ID, ExerciseName: ex.ExerciseName,
			WorkoutExerciseTargets: dto.WorkoutExerciseTargets(ex.WorkoutExerciseTargets),
			Sets:                   sets,
		}
	}
	return result
}
