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

func newUserClient(t *testing.T) *client {
	t.Helper()
	c := newClient(t)
	c.token = signUp(t, c, newEmail()).Tokens.AccessToken
	return c
}

func ptr[T any](v T) *T {
	return &v
}

// ids of basic exercises from the catalog
func basicExerciseIDs(t *testing.T, n int) []uuid.UUID {
	t.Helper()
	exercises := readCatalog(t).exercises
	require.GreaterOrEqual(t, len(exercises), n)
	res := make([]uuid.UUID, 0, n)
	for _, e := range exercises[:n] {
		res = append(res, e.ID)
	}
	return res
}

func insertTemplate(t *testing.T, name string, exerciseIDs []uuid.UUID) uuid.UUID {
	t.Helper()
	ctx := context.Background()
	var id uuid.UUID
	require.NoError(t, db.QueryRow(ctx, `
		INSERT INTO workout_templates (name, focus, level)
		VALUES ($1, 'full_body', 'beginner') RETURNING id
	`, name).Scan(&id))

	for i, exerciseID := range exerciseIDs {
		_, err := db.Exec(ctx, `
			INSERT INTO workout_template_exercises (workout_template_id, exercise_id, order_index, sets, target_reps, rest_seconds)
			VALUES ($1, $2, $3, 3, 10, 60)
		`, id, exerciseID, i)
		require.NoError(t, err)
	}
	return id
}

// weeks[i] are templates of workouts of week i + 1 in order
func insertDefaultProgram(t *testing.T, name string, weeks [][]uuid.UUID) uuid.UUID {
	t.Helper()
	ctx := context.Background()
	var id uuid.UUID
	require.NoError(t, db.QueryRow(ctx, `
		INSERT INTO default_programs (name, goal, level, days_per_week, weeks_count)
		VALUES ($1, 'muscle', 'beginner', $2, $3) RETURNING id
	`, name, len(weeks[0]), len(weeks)).Scan(&id))

	for week, templateIDs := range weeks {
		for i, templateID := range templateIDs {
			_, err := db.Exec(ctx, `
				INSERT INTO default_program_days (default_program_id, week_number, order_index, workout_template_id)
				VALUES ($1, $2, $3, $4)
			`, id, week+1, i, templateID)
			require.NoError(t, err)
		}
	}
	return id
}

func getProgram(c *client, id uuid.UUID) dto.ProgramDTO {
	c.t.Helper()
	var res dto.ProgramDTO
	c.requireOK(http.MethodGet, "/programs/"+id.String(), nil, &res)
	return res
}

// exercises of the day of the week, the day must be in the week
func dayOf(t *testing.T, p dto.ProgramDTO, week, dayOfWeek int) dto.ProgramDayDTO {
	t.Helper()
	require.LessOrEqual(t, week, len(p.Weeks))
	for _, day := range p.Weeks[week-1].Days {
		if day.DayOfWeek == dayOfWeek {
			return day
		}
	}
	t.Fatalf("no day %d in week %d", dayOfWeek, week)
	return dto.ProgramDayDTO{}
}

func exerciseInput(id uuid.UUID, sets, reps int) dto.ProgramDayExerciseInput {
	return dto.ProgramDayExerciseInput{ExerciseID: id, Sets: sets, TargetReps: reps}
}

func TestDefaultProgramsCatalog(t *testing.T) {
	t.Parallel()
	exercises := basicExerciseIDs(t, 2)
	prefix := newPrefix()

	a := insertTemplate(t, prefix+"A", exercises)
	b := insertTemplate(t, prefix+"B", exercises[:1])
	program := insertDefaultProgram(t, prefix+"program", [][]uuid.UUID{{a, b}, {b, a}})
	other := insertDefaultProgram(t, prefix+"other", [][]uuid.UUID{{a, b, a}})

	c := newClient(t)

	var list []dto.DefaultProgramSummaryDTO
	c.requireOK(http.MethodGet, "/default-programs?day_of_week=1&day_of_week=4&level=beginner", nil, &list)
	ids := []uuid.UUID{}
	for _, p := range list {
		ids = append(ids, p.ID)
		require.Equal(t, 2, p.DaysPerWeek)
		if p.ID == program {
			require.Equal(t, 2, p.WeeksCount)
		}
	}
	require.Contains(t, ids, program)
	require.NotContains(t, ids, other)

	c.requireOK(http.MethodGet, "/default-programs?goal=strength", nil, &list)
	for _, p := range list {
		require.NotContains(t, []uuid.UUID{program, other}, p.ID)
	}

	c.requireError(http.MethodGet, "/default-programs?day_of_week=1&day_of_week=1", nil, http.StatusBadRequest, "undefined_error")
	c.requireError(http.MethodGet, "/default-programs?day_of_week=8", nil, http.StatusBadRequest, "undefined_error")
	c.requireError(http.MethodGet, "/default-programs?goal=unknown", nil, http.StatusBadRequest, "undefined_error")

	var res dto.DefaultProgramDTO
	c.requireOK(http.MethodGet, "/default-programs/"+program.String(), nil, &res)
	require.Equal(t, prefix+"program", res.Name)
	require.Equal(t, 2, res.DaysPerWeek)
	require.Equal(t, 2, res.WeeksCount)
	require.Len(t, res.Days, 4)
	// ordered by week and position in the week
	require.Equal(t, []int{1, 1, 2, 2}, []int{res.Days[0].WeekNumber, res.Days[1].WeekNumber, res.Days[2].WeekNumber, res.Days[3].WeekNumber})
	require.Equal(t, []int{0, 1, 0, 1}, []int{res.Days[0].OrderIndex, res.Days[1].OrderIndex, res.Days[2].OrderIndex, res.Days[3].OrderIndex})
	require.Equal(t, a, res.Days[0].WorkoutTemplate.ID)
	require.Len(t, res.Days[0].WorkoutTemplate.Exercises, 2)
	require.NotEmpty(t, res.Days[0].WorkoutTemplate.Exercises[0].ExerciseName)
	require.Equal(t, b, res.Days[2].WorkoutTemplate.ID)

	c.requireError(http.MethodGet, "/default-programs/"+uuid.NewString(), nil, http.StatusNotFound, "undefined_error")
	c.requireError(http.MethodGet, "/default-programs/not-uuid", nil, http.StatusBadRequest, "undefined_error")
}

func TestProgramFromDefault(t *testing.T) {
	t.Parallel()
	exercises := basicExerciseIDs(t, 3)
	prefix := newPrefix()
	a := insertTemplate(t, prefix+"A", exercises)
	b := insertTemplate(t, prefix+"B", exercises[:1])
	c3 := insertTemplate(t, prefix+"C", exercises[:2])
	def := insertDefaultProgram(t, prefix+"program", [][]uuid.UUID{{a, b, a}, {c3, c3, b}})

	c := newUserClient(t)

	// workouts are laid out on days in ascending order regardless of the request order
	var program dto.ProgramDTO
	c.requireOK(http.MethodPost, "/programs/from-default", dto.CreateProgramFromDefaultRequestBody{
		DefaultProgramID: def,
		DaysOfWeek:       []int{5, 1, 3},
	}, &program)
	require.Equal(t, prefix+"program", program.Name)
	require.Equal(t, "default", program.SourceKind)
	require.Equal(t, "draft", program.State)
	require.Equal(t, "muscle", program.Goal)
	require.Equal(t, "beginner", program.Level)
	require.Equal(t, def, *program.DefaultProgramID)
	require.Equal(t, []int{1, 3, 5}, program.DaysOfWeek)
	require.Equal(t, 2, program.WeeksCount)
	require.Nil(t, program.StartDate)
	require.Len(t, program.Weeks, 2)

	counts := func(week int) []int {
		res := []int{}
		for _, day := range program.Weeks[week-1].Days {
			res = append(res, len(day.Exercises))
		}
		return res
	}
	require.Equal(t, []int{3, 1, 3}, counts(1))
	require.Equal(t, []int{2, 2, 1}, counts(2))
	require.Equal(t, 1, program.Weeks[0].WeekNumber)
	require.Equal(t, 5, program.Weeks[0].Days[2].DayOfWeek)
	require.NotEmpty(t, program.Weeks[0].Days[0].Exercises[0].ExerciseName)

	require.Equal(t, program, getProgram(c, program.ID))

	c.requireError(http.MethodPost, "/programs/from-default", dto.CreateProgramFromDefaultRequestBody{
		DefaultProgramID: def,
		DaysOfWeek:       []int{1, 3},
	}, http.StatusBadRequest, model.ProgramDaysCountMismatchKey)
	c.requireError(http.MethodPost, "/programs/from-default", dto.CreateProgramFromDefaultRequestBody{
		DefaultProgramID: uuid.New(),
		DaysOfWeek:       []int{1, 3, 5},
	}, http.StatusNotFound, "undefined_error")
	c.requireError(http.MethodPost, "/programs/from-default", dto.CreateProgramFromDefaultRequestBody{
		DefaultProgramID: def,
		DaysOfWeek:       []int{1, 3, 3},
	}, http.StatusBadRequest, "undefined_error")
}

func TestCreateProgramMetadata(t *testing.T) {
	t.Parallel()
	c := newUserClient(t)

	// the draft appears in the list at once, days are sorted, weeks are empty
	var program dto.ProgramDTO
	c.requireOK(http.MethodPost, "/programs", dto.CreateProgramRequestBody{
		Name: "first", Goal: "strength", Level: "beginner", DaysOfWeek: []int{5, 1}, WeeksCount: ptr(2),
	}, &program)
	require.Equal(t, "custom", program.SourceKind)
	require.Equal(t, "draft", program.State)
	require.Nil(t, program.DefaultProgramID)
	require.Equal(t, []int{1, 5}, program.DaysOfWeek)
	require.Equal(t, 2, program.WeeksCount)
	require.Len(t, program.Weeks, 2)
	for week := 1; week <= 2; week++ {
		require.Len(t, program.Weeks[week-1].Days, 2)
		require.Empty(t, dayOf(t, program, week, 1).Exercises)
		require.NotNil(t, dayOf(t, program, week, 5).Exercises)
	}

	var list []dto.ProgramSummaryDTO
	c.requireOK(http.MethodGet, "/programs", nil, &list)
	require.Len(t, list, 1)
	require.Equal(t, program.ID, list[0].ID)
	require.Equal(t, []int{1, 5}, list[0].DaysOfWeek)

	// days and weeks are optional
	var bare dto.ProgramDTO
	c.requireOK(http.MethodPost, "/programs", dto.CreateProgramRequestBody{Name: "bare", Goal: "muscle", Level: "advanced"}, &bare)
	require.NotNil(t, bare.DaysOfWeek)
	require.Empty(t, bare.DaysOfWeek)
	require.Equal(t, 1, bare.WeeksCount)
	require.Len(t, bare.Weeks, 1)
	require.NotNil(t, bare.Weeks[0].Days)

	// any number of programs is allowed
	c.requireOK(http.MethodPost, "/programs", dto.CreateProgramRequestBody{Name: "third", Goal: "muscle", Level: "advanced"}, nil)
	c.requireOK(http.MethodGet, "/programs", nil, &list)
	require.Len(t, list, 3)

	for _, body := range []dto.CreateProgramRequestBody{
		{Name: "x", Goal: "bad", Level: "beginner"},
		{Name: "x", Goal: "muscle", Level: "bad"},
		{Name: " ", Goal: "muscle", Level: "beginner"},
		{Name: "x", Goal: "muscle", Level: "beginner", DaysOfWeek: []int{8}},
		{Name: "x", Goal: "muscle", Level: "beginner", DaysOfWeek: []int{1, 1}},
		{Name: "x", Goal: "muscle", Level: "beginner", WeeksCount: ptr(0)},
		{Name: "x", Goal: "muscle", Level: "beginner", WeeksCount: ptr(model.MaxProgramWeeks + 1)},
	} {
		c.requireError(http.MethodPost, "/programs", body, http.StatusBadRequest, "undefined_error")
	}
}

func TestProgramStructureEditing(t *testing.T) {
	t.Parallel()
	exercises := basicExerciseIDs(t, 3)
	c := newUserClient(t)

	var program dto.ProgramDTO
	c.requireOK(http.MethodPost, "/programs", dto.CreateProgramRequestBody{
		Name: "p", Goal: "strength", Level: "beginner", DaysOfWeek: []int{1, 3}, WeeksCount: ptr(2),
	}, &program)
	path := "/programs/" + program.ID.String()

	// the structure is filled week by week
	weight := 62.5
	rest := 120
	var filled dto.ProgramDTO
	c.requireOK(http.MethodPut, path+"/weeks/1", dto.ReplaceProgramWeekRequestBody{Days: []dto.ProgramDayInput{
		{DayOfWeek: 3, Exercises: []dto.ProgramDayExerciseInput{
			{ExerciseID: exercises[0], Sets: 4, TargetReps: 8, TargetWeightKg: &weight, RestSeconds: &rest},
			exerciseInput(exercises[1], 3, 12),
		}},
		{DayOfWeek: 1, Exercises: []dto.ProgramDayExerciseInput{exerciseInput(exercises[2], 5, 5)}},
	}}, &filled)
	require.Len(t, dayOf(t, filled, 1, 3).Exercises, 2)
	first := dayOf(t, filled, 1, 3).Exercises[0]
	require.Equal(t, 62.5, *first.TargetWeightKg)
	require.Equal(t, 120, *first.RestSeconds)
	require.NotEmpty(t, first.ExerciseName)
	require.Equal(t, 0, first.OrderIndex)
	require.Equal(t, 1, dayOf(t, filled, 1, 3).Exercises[1].OrderIndex)
	require.Len(t, dayOf(t, filled, 1, 1).Exercises, 1)
	require.Empty(t, dayOf(t, filled, 2, 1).Exercises)

	// a week is replaced entirely
	filled = dto.ProgramDTO{}
	c.requireOK(http.MethodPut, path+"/weeks/1", dto.ReplaceProgramWeekRequestBody{Days: []dto.ProgramDayInput{
		{DayOfWeek: 1, Exercises: []dto.ProgramDayExerciseInput{exerciseInput(exercises[0], 2, 15)}},
	}}, &filled)
	require.Empty(t, dayOf(t, filled, 1, 3).Exercises)
	require.Len(t, dayOf(t, filled, 1, 1).Exercises, 1)
	require.Equal(t, 2, dayOf(t, filled, 1, 1).Exercises[0].Sets)
	require.Nil(t, dayOf(t, filled, 1, 1).Exercises[0].TargetWeightKg)

	c.requireOK(http.MethodPut, path+"/weeks/2", dto.ReplaceProgramWeekRequestBody{Days: []dto.ProgramDayInput{
		{DayOfWeek: 1, Exercises: []dto.ProgramDayExerciseInput{exerciseInput(exercises[1], 1, 1)}},
		{DayOfWeek: 3, Exercises: []dto.ProgramDayExerciseInput{exerciseInput(exercises[2], 1, 1)}},
	}}, nil)
	filled = getProgram(c, program.ID)
	require.Equal(t, exercises[0], dayOf(t, filled, 1, 1).Exercises[0].ExerciseID)
	require.Equal(t, exercises[1], dayOf(t, filled, 2, 1).Exercises[0].ExerciseID)
	require.Equal(t, exercises[2], dayOf(t, filled, 2, 3).Exercises[0].ExerciseID)

	// invalid weeks and days change nothing
	badWeeks := []struct {
		path string
		body dto.ReplaceProgramWeekRequestBody
	}{
		{"/weeks/3", dto.ReplaceProgramWeekRequestBody{}},
		{"/weeks/0", dto.ReplaceProgramWeekRequestBody{}},
		{"/weeks/x", dto.ReplaceProgramWeekRequestBody{}},
		{"/weeks/1", dto.ReplaceProgramWeekRequestBody{Days: []dto.ProgramDayInput{{DayOfWeek: 2}}}},
		{"/weeks/1", dto.ReplaceProgramWeekRequestBody{Days: []dto.ProgramDayInput{{DayOfWeek: 1}, {DayOfWeek: 1}}}},
		{"/weeks/1", dto.ReplaceProgramWeekRequestBody{Days: []dto.ProgramDayInput{{DayOfWeek: 1, Exercises: []dto.ProgramDayExerciseInput{exerciseInput(exercises[0], 0, 5)}}}}},
		{"/weeks/1", dto.ReplaceProgramWeekRequestBody{Days: []dto.ProgramDayInput{{DayOfWeek: 1, Exercises: []dto.ProgramDayExerciseInput{exerciseInput(exercises[0], 3, 0)}}}}},
	}
	for _, bad := range badWeeks {
		c.requireError(http.MethodPut, path+bad.path, bad.body, http.StatusBadRequest, "undefined_error")
	}
	c.requireError(http.MethodPut, path+"/weeks/1", dto.ReplaceProgramWeekRequestBody{Days: []dto.ProgramDayInput{
		{DayOfWeek: 1, Exercises: []dto.ProgramDayExerciseInput{exerciseInput(uuid.New(), 3, 5)}},
	}}, http.StatusBadRequest, model.ProgramExerciseNotAvailableKey)
	require.Equal(t, filled, getProgram(c, program.ID))

	// removed days and weeks lose exercises, the schedule is common for all weeks
	var updated dto.ProgramDTO
	c.requireOK(http.MethodPatch, path, dto.UpdateProgramRequestBody{DaysOfWeek: []int{1}}, &updated)
	require.Equal(t, []int{1}, updated.DaysOfWeek)
	require.Len(t, updated.Weeks[0].Days, 1)
	require.Len(t, updated.Weeks[1].Days, 1)
	require.Len(t, dayOf(t, updated, 2, 1).Exercises, 1)

	updated = dto.ProgramDTO{}
	c.requireOK(http.MethodPatch, path, dto.UpdateProgramRequestBody{DaysOfWeek: []int{1, 4}, WeeksCount: ptr(1)}, &updated)
	require.Equal(t, 1, updated.WeeksCount)
	require.Len(t, updated.Weeks, 1)
	require.Len(t, dayOf(t, updated, 1, 1).Exercises, 1)
	require.Empty(t, dayOf(t, updated, 1, 4).Exercises)
	var exercisesInDB int
	require.NoError(t, db.QueryRow(context.Background(),
		`SELECT COUNT(*) FROM user_program_exercises WHERE program_id = $1`, program.ID).Scan(&exercisesInDB))
	require.Equal(t, 1, exercisesInDB)

	// the same request fills a new program and edits it later, weeks_count is the number of weeks
	full := dto.ReplaceProgramStructureRequestBody{
		DaysOfWeek: []int{6, 2},
		Weeks: []dto.ProgramWeekInput{
			{Days: []dto.ProgramDayInput{
				{DayOfWeek: 2, Exercises: []dto.ProgramDayExerciseInput{exerciseInput(exercises[0], 3, 10), exerciseInput(exercises[1], 3, 10)}},
				{DayOfWeek: 6, Exercises: []dto.ProgramDayExerciseInput{exerciseInput(exercises[2], 3, 10)}},
			}},
			{Days: []dto.ProgramDayInput{{DayOfWeek: 6, Exercises: []dto.ProgramDayExerciseInput{exerciseInput(exercises[0], 2, 12)}}}},
			{},
		},
	}
	var replaced dto.ProgramDTO
	c.requireOK(http.MethodPut, path+"/structure", full, &replaced)
	require.Equal(t, []int{2, 6}, replaced.DaysOfWeek)
	require.Equal(t, 3, replaced.WeeksCount)
	require.Len(t, replaced.Weeks, 3)
	require.Len(t, dayOf(t, replaced, 1, 2).Exercises, 2)
	require.Empty(t, dayOf(t, replaced, 2, 2).Exercises)
	require.Len(t, dayOf(t, replaced, 2, 6).Exercises, 1)
	require.Empty(t, dayOf(t, replaced, 3, 6).Exercises)

	// an empty structure is one empty week without days
	c.requireOK(http.MethodPut, path+"/structure", dto.ReplaceProgramStructureRequestBody{}, &replaced)
	require.Empty(t, replaced.DaysOfWeek)
	require.Equal(t, 1, replaced.WeeksCount)

	c.requireOK(http.MethodPut, path+"/structure", full, &replaced)
	badStructures := []dto.ReplaceProgramStructureRequestBody{
		{DaysOfWeek: []int{8}},
		{DaysOfWeek: []int{1, 1}},
		{DaysOfWeek: []int{1}, Weeks: []dto.ProgramWeekInput{{Days: []dto.ProgramDayInput{{DayOfWeek: 2}}}}},
		{DaysOfWeek: []int{1}, Weeks: []dto.ProgramWeekInput{{Days: []dto.ProgramDayInput{{DayOfWeek: 1}, {DayOfWeek: 1}}}}},
		{DaysOfWeek: []int{1}, Weeks: make([]dto.ProgramWeekInput, model.MaxProgramWeeks+1)},
	}
	for _, body := range badStructures {
		c.requireError(http.MethodPut, path+"/structure", body, http.StatusBadRequest, "undefined_error")
	}
	c.requireError(http.MethodPut, path+"/structure", dto.ReplaceProgramStructureRequestBody{
		DaysOfWeek: []int{1},
		Weeks:      []dto.ProgramWeekInput{{Days: []dto.ProgramDayInput{{DayOfWeek: 1, Exercises: []dto.ProgramDayExerciseInput{exerciseInput(uuid.New(), 1, 1)}}}}},
	}, http.StatusBadRequest, model.ProgramExerciseNotAvailableKey)
	require.Equal(t, replaced, getProgram(c, program.ID))

	for _, body := range []dto.UpdateProgramRequestBody{
		{Name: ptr(" ")}, {Goal: ptr("bad")}, {Level: ptr("bad")}, {DaysOfWeek: []int{0}}, {WeeksCount: ptr(0)}, {WeeksCount: ptr(model.MaxProgramWeeks + 1)},
	} {
		c.requireError(http.MethodPatch, path, body, http.StatusBadRequest, "undefined_error")
	}
	require.Equal(t, replaced, getProgram(c, program.ID))
}

func TestProgramLifecycle(t *testing.T) {
	t.Parallel()
	exercises := basicExerciseIDs(t, 2)
	c := newUserClient(t)

	newProgram := func(name string, days []int) dto.ProgramDTO {
		t.Helper()
		var res dto.ProgramDTO
		c.requireOK(http.MethodPost, "/programs", dto.CreateProgramRequestBody{Name: name, Goal: "muscle", Level: "beginner", DaysOfWeek: days}, &res)
		return res
	}
	fill := func(id uuid.UUID, day int) {
		t.Helper()
		c.requireOK(http.MethodPut, "/programs/"+id.String()+"/structure", dto.ReplaceProgramStructureRequestBody{
			DaysOfWeek: []int{day},
			Weeks:      []dto.ProgramWeekInput{{Days: []dto.ProgramDayInput{{DayOfWeek: day, Exercises: []dto.ProgramDayExerciseInput{exerciseInput(exercises[0], 3, 10)}}}}},
		}, nil)
	}

	first := newProgram("first", nil)
	path := "/programs/" + first.ID.String()

	// an empty draft can not be started
	c.requireError(http.MethodPost, path+"/start", nil, http.StatusBadRequest, model.ProgramIncompleteKey)

	var renamed dto.ProgramDTO
	c.requireOK(http.MethodPatch, path, dto.UpdateProgramRequestBody{Name: ptr("renamed"), Level: ptr("advanced")}, &renamed)
	require.Equal(t, "renamed", renamed.Name)
	require.Equal(t, "advanced", renamed.Level)
	require.Equal(t, "muscle", renamed.Goal)

	fill(first.ID, 3)
	var started dto.StartProgramResponse
	c.requireOK(http.MethodPost, path+"/start", nil, &started)
	require.Equal(t, "in_progress", started.State)
	require.NotNil(t, started.StartDate)
	require.Nil(t, started.FinishedProgramID)
	c.requireError(http.MethodPost, path+"/start", nil, http.StatusBadRequest, model.ProgramInvalidStateTransitionKey)
	c.requireError(http.MethodDelete, path, nil, http.StatusBadRequest, model.ProgramNotDraftKey)

	// a program in progress can still be edited
	c.requireOK(http.MethodPatch, path, dto.UpdateProgramRequestBody{Name: ptr("edited")}, nil)

	// the second program finishes the first one when it starts
	second := newProgram("second", nil)
	fill(second.ID, 7)
	started = dto.StartProgramResponse{}
	c.requireOK(http.MethodPost, "/programs/"+second.ID.String()+"/start", nil, &started)
	require.Equal(t, second.ID, started.ID)
	require.Equal(t, first.ID, *started.FinishedProgramID)

	var list []dto.ProgramSummaryDTO
	c.requireOK(http.MethodGet, "/programs", nil, &list)
	require.Len(t, list, 2)
	require.Equal(t, second.ID, list[0].ID)

	c.requireOK(http.MethodGet, "/programs?state=finished", nil, &list)
	require.Len(t, list, 1)
	require.Equal(t, first.ID, list[0].ID)
	require.Equal(t, "finished", list[0].State)

	c.requireOK(http.MethodGet, "/programs?state=in_progress", nil, &list)
	require.Len(t, list, 1)
	require.Equal(t, second.ID, list[0].ID)

	c.requireError(http.MethodGet, "/programs?state=active", nil, http.StatusBadRequest, "undefined_error")

	// finished program is read only
	c.requireError(http.MethodPatch, path, dto.UpdateProgramRequestBody{Name: ptr("x")}, http.StatusBadRequest, model.ProgramReadOnlyKey)
	c.requireError(http.MethodPut, path+"/structure", dto.ReplaceProgramStructureRequestBody{}, http.StatusBadRequest, model.ProgramReadOnlyKey)
	c.requireError(http.MethodPut, path+"/weeks/1", dto.ReplaceProgramWeekRequestBody{}, http.StatusBadRequest, model.ProgramReadOnlyKey)
	c.requireError(http.MethodPost, path+"/finish", nil, http.StatusBadRequest, model.ProgramInvalidStateTransitionKey)
	c.requireError(http.MethodPost, path+"/start", nil, http.StatusBadRequest, model.ProgramInvalidStateTransitionKey)

	var finished dto.ProgramDTO
	c.requireOK(http.MethodPost, "/programs/"+second.ID.String()+"/finish", nil, &finished)
	require.Equal(t, "finished", finished.State)

	// only a draft can be deleted
	draft := newProgram("draft", nil)
	c.requireError(http.MethodPost, "/programs/"+draft.ID.String()+"/finish", nil, http.StatusBadRequest, model.ProgramInvalidStateTransitionKey)
	c.requireOK(http.MethodDelete, "/programs/"+draft.ID.String(), nil, nil)
	c.requireError(http.MethodGet, "/programs/"+draft.ID.String(), nil, http.StatusNotFound, "undefined_error")
}

func TestStartIncompleteProgram(t *testing.T) {
	t.Parallel()
	exercises := basicExerciseIDs(t, 1)
	c := newUserClient(t)

	var program dto.ProgramDTO
	c.requireOK(http.MethodPost, "/programs", dto.CreateProgramRequestBody{
		Name: "p", Goal: "muscle", Level: "beginner", DaysOfWeek: []int{1, 2}, WeeksCount: ptr(2),
	}, &program)
	path := "/programs/" + program.ID.String()
	day := func(dayOfWeek int) dto.ProgramDayInput {
		return dto.ProgramDayInput{DayOfWeek: dayOfWeek, Exercises: []dto.ProgramDayExerciseInput{exerciseInput(exercises[0], 3, 10)}}
	}

	// the second day of the second week is empty
	c.requireOK(http.MethodPut, path+"/weeks/1", dto.ReplaceProgramWeekRequestBody{Days: []dto.ProgramDayInput{day(1), day(2)}}, nil)
	c.requireOK(http.MethodPut, path+"/weeks/2", dto.ReplaceProgramWeekRequestBody{Days: []dto.ProgramDayInput{day(1)}}, nil)
	c.requireError(http.MethodPost, path+"/start", nil, http.StatusBadRequest, model.ProgramIncompleteKey)

	// a program without weeks_count increase has only filled days
	c.requireOK(http.MethodPut, path+"/weeks/2", dto.ReplaceProgramWeekRequestBody{Days: []dto.ProgramDayInput{day(1), day(2)}}, nil)
	c.requireOK(http.MethodPost, path+"/start", nil, nil)
}

func TestProgramsOfOtherUser(t *testing.T) {
	t.Parallel()
	exercises := basicExerciseIDs(t, 1)
	owner := newUserClient(t)
	other := newUserClient(t)

	var program dto.ProgramDTO
	owner.requireOK(http.MethodPost, "/programs", dto.CreateProgramRequestBody{Name: "p", Goal: "muscle", Level: "beginner", DaysOfWeek: []int{1}}, &program)
	path := "/programs/" + program.ID.String()

	day := dto.ProgramDayInput{DayOfWeek: 1, Exercises: []dto.ProgramDayExerciseInput{exerciseInput(exercises[0], 1, 1)}}
	other.requireError(http.MethodGet, path, nil, http.StatusForbidden, "undefined_error")
	other.requireError(http.MethodPatch, path, dto.UpdateProgramRequestBody{Name: ptr("x")}, http.StatusForbidden, "undefined_error")
	other.requireError(http.MethodPut, path+"/structure", dto.ReplaceProgramStructureRequestBody{
		DaysOfWeek: []int{1}, Weeks: []dto.ProgramWeekInput{{Days: []dto.ProgramDayInput{day}}},
	}, http.StatusForbidden, "undefined_error")
	other.requireError(http.MethodPut, path+"/weeks/1", dto.ReplaceProgramWeekRequestBody{Days: []dto.ProgramDayInput{day}}, http.StatusForbidden, "undefined_error")
	other.requireError(http.MethodPost, path+"/start", nil, http.StatusForbidden, "undefined_error")
	other.requireError(http.MethodPost, path+"/finish", nil, http.StatusForbidden, "undefined_error")
	other.requireError(http.MethodDelete, path, nil, http.StatusForbidden, "undefined_error")

	var list []dto.ProgramSummaryDTO
	other.requireOK(http.MethodGet, "/programs", nil, &list)
	require.Empty(t, list)
	require.NotNil(t, list)

	owner.requireOK(http.MethodGet, path, nil, nil)
}

func TestProgramsRequireAuth(t *testing.T) {
	t.Parallel()
	c := newClient(t)

	id := uuid.NewString()
	for _, req := range []struct{ method, path string }{
		{http.MethodGet, "/programs"},
		{http.MethodPost, "/programs"},
		{http.MethodPost, "/programs/from-default"},
		{http.MethodGet, "/programs/" + id},
		{http.MethodPatch, "/programs/" + id},
		{http.MethodPut, "/programs/" + id + "/structure"},
		{http.MethodPut, "/programs/" + id + "/weeks/1"},
		{http.MethodPost, "/programs/" + id + "/start"},
		{http.MethodPost, "/programs/" + id + "/finish"},
		{http.MethodDelete, "/programs/" + id},
	} {
		c.requireError(req.method, req.path, nil, http.StatusUnauthorized, model.AuthErrorInvalidTokenKey)
	}
}
