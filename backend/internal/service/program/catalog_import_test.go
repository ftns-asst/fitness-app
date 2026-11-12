package program

import (
	"ftns-asst/internal/model"
	"testing"

	"github.com/google/uuid"
	"github.com/stretchr/testify/require"
	excelize "github.com/xuri/excelize/v2"
)

// relative to the package directory, where go test runs
const (
	programsTablePath  = "../../../../data/default_programs.xlsx"
	exercisesTablePath = "../../../../data/exercise_catalog.xlsx"
)

func TestDefaultProgramsTable(t *testing.T) {
	templates, programs, err := loadDefaultPrograms(programsTablePath)
	require.NoError(t, err)
	require.NotEmpty(t, templates)

	// each level has more than three programs
	byLevel := map[model.ProgramLevel]int{}
	ids := model.NewSet[uuid.UUID]()
	for _, program := range programs {
		byLevel[program.Level]++
		require.NotContains(t, ids, program.ID, program.Name)
		ids[program.ID] = struct{}{}

		require.NotNil(t, program.Description, program.Name)
		require.GreaterOrEqual(t, program.DaysPerWeek, 1, program.Name)
		require.LessOrEqual(t, program.DaysPerWeek, model.MaxDayOfWeek, program.Name)
		require.Len(t, program.Days, program.DaysPerWeek*program.WeeksCount, program.Name)
	}
	for _, level := range []model.ProgramLevel{model.ProgramLevelBeginner, model.ProgramLevelIntermediate, model.ProgramLevelAdvanced} {
		require.Greater(t, byLevel[level], 3, "programs of level %s", level)
	}

	// a template has the level of the programs which use it
	templatesByID := map[uuid.UUID]*model.WorkoutTemplate{}
	for _, template := range templates {
		require.NotContains(t, ids, template.ID, template.Name)
		ids[template.ID] = struct{}{}
		templatesByID[template.ID] = template
		require.NotEmpty(t, template.Exercises, template.Name)
	}
	for _, program := range programs {
		for _, day := range program.Days {
			require.Equal(t, program.Level, templatesByID[day.WorkoutTemplateID].Level, program.Name)
		}
	}
}

// programs refer to exercises of the exercise table by uuid, the names in the programs table are for readers
func TestDefaultProgramsUseExercisesFromCatalog(t *testing.T) {
	catalog, err := excelize.OpenFile(exercisesTablePath)
	require.NoError(t, err)
	defer func() { _ = catalog.Close() }()

	rows, err := catalog.GetRows("exercises")
	require.NoError(t, err)
	namesByID := map[uuid.UUID]string{}
	for _, row := range rows[1:] {
		namesByID[uuid.MustParse(row[0])] = row[1]
	}

	templates, _, err := loadDefaultPrograms(programsTablePath)
	require.NoError(t, err)

	for _, template := range templates {
		for _, exercise := range template.Exercises {
			name, ok := namesByID[exercise.ExerciseID]
			require.True(t, ok, "template %q: exercise %s is not in the catalog", template.Name, exercise.ExerciseID)
			require.Equal(t, name, exercise.ExerciseName, "template %q", template.Name)
		}
	}
}

// parts of a valid table, tests change one of them
type table struct {
	programs          [][]any
	programDays       [][]any
	templates         [][]any
	templateExercises [][]any
}

var exerciseUUID = "3b69c049-d674-4ec0-9e4f-a279d6f4d332"

func validTable() *table {
	return &table{
		programs: [][]any{
			{"key", "name", "description", "goal", "level"},
			{"p1", "Program", "Description", "strength", "beginner"},
		},
		programDays: [][]any{
			{"program", "week", "position", "template"},
			{"p1", 1, 1, "t1"},
			{"p1", 1, 2, "t2"},
			{"p1", 2, 1, "t2"},
			{"p1", 2, 2, "t1"},
		},
		templates: [][]any{
			{"key", "name", "focus", "level"},
			{"t1", "Template 1", "full_body", "beginner"},
			{"t2", "Template 2", "upper", "beginner"},
		},
		templateExercises: [][]any{
			{"template", "position", "exercise", "exercise_name", "sets", "reps", "percent_1rm", "rest_seconds"},
			{"t1", 1, exerciseUUID, "Bench", 3, 5, 80.5, 120},
			{"t1", 2, exerciseUUID, "Bench", 1, 8, nil, nil},
			{"t2", 1, exerciseUUID, "Bench", 5, 5, nil, nil},
		},
	}
}

func (tb *table) file(t *testing.T) *excelize.File {
	t.Helper()
	file := excelize.NewFile()
	require.NoError(t, file.SetSheetName("Sheet1", programsSheet))
	for sheet, rows := range map[string][][]any{
		programsSheet:          tb.programs,
		programDaysSheet:       tb.programDays,
		templatesSheet:         tb.templates,
		templateExercisesSheet: tb.templateExercises,
	} {
		if sheet != programsSheet {
			_, err := file.NewSheet(sheet)
			require.NoError(t, err)
		}
		for i, row := range rows {
			cell, err := excelize.CoordinatesToCellName(1, i+1)
			require.NoError(t, err)
			require.NoError(t, file.SetSheetRow(sheet, cell, &row))
		}
	}
	return file
}

func TestParseDefaultProgramsValidTable(t *testing.T) {
	templates, programs, err := parseDefaultPrograms(validTable().file(t))
	require.NoError(t, err)
	require.Len(t, templates, 2)
	require.Len(t, programs, 1)

	program := programs[0]
	require.Equal(t, programID("p1"), program.ID)
	require.Equal(t, "Description", *program.Description)
	require.Equal(t, model.ProgramGoalStrength, program.Goal)
	require.Equal(t, 2, program.DaysPerWeek)
	require.Equal(t, 2, program.WeeksCount)
	require.Len(t, program.Days, 4)
	require.Equal(t, []int{1, 1, 2, 2}, []int{program.Days[0].WeekNumber, program.Days[1].WeekNumber, program.Days[2].WeekNumber, program.Days[3].WeekNumber})
	require.Equal(t, []int{0, 1, 0, 1}, []int{program.Days[0].OrderIndex, program.Days[1].OrderIndex, program.Days[2].OrderIndex, program.Days[3].OrderIndex})
	require.Equal(t, templateID("t1"), program.Days[0].WorkoutTemplateID)

	first := templates[0]
	require.Equal(t, templateID("t1"), first.ID)
	require.Equal(t, model.WorkoutFocusFullBody, first.Focus)
	require.Len(t, first.Exercises, 2)
	require.Equal(t, 80.5, *first.Exercises[0].TargetPercent1RM)
	require.Equal(t, 120, *first.Exercises[0].RestSeconds)
	require.Equal(t, 0, first.Exercises[0].OrderIndex)
	require.Equal(t, 1, first.Exercises[1].OrderIndex)
	require.Nil(t, first.Exercises[1].TargetPercent1RM)
	require.Nil(t, first.Exercises[1].RestSeconds)
}

func TestParseDefaultProgramsInvalidTable(t *testing.T) {
	tests := []struct {
		name   string
		change func(tb *table)
	}{
		{"unknown template in program days", func(tb *table) { tb.programDays[1][3] = "unknown" }},
		{"unknown program in program days", func(tb *table) { tb.programDays[1][0] = "unknown" }},
		{"unknown template of exercise", func(tb *table) { tb.templateExercises[1][0] = "unknown" }},
		{"invalid focus", func(tb *table) { tb.templates[1][2] = "arms" }},
		{"invalid template level", func(tb *table) { tb.templates[1][3] = "expert" }},
		{"invalid goal", func(tb *table) { tb.programs[1][3] = "health" }},
		{"invalid program level", func(tb *table) { tb.programs[1][4] = "pro" }},
		{"duplicate template key", func(tb *table) { tb.templates[2][0] = "t1" }},
		{"duplicate program key", func(tb *table) { tb.programs = append(tb.programs, tb.programs[1]) }},
		{"empty program name", func(tb *table) { tb.programs[1][1] = "" }},
		{"template is not used", func(tb *table) {
			tb.templates = append(tb.templates, []any{"t3", "Template 3", "upper", "beginner"})
			tb.templateExercises = append(tb.templateExercises, []any{"t3", 1, exerciseUUID, "Bench", 3, 5, nil, nil})
		}},
		{"template without exercises", func(tb *table) {
			tb.templateExercises = tb.templateExercises[:3]
		}},
		{"gap in weeks", func(tb *table) { tb.programDays[3][1] = 3; tb.programDays[4][1] = 3 }},
		{"different days per week", func(tb *table) { tb.programDays = tb.programDays[:4] }},
		{"gap in positions of days", func(tb *table) { tb.programDays[2][2] = 3 }},
		{"repeated position of a day", func(tb *table) { tb.programDays[2][2] = 1 }},
		{"too many days in a week", func(tb *table) { tb.programDays[1][2] = 8 }},
		{"program without days", func(tb *table) {
			tb.programDays = tb.programDays[:1]
			tb.templates = tb.templates[:1]
			tb.templateExercises = tb.templateExercises[:1]
		}},
		{"gap in positions of exercises", func(tb *table) { tb.templateExercises[2][1] = 3 }},
		{"repeated position of an exercise", func(tb *table) { tb.templateExercises[2][1] = 1 }},
		{"invalid exercise uuid", func(tb *table) { tb.templateExercises[1][2] = "not-uuid" }},
		{"zero sets", func(tb *table) { tb.templateExercises[1][4] = 0 }},
		{"too many sets", func(tb *table) { tb.templateExercises[1][4] = model.MaxExerciseSets + 1 }},
		{"zero reps", func(tb *table) { tb.templateExercises[1][5] = 0 }},
		{"zero percent", func(tb *table) { tb.templateExercises[1][6] = 0 }},
		{"percent over limit", func(tb *table) { tb.templateExercises[1][6] = 100.5 }},
		{"negative rest", func(tb *table) { tb.templateExercises[1][7] = -1 }},
		{"wrong header", func(tb *table) { tb.templates[0][2] = "kind" }},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			tb := validTable()
			tt.change(tb)
			_, _, err := parseDefaultPrograms(tb.file(t))
			require.Error(t, err)
		})
	}
}

func TestParseDefaultProgramsMissingSheet(t *testing.T) {
	file := validTable().file(t)
	require.NoError(t, file.DeleteSheet(templatesSheet))

	_, _, err := parseDefaultPrograms(file)
	require.Error(t, err)
}

func TestLoadDefaultProgramsMissingFile(t *testing.T) {
	_, _, err := loadDefaultPrograms("not-existing.xlsx")
	require.Error(t, err)
}

func TestCatalogIDsAreStable(t *testing.T) {
	require.Equal(t, templateID("same"), templateID("same"))
	require.NotEqual(t, templateID("same"), templateID("other"))
	// a template and a program with the same key are different entities
	require.NotEqual(t, templateID("same"), programID("same"))
}
