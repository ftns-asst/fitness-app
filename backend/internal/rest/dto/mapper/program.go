package mapper

import (
	"ftns-asst/internal/model"
	"ftns-asst/internal/rest/dto"
	"ftns-asst/internal/util"

	"github.com/google/uuid"
)

const dateLayout = "2006-01-02"

// like util.Map, but nil slice becomes an empty one to be json array
func mapList[T any, K any](slice []T, f func(T) K) []K {
	res := make([]K, 0, len(slice))
	for _, item := range slice {
		res = append(res, f(item))
	}
	return res
}

func ConvertWorkoutTemplateToDTO(m *model.WorkoutTemplate) *dto.WorkoutTemplateDTO {
	if m == nil {
		return nil
	}
	return &dto.WorkoutTemplateDTO{
		ID:        m.ID,
		Name:      m.Name,
		Focus:     string(m.Focus),
		Equipment: mapList(m.Equipments, func(k model.EquipmentKey) string { return string(k) }),
		Exercises: mapList(m.Exercises, func(e *model.WorkoutTemplateExercise) dto.WorkoutTemplateExerciseDTO {
			return dto.WorkoutTemplateExerciseDTO{
				ExerciseID:       e.ExerciseID,
				ExerciseName:     e.ExerciseName,
				OrderIndex:       e.OrderIndex,
				Sets:             e.Sets,
				TargetReps:       e.TargetReps,
				TargetPercent1RM: e.TargetPercent1RM,
				RestSeconds:      e.RestSeconds,
			}
		}),
	}
}

func ConvertDefaultProgramToSummaryDTO(m *model.DefaultProgram) *dto.DefaultProgramSummaryDTO {
	if m == nil {
		return nil
	}
	return &dto.DefaultProgramSummaryDTO{
		ID:          m.ID,
		Name:        m.Name,
		Description: m.Description,
		Goal:        string(m.Goal),
		Level:       string(m.Level),
		DaysPerWeek: m.DaysPerWeek,
		WeeksCount:  m.WeeksCount,
	}
}

func ConvertDefaultProgramToDTO(m *model.DefaultProgram) *dto.DefaultProgramDTO {
	if m == nil {
		return nil
	}
	return &dto.DefaultProgramDTO{
		DefaultProgramSummaryDTO: util.UnPtr(ConvertDefaultProgramToSummaryDTO(m)),
		Days: mapList(m.Days, func(d *model.DefaultProgramDay) dto.DefaultProgramDayDTO {
			return dto.DefaultProgramDayDTO{
				WeekNumber:      d.WeekNumber,
				OrderIndex:      d.OrderIndex,
				WorkoutTemplate: ConvertWorkoutTemplateToDTO(d.WorkoutTemplate),
			}
		}),
	}
}

func ConvertProgramToSummaryDTO(m *model.UserProgram) *dto.ProgramSummaryDTO {
	if m == nil {
		return nil
	}

	var startDate *string
	if m.StartDate != nil {
		startDate = util.Ptr(m.StartDate.Format(dateLayout))
	}

	return &dto.ProgramSummaryDTO{
		ID:               m.ID,
		Name:             m.Name,
		SourceKind:       string(m.SourceKind),
		DefaultProgramID: m.DefaultProgramID,
		Goal:             string(m.Goal),
		Level:            string(m.Level),
		State:            string(m.State),
		DaysOfWeek:       mapList(m.DaysOfWeek, func(d int) int { return d }),
		WeeksCount:       m.WeeksCount,
		StartDate:        startDate,
		CreatedAt:        m.CreatedAt,
		UpdatedAt:        m.UpdatedAt,
	}
}

// ConvertProgramToDTO groups exercises by weeks and days, every week has all days of the program
func ConvertProgramToDTO(m *model.UserProgram) *dto.ProgramDTO {
	if m == nil {
		return nil
	}

	type cell struct{ week, day int }
	exercises := make(map[cell][]dto.ProgramDayExerciseDTO, len(m.Exercises))
	for _, e := range m.Exercises {
		key := cell{e.WeekNumber, e.DayOfWeek}
		exercises[key] = append(exercises[key], dto.ProgramDayExerciseDTO{
			ID:               e.ID,
			ExerciseID:       e.ExerciseID,
			ExerciseName:     e.ExerciseName,
			OrderIndex:       e.OrderIndex,
			Sets:             e.Sets,
			TargetReps:       e.TargetReps,
			TargetWeightKg:   e.TargetWeightKg,
			TargetPercent1RM: e.TargetPercent1RM,
			RestSeconds:      e.RestSeconds,
		})
	}

	weeks := make([]dto.ProgramWeekDTO, 0, m.WeeksCount)
	for week := 1; week <= m.WeeksCount; week++ {
		days := mapList(m.DaysOfWeek, func(day int) dto.ProgramDayDTO {
			list := exercises[cell{week, day}]
			if list == nil {
				list = []dto.ProgramDayExerciseDTO{}
			}
			return dto.ProgramDayDTO{DayOfWeek: day, Exercises: list}
		})
		weeks = append(weeks, dto.ProgramWeekDTO{WeekNumber: week, Days: days})
	}

	return &dto.ProgramDTO{
		ProgramSummaryDTO: util.UnPtr(ConvertProgramToSummaryDTO(m)),
		Weeks:             weeks,
		RequiredMeasurements: mapList(m.RequiredMeasurements, func(e *model.MeasurementExercise) dto.MeasurementExerciseDTO {
			return dto.MeasurementExerciseDTO{ExerciseID: e.ExerciseID, ExerciseName: e.ExerciseName}
		}),
	}
}

func ConvertStartProgramResultToDTO(m *model.StartProgramResult) *dto.StartProgramResponse {
	if m == nil {
		return nil
	}
	return &dto.StartProgramResponse{
		ProgramDTO:        util.UnPtr(ConvertProgramToDTO(m.Program)),
		FinishedProgramID: m.FinishedProgramID,
	}
}

func ConvertCreateProgramInputFromDTO(d *dto.CreateProgramRequestBody) *model.CreateUserProgramInput {
	if d == nil {
		return nil
	}
	return &model.CreateUserProgramInput{
		Name:       d.Name,
		Goal:       model.ProgramGoal(d.Goal),
		Level:      model.ProgramLevel(d.Level),
		DaysOfWeek: d.DaysOfWeek,
		WeeksCount: d.WeeksCount,
	}
}

func ConvertUpdateProgramInputFromDTO(id uuid.UUID, d *dto.UpdateProgramRequestBody) *model.UpdateUserProgramInput {
	if d == nil {
		return nil
	}

	input := &model.UpdateUserProgramInput{
		ID:         id,
		Name:       d.Name,
		DaysOfWeek: d.DaysOfWeek,
		WeeksCount: d.WeeksCount,
	}
	if d.Goal != nil {
		input.Goal = util.Ptr(model.ProgramGoal(*d.Goal))
	}
	if d.Level != nil {
		input.Level = util.Ptr(model.ProgramLevel(*d.Level))
	}
	return input
}

func convertProgramDaysFromDTO(days []dto.ProgramDayInput) *model.UserProgramWeek {
	return &model.UserProgramWeek{
		Days: mapList(days, func(day dto.ProgramDayInput) *model.UserProgramDay {
			return &model.UserProgramDay{
				DayOfWeek: day.DayOfWeek,
				Exercises: mapList(day.Exercises, func(e dto.ProgramDayExerciseInput) *model.UserProgramExercise {
					return &model.UserProgramExercise{
						ExerciseID:       e.ExerciseID,
						Sets:             e.Sets,
						TargetReps:       e.TargetReps,
						TargetWeightKg:   e.TargetWeightKg,
						TargetPercent1RM: e.TargetPercent1RM,
						RestSeconds:      e.RestSeconds,
					}
				}),
			}
		}),
	}
}

func ConvertProgramWeekFromDTO(d *dto.ReplaceProgramWeekRequestBody) *model.UserProgramWeek {
	if d == nil {
		return nil
	}
	return convertProgramDaysFromDTO(d.Days)
}

func ConvertProgramStructureFromDTO(d *dto.ReplaceProgramStructureRequestBody) *model.UserProgramStructure {
	if d == nil {
		return nil
	}
	return &model.UserProgramStructure{
		DaysOfWeek: d.DaysOfWeek,
		Weeks: mapList(d.Weeks, func(w dto.ProgramWeekInput) *model.UserProgramWeek {
			return convertProgramDaysFromDTO(w.Days)
		}),
	}
}
