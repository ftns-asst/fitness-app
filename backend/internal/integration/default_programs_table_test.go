//go:build integration

package integration

import (
	"context"
	"ftns-asst/internal/rest/dto"
	"net/http"
	"strings"
	"testing"

	"github.com/google/uuid"
	"github.com/stretchr/testify/require"
)

// names of default programs from the table
const (
	strengthBasic3Days5x5 = "Базовая сила, 3 дня: тренировки A и B по 5×5"
	strengthBasic3Days3x5 = "Базовая сила, 3 дня: тренировки A и B по 3×5"
	strengthBasic4Days    = "Базовая сила, 4 дня: основные подъёмы 5×3, второстепенные 3×10"
	percents3Days         = "Сила по процентам, 3 дня: цикл из 3 недель"
	percents4Days         = "Сила по процентам, 4 дня: цикл из 4 недель"
)

// relative to the package directory, where go test runs
var defaultProgramsPath = getEnv("TEST_DEFAULT_PROGRAMS_PATH", "../../../data/default_programs.xlsx")

func countRows(t *testing.T, table string) int {
	t.Helper()
	var count int
	require.NoError(t, db.QueryRow(context.Background(), "SELECT COUNT(*) FROM "+table).Scan(&count))
	return count
}

func defaultProgramByName(t *testing.T, c *client, name string) dto.DefaultProgramSummaryDTO {
	t.Helper()
	var list []dto.DefaultProgramSummaryDTO
	c.requireOK(http.MethodGet, "/default-programs", nil, &list)
	for _, program := range list {
		if program.Name == name {
			return program
		}
	}
	t.Fatalf("default program %q is not found", name)
	return dto.DefaultProgramSummaryDTO{}
}

func TestDefaultProgramsFromTable(t *testing.T) {
	t.Parallel()
	c := newClient(t)

	var list []dto.DefaultProgramSummaryDTO
	c.requireOK(http.MethodGet, "/default-programs", nil, &list)

	// each level has more than three programs from the table
	levels := map[string]int{}
	for _, program := range list {
		levels[program.Level]++
	}
	for _, level := range []string{"beginner", "intermediate", "advanced"} {
		require.Greater(t, levels[level], 3, level)
	}

	var beginners []dto.DefaultProgramSummaryDTO
	c.requireOK(http.MethodGet, "/default-programs?level=beginner&day_of_week=1&day_of_week=3&day_of_week=5", nil, &beginners)
	names := []string{}
	for _, program := range beginners {
		require.Equal(t, 3, program.DaysPerWeek)
		names = append(names, program.Name)
	}
	require.Contains(t, names, strengthBasic3Days5x5)
	require.Contains(t, names, strengthBasic3Days3x5)
	require.Contains(t, names, percents3Days)
	require.NotContains(t, names, strengthBasic4Days) // four days a week

	// two weeks of three workouts, A and B alternate, equipment is taken from the exercises
	sl := defaultProgramByName(t, c, strengthBasic3Days5x5)
	require.Equal(t, "strength", sl.Goal)
	require.Equal(t, 2, sl.WeeksCount)
	require.NotNil(t, sl.Description)
	require.Contains(t, *sl.Description, "https://stronglifts.com/")

	var details dto.DefaultProgramDTO
	c.requireOK(http.MethodGet, "/default-programs/"+sl.ID.String(), nil, &details)
	require.Len(t, details.Days, 6)
	workoutNames := []string{}
	for _, day := range details.Days {
		workoutNames = append(workoutNames, day.WorkoutTemplate.Name)
	}
	workoutA := "Тренировка A: приседания, жим лёжа, тяга штанги в наклоне"
	workoutB := "Тренировка B: приседания, армейский жим, становая тяга"
	require.Equal(t, []string{workoutA, workoutB, workoutA, workoutB, workoutA, workoutB}, workoutNames)

	firstWorkout := details.Days[0].WorkoutTemplate
	require.Equal(t, "full_body", firstWorkout.Focus)
	require.ElementsMatch(t, []string{"barbell", "squat_rack", "bench"}, firstWorkout.Equipment)
	require.Len(t, firstWorkout.Exercises, 3)
	require.Equal(t, "Приседания со штангой на спине", firstWorkout.Exercises[0].ExerciseName)
	require.Equal(t, 5, firstWorkout.Exercises[0].Sets)
	require.Equal(t, 5, firstWorkout.Exercises[0].TargetReps)
	require.Equal(t, 180, *firstWorkout.Exercises[0].RestSeconds)
	require.Nil(t, firstWorkout.Exercises[0].TargetPercent1RM)

	// the loads of 5/3/1 are percents of the personal record
	wendler := defaultProgramByName(t, c, percents4Days)
	require.Equal(t, 4, wendler.DaysPerWeek)
	require.Equal(t, 4, wendler.WeeksCount)
	c.requireOK(http.MethodGet, "/default-programs/"+wendler.ID.String(), nil, &details)
	require.Len(t, details.Days, 16)
	firstSet := details.Days[0].WorkoutTemplate.Exercises[0]
	require.Equal(t, 58.5, *firstSet.TargetPercent1RM)
	require.Equal(t, 5, firstSet.TargetReps)
}

// the table can be loaded again: records are updated, not duplicated
func TestDefaultProgramsLoadIsRepeatable(t *testing.T) {
	ctx := context.Background()

	programs := countRows(t, "default_programs")
	templates := countRows(t, "workout_templates")
	templateExercises := countRows(t, "workout_template_exercises")
	days := countRows(t, "default_program_days")

	var before []dto.DefaultProgramSummaryDTO
	newClient(t).requireOK(http.MethodGet, "/default-programs", nil, &before)

	require.NoError(t, services.Program().Start(ctx))

	require.Equal(t, programs, countRows(t, "default_programs"))
	require.Equal(t, templates, countRows(t, "workout_templates"))
	require.Equal(t, templateExercises, countRows(t, "workout_template_exercises"))
	require.Equal(t, days, countRows(t, "default_program_days"))

	var after []dto.DefaultProgramSummaryDTO
	newClient(t).requireOK(http.MethodGet, "/default-programs", nil, &after)
	require.ElementsMatch(t, before, after)
}

func TestProgramFromTableNeedsMeasurement(t *testing.T) {
	t.Parallel()
	c, userID := newUserWithID(t)

	// without percents the program is a draft at once
	sl := defaultProgramByName(t, c, strengthBasic3Days5x5)
	var program dto.ProgramDTO
	c.requireOK(http.MethodPost, "/programs/from-default", dto.CreateProgramFromDefaultRequestBody{
		DefaultProgramID: sl.ID, DaysOfWeek: []int{1, 3, 5},
	}, &program)
	require.Equal(t, "draft", program.State)
	require.Equal(t, []int{1, 3, 5}, program.DaysOfWeek)
	require.Equal(t, 2, program.WeeksCount)
	require.Len(t, dayOf(t, program, 1, 1).Exercises, 3)
	require.Len(t, dayOf(t, program, 1, 3).Exercises, 3)
	require.Equal(t, 180, *dayOf(t, program, 1, 1).Exercises[0].RestSeconds)

	// the percent loads need records of the four main lifts
	wendler := defaultProgramByName(t, c, percents3Days)
	program = dto.ProgramDTO{}
	c.requireOK(http.MethodPost, "/programs/from-default", dto.CreateProgramFromDefaultRequestBody{
		DefaultProgramID: wendler.ID, DaysOfWeek: []int{2, 4, 6},
	}, &program)
	require.Equal(t, "needs_measurement", program.State)
	require.Equal(t, 3, program.WeeksCount)
	names := []string{}
	for _, m := range program.RequiredMeasurements {
		names = append(names, m.ExerciseName)
	}
	require.ElementsMatch(t, []string{
		"Приседания со штангой на спине", "Жим штанги лёжа", "Становая тяга", "Армейский жим штанги стоя",
	}, names)

	records := []dto.CreatePersonalRecordRequestBody{}
	for _, m := range program.RequiredMeasurements {
		records = append(records, dto.CreatePersonalRecordRequestBody{ExerciseID: m.ExerciseID, WeightKg: 100})
	}
	c.requireOK(http.MethodPost, "/users/"+userID.String()+"/prs/bulk", dto.CreatePersonalRecordsRequestBody{Records: records}, nil)

	program = getProgram(c, program.ID)
	require.Equal(t, "draft", program.State)
	require.Empty(t, program.RequiredMeasurements)
	c.requireOK(http.MethodPost, "/programs/"+program.ID.String()+"/start", nil, nil)
}

func TestEveryDefaultProgramCanBeUsed(t *testing.T) {
	t.Parallel()
	c := newUserClient(t)

	var list []dto.DefaultProgramSummaryDTO
	c.requireOK(http.MethodGet, "/default-programs", nil, &list)

	// other tests add their own programs, the programs from the table have a source in the description
	checked := 0
	for _, summary := range list {
		if summary.Description == nil || !strings.Contains(*summary.Description, "Источник: ") {
			continue
		}
		checked++

		var details dto.DefaultProgramDTO
		c.requireOK(http.MethodGet, "/default-programs/"+summary.ID.String(), nil, &details)
		require.Len(t, details.Days, summary.DaysPerWeek*summary.WeeksCount, summary.Name)
		for _, day := range details.Days {
			require.NotEmpty(t, day.WorkoutTemplate.Exercises, summary.Name)
			require.NotEmpty(t, day.WorkoutTemplate.Equipment, summary.Name)
		}

		// the first selected days of week are enough for any program
		days := []int{1, 2, 3, 4, 5, 6, 7}[:summary.DaysPerWeek]
		var program dto.ProgramDTO
		c.requireOK(http.MethodPost, "/programs/from-default", dto.CreateProgramFromDefaultRequestBody{
			DefaultProgramID: summary.ID, DaysOfWeek: days,
		}, &program)
		require.Equal(t, summary.WeeksCount, program.WeeksCount, summary.Name)
		require.Equal(t, days, program.DaysOfWeek, summary.Name)
		for _, week := range program.Weeks {
			require.Len(t, week.Days, summary.DaysPerWeek, summary.Name)
			for _, day := range week.Days {
				require.NotEmpty(t, day.Exercises, summary.Name)
			}
		}
		require.Contains(t, []string{"draft", "needs_measurement"}, program.State, summary.Name)
		require.NotEqual(t, uuid.Nil, program.ID)
	}
	require.GreaterOrEqual(t, checked, 12)
}
