//go:build integration

package integration

import (
	"context"
	"ftns-asst/internal/model"
	"ftns-asst/internal/rest/dto"
	"net/http"
	"testing"

	"github.com/google/uuid"
	"github.com/stretchr/testify/require"
)

func setTemplatePercent(t *testing.T, templateID uuid.UUID, percent float64) {
	t.Helper()
	_, err := db.Exec(context.Background(), `
		UPDATE workout_template_exercises SET target_percent_1rm = $2 WHERE workout_template_id = $1
	`, templateID, percent)
	require.NoError(t, err)
}

func measurementIDs(p dto.ProgramDTO) []uuid.UUID {
	res := []uuid.UUID{}
	for _, m := range p.RequiredMeasurements {
		res = append(res, m.ExerciseID)
	}
	return res
}

func percentExercise(id uuid.UUID, percent float64) dto.ProgramDayExerciseInput {
	return dto.ProgramDayExerciseInput{ExerciseID: id, Sets: 3, TargetReps: 5, TargetPercent1RM: ptr(percent)}
}

func listPrograms(c *client, state string) []dto.ProgramSummaryDTO {
	c.t.Helper()
	var list []dto.ProgramSummaryDTO
	c.requireOK(http.MethodGet, "/programs?state="+state, nil, &list)
	return list
}

func TestDefaultProgramNeedsMeasurement(t *testing.T) {
	t.Parallel()
	exercises := basicExerciseIDs(t, 3)
	prefix := newPrefix()
	// the first workout has percent load in two exercises, the second one has no percent
	a := insertTemplate(t, prefix+"A", exercises[:2])
	setTemplatePercent(t, a, 80)
	b := insertTemplate(t, prefix+"B", exercises[2:])
	def := insertDefaultProgram(t, prefix+"program", [][]uuid.UUID{{a, b}})

	var template dto.DefaultProgramDTO
	c, userID := newUserWithID(t)
	c.requireOK(http.MethodGet, "/default-programs/"+def.String(), nil, &template)
	require.Equal(t, 80.0, *template.Days[0].WorkoutTemplate.Exercises[0].TargetPercent1RM)
	require.Nil(t, template.Days[1].WorkoutTemplate.Exercises[0].TargetPercent1RM)

	// records are missing, so the program waits for measurement
	var program dto.ProgramDTO
	c.requireOK(http.MethodPost, "/programs/from-default", dto.CreateProgramFromDefaultRequestBody{
		DefaultProgramID: def, DaysOfWeek: []int{1, 4},
	}, &program)
	require.Equal(t, "needs_measurement", program.State)
	require.ElementsMatch(t, exercises[:2], measurementIDs(program))
	require.NotEmpty(t, program.RequiredMeasurements[0].ExerciseName)
	require.Equal(t, 80.0, *dayOf(t, program, 1, 1).Exercises[0].TargetPercent1RM)
	require.Nil(t, dayOf(t, program, 1, 1).Exercises[0].TargetWeightKg)

	require.Len(t, listPrograms(c, "needs_measurement"), 1)
	require.Empty(t, listPrograms(c, "draft"))
	path := "/programs/" + program.ID.String()
	c.requireError(http.MethodPost, path+"/start", nil, http.StatusBadRequest, model.ProgramMeasurementRequiredKey)

	// one record is not enough, the other one is estimated by a formula
	prsPath := "/users/" + userID.String() + "/prs"
	var record dto.PersonalRecordDTO
	c.requireOK(http.MethodPost, prsPath, dto.CreatePersonalRecordRequestBody{ExerciseID: exercises[0], WeightKg: 100}, &record)
	require.Equal(t, 100.0, record.WeightKg)
	require.False(t, record.IsCalculated)

	program = dto.ProgramDTO{}
	c.requireOK(http.MethodGet, path, nil, &program)
	require.Equal(t, "needs_measurement", program.State)
	require.Equal(t, []uuid.UUID{exercises[1]}, measurementIDs(program))

	record = dto.PersonalRecordDTO{}
	c.requireOK(http.MethodPost, prsPath, dto.CreatePersonalRecordRequestBody{ExerciseID: exercises[1], WeightKg: 60, Reps: ptr(10)}, &record)
	require.Equal(t, 80.0, record.WeightKg)
	require.True(t, record.IsCalculated)

	// all records are added, the program is draft and can start
	program = dto.ProgramDTO{}
	c.requireOK(http.MethodGet, path, nil, &program)
	require.Equal(t, "draft", program.State)
	require.Empty(t, program.RequiredMeasurements)
	require.NotNil(t, program.RequiredMeasurements)
	require.Len(t, listPrograms(c, "draft"), 1)
	require.Empty(t, listPrograms(c, "needs_measurement"))

	var started dto.StartProgramResponse
	c.requireOK(http.MethodPost, path+"/start", nil, &started)
	require.Equal(t, "in_progress", started.State)
	require.Empty(t, started.RequiredMeasurements)

	// a user with records gets a draft at once
	other, otherID := newUserWithID(t)
	otherPrs := "/users/" + otherID.String() + "/prs"
	other.requireOK(http.MethodPost, otherPrs, dto.CreatePersonalRecordRequestBody{ExerciseID: exercises[0], WeightKg: 50}, nil)
	other.requireOK(http.MethodPost, otherPrs, dto.CreatePersonalRecordRequestBody{ExerciseID: exercises[1], WeightKg: 40}, nil)
	var ready dto.ProgramDTO
	other.requireOK(http.MethodPost, "/programs/from-default", dto.CreateProgramFromDefaultRequestBody{
		DefaultProgramID: def, DaysOfWeek: []int{2, 5},
	}, &ready)
	require.Equal(t, "draft", ready.State)
	require.Empty(t, ready.RequiredMeasurements)
}

func TestCustomProgramMeasurementState(t *testing.T) {
	t.Parallel()
	exercises := basicExerciseIDs(t, 2)
	c, userID := newUserWithID(t)

	var program dto.ProgramDTO
	c.requireOK(http.MethodPost, "/programs", dto.CreateProgramRequestBody{Name: "p", Goal: "strength", Level: "beginner", DaysOfWeek: []int{1}}, &program)
	require.Equal(t, "draft", program.State)
	path := "/programs/" + program.ID.String()

	week := func(exercise dto.ProgramDayExerciseInput) dto.ReplaceProgramWeekRequestBody {
		return dto.ReplaceProgramWeekRequestBody{Days: []dto.ProgramDayInput{{DayOfWeek: 1, Exercises: []dto.ProgramDayExerciseInput{exercise}}}}
	}

	// percent load without a record, weight and percent can not be set together
	var changed dto.ProgramDTO
	c.requireOK(http.MethodPut, path+"/weeks/1", week(percentExercise(exercises[0], 75.5)), &changed)
	require.Equal(t, "needs_measurement", changed.State)
	require.Equal(t, []uuid.UUID{exercises[0]}, measurementIDs(changed))
	require.Equal(t, 75.5, *dayOf(t, changed, 1, 1).Exercises[0].TargetPercent1RM)

	badExercises := []dto.ProgramDayExerciseInput{
		{ExerciseID: exercises[0], Sets: 3, TargetReps: 5, TargetWeightKg: ptr(50.0), TargetPercent1RM: ptr(80.0)},
		percentExercise(exercises[0], 0),
		percentExercise(exercises[0], 100.5),
	}
	for _, bad := range badExercises {
		c.requireError(http.MethodPut, path+"/weeks/1", week(bad), http.StatusBadRequest, "undefined_error")
	}

	// a plain weight does not need a record: the state goes back
	changed = dto.ProgramDTO{}
	c.requireOK(http.MethodPut, path+"/weeks/1", week(dto.ProgramDayExerciseInput{ExerciseID: exercises[0], Sets: 3, TargetReps: 5, TargetWeightKg: ptr(50.0)}), &changed)
	require.Equal(t, "draft", changed.State)
	require.Empty(t, changed.RequiredMeasurements)

	// a record which is added before the percent load removes the need at once
	c.requireOK(http.MethodPost, "/users/"+userID.String()+"/prs", dto.CreatePersonalRecordRequestBody{ExerciseID: exercises[1], WeightKg: 90}, nil)
	changed = dto.ProgramDTO{}
	c.requireOK(http.MethodPut, path+"/structure", dto.ReplaceProgramStructureRequestBody{
		DaysOfWeek: []int{1},
		Weeks:      []dto.ProgramWeekInput{{Days: []dto.ProgramDayInput{{DayOfWeek: 1, Exercises: []dto.ProgramDayExerciseInput{percentExercise(exercises[1], 85)}}}}},
	}, &changed)
	require.Equal(t, "draft", changed.State)

	changed = dto.ProgramDTO{}
	c.requireOK(http.MethodPut, path+"/structure", dto.ReplaceProgramStructureRequestBody{
		DaysOfWeek: []int{1},
		Weeks:      []dto.ProgramWeekInput{{Days: []dto.ProgramDayInput{{DayOfWeek: 1, Exercises: []dto.ProgramDayExerciseInput{percentExercise(exercises[1], 85), percentExercise(exercises[0], 70)}}}}},
	}, &changed)
	require.Equal(t, "needs_measurement", changed.State)
	require.Equal(t, []uuid.UUID{exercises[0]}, measurementIDs(changed))

	// removing the day with the exercise removes the need too
	changed = dto.ProgramDTO{}
	c.requireOK(http.MethodPatch, path, dto.UpdateProgramRequestBody{DaysOfWeek: []int{}}, &changed)
	require.Equal(t, "draft", changed.State)

	// a program which waits for records can be edited and deleted
	c.requireOK(http.MethodPut, path+"/weeks/1", dto.ReplaceProgramWeekRequestBody{}, nil)
	c.requireOK(http.MethodPatch, path, dto.UpdateProgramRequestBody{DaysOfWeek: []int{1}}, nil)
	c.requireOK(http.MethodPut, path+"/weeks/1", week(percentExercise(exercises[0], 70)), nil)
	require.Len(t, listPrograms(c, "needs_measurement"), 1)
	c.requireOK(http.MethodDelete, path, nil, nil)
	require.Empty(t, listPrograms(c, "needs_measurement"))
}

func TestCreatePersonalRecord(t *testing.T) {
	t.Parallel()
	exercises := basicExerciseIDs(t, 1)
	exercise := exercises[0]
	c, userID := newUserWithID(t)
	prsPath := "/users/" + userID.String() + "/prs"

	create := func(body dto.CreatePersonalRecordRequestBody) dto.PersonalRecordDTO {
		t.Helper()
		var res dto.PersonalRecordDTO
		c.requireOK(http.MethodPost, prsPath, body, &res)
		return res
	}

	record := create(dto.CreatePersonalRecordRequestBody{ExerciseID: exercise, WeightKg: 102.5})
	require.NotEqual(t, uuid.Nil, record.ID)
	require.Equal(t, 102.5, record.WeightKg)
	require.False(t, record.IsCalculated)
	require.NotEmpty(t, record.ExerciseName)

	// one repetition is the record itself
	record = create(dto.CreatePersonalRecordRequestBody{ExerciseID: exercise, WeightKg: 105, Reps: ptr(1)})
	require.Equal(t, 105.0, record.WeightKg)
	require.False(t, record.IsCalculated)

	// a working set is estimated by a formula
	record = create(dto.CreatePersonalRecordRequestBody{ExerciseID: exercise, WeightKg: 100, Reps: ptr(5)})
	require.Equal(t, 116.67, record.WeightKg)
	require.True(t, record.IsCalculated)

	var history []dto.PersonalRecordDTO
	c.requireOK(http.MethodGet, prsPath+"/"+exercise.String(), nil, &history)
	require.Len(t, history, 3)
	require.Equal(t, 116.67, history[0].WeightKg)

	invalid := []dto.CreatePersonalRecordRequestBody{
		{ExerciseID: exercise, WeightKg: 0},
		{ExerciseID: exercise, WeightKg: -5},
		{ExerciseID: exercise, WeightKg: 10000},
		{ExerciseID: exercise, WeightKg: 100, Reps: ptr(0)},
		{WeightKg: 100},
	}
	for _, body := range invalid {
		c.requireError(http.MethodPost, prsPath, body, http.StatusBadRequest, "undefined_error")
	}

	// the exercise must exist and be available: not found and a private exercise of another user are not
	c.requireError(http.MethodPost, prsPath, dto.CreatePersonalRecordRequestBody{ExerciseID: uuid.New(), WeightKg: 50}, http.StatusBadRequest, model.ProgramExerciseNotAvailableKey)

	other, otherID := newUserWithID(t)
	private := createExercise(other, dto.CreateExerciseRequestBody{Name: newPrefix() + "private"})
	c.requireError(http.MethodPost, prsPath, dto.CreatePersonalRecordRequestBody{ExerciseID: private.ID, WeightKg: 50}, http.StatusBadRequest, model.ProgramExerciseNotAvailableKey)
	other.requireOK(http.MethodPost, "/users/"+otherID.String()+"/prs", dto.CreatePersonalRecordRequestBody{ExerciseID: private.ID, WeightKg: 50}, nil)

	// records are added only to the own account
	other.requireError(http.MethodPost, prsPath, dto.CreatePersonalRecordRequestBody{ExerciseID: exercise, WeightKg: 50}, http.StatusForbidden, "undefined_error")
	newClient(t).requireError(http.MethodPost, prsPath, dto.CreatePersonalRecordRequestBody{ExerciseID: exercise, WeightKg: 50}, http.StatusUnauthorized, model.AuthErrorInvalidTokenKey)
}
