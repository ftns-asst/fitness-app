package program

import (
	"fmt"
	"ftns-asst/internal/model"
	"strconv"
	"strings"

	"github.com/google/uuid"
	excelize "github.com/xuri/excelize/v2"
)

// ids of templates and programs are made from their keys, so the import can be repeated
var catalogNamespace = uuid.MustParse("8b0f5b8e-61c4-4d5c-9a3e-2f6a41d7c9b0")

const (
	programsSheet          = "programs"
	programDaysSheet       = "program_days"
	templatesSheet         = "templates"
	templateExercisesSheet = "template_exercises"
)

var (
	programsColumns          = []string{"key", "name", "description", "goal", "level"}
	programDaysColumns       = []string{"program", "week", "position", "template"}
	templatesColumns         = []string{"key", "name", "focus", "level"}
	templateExercisesColumns = []string{"template", "position", "exercise", "exercise_name", "sets", "reps", "percent_1rm", "rest_seconds"}
)

func templateID(key string) uuid.UUID {
	return uuid.NewSHA1(catalogNamespace, []byte("workout_template:"+key))
}

func programID(key string) uuid.UUID {
	return uuid.NewSHA1(catalogNamespace, []byte("default_program:"+key))
}

// loadDefaultPrograms reads workout templates and default programs from the table
func loadDefaultPrograms(path string) ([]*model.WorkoutTemplate, []*model.DefaultProgram, error) {
	file, err := excelize.OpenFile(path)
	if err != nil {
		return nil, nil, fmt.Errorf("failed to open file: %w", err)
	}
	defer func() {
		_ = file.Close()
	}()

	return parseDefaultPrograms(file)
}

// parseDefaultPrograms checks the table and builds templates and programs, equipment of templates is not set:
// it is taken from exercises when the data is saved
func parseDefaultPrograms(file *excelize.File) ([]*model.WorkoutTemplate, []*model.DefaultProgram, error) {
	templates, templateByKey, err := parseTemplates(file)
	if err != nil {
		return nil, nil, err
	}

	programs, err := parsePrograms(file, templateByKey)
	if err != nil {
		return nil, nil, err
	}

	used := model.NewSet[uuid.UUID]()
	for _, program := range programs {
		for _, day := range program.Days {
			used[day.WorkoutTemplateID] = struct{}{}
		}
	}
	for _, template := range templates {
		if _, ok := used[template.ID]; !ok {
			return nil, nil, fmt.Errorf("template %q is not used by any program", template.Name)
		}
	}

	return templates, programs, nil
}

// readSheet returns rows without the header, short rows are filled with empty cells
func readSheet(file *excelize.File, sheet string, columns []string) ([][]string, error) {
	rows, err := file.GetRows(sheet)
	if err != nil {
		return nil, fmt.Errorf("failed to get sheet %s: %w", sheet, err)
	}
	if len(rows) == 0 {
		return nil, fmt.Errorf("sheet %s is empty", sheet)
	}

	header := rows[0]
	for i, column := range columns {
		if i >= len(header) || strings.TrimSpace(header[i]) != column {
			return nil, fmt.Errorf("incorrect columns of sheet %s, must be: %s", sheet, strings.Join(columns, ", "))
		}
	}

	res := make([][]string, 0, len(rows)-1)
	for _, row := range rows[1:] {
		cells := make([]string, len(columns))
		empty := true
		for i := range columns {
			if i < len(row) {
				cells[i] = strings.TrimSpace(row[i])
			}
			if cells[i] != "" {
				empty = false
			}
		}
		if !empty {
			res = append(res, cells)
		}
	}
	return res, nil
}

func parseTemplates(file *excelize.File) ([]*model.WorkoutTemplate, map[string]*model.WorkoutTemplate, error) {
	rows, err := readSheet(file, templatesSheet, templatesColumns)
	if err != nil {
		return nil, nil, err
	}

	templates := make([]*model.WorkoutTemplate, 0, len(rows))
	byKey := make(map[string]*model.WorkoutTemplate, len(rows))
	for i, row := range rows {
		line := i + 2
		key, name := row[0], row[1]
		if key == "" || name == "" {
			return nil, nil, fmt.Errorf("sheet %s, row %d: key and name are required", templatesSheet, line)
		}
		if _, ok := byKey[key]; ok {
			return nil, nil, fmt.Errorf("sheet %s, row %d: duplicate key %q", templatesSheet, line, key)
		}

		focus := model.WorkoutFocus(row[2])
		if !focus.IsValid() {
			return nil, nil, fmt.Errorf("sheet %s, row %d: invalid focus %q", templatesSheet, line, row[2])
		}
		level := model.ProgramLevel(row[3])
		if !level.IsValid() {
			return nil, nil, fmt.Errorf("sheet %s, row %d: invalid level %q", templatesSheet, line, row[3])
		}

		template := &model.WorkoutTemplate{ID: templateID(key), Name: name, Focus: focus, Level: level}
		templates = append(templates, template)
		byKey[key] = template
	}

	if err = parseTemplateExercises(file, byKey); err != nil {
		return nil, nil, err
	}
	return templates, byKey, nil
}

func parseTemplateExercises(file *excelize.File, templates map[string]*model.WorkoutTemplate) error {
	rows, err := readSheet(file, templateExercisesSheet, templateExercisesColumns)
	if err != nil {
		return err
	}

	// positions of exercises of a template, they must be 1..n without gaps
	positions := make(map[string]map[int]*model.WorkoutTemplateExercise)
	for i, row := range rows {
		line := i + 2
		template, ok := templates[row[0]]
		if !ok {
			return fmt.Errorf("sheet %s, row %d: unknown template %q", templateExercisesSheet, line, row[0])
		}

		position, err := parseInt(row[1])
		if err != nil || position < 1 {
			return fmt.Errorf("sheet %s, row %d: position must be a positive number", templateExercisesSheet, line)
		}
		if positions[row[0]] == nil {
			positions[row[0]] = make(map[int]*model.WorkoutTemplateExercise)
		}
		if _, ok = positions[row[0]][position]; ok {
			return fmt.Errorf("sheet %s, row %d: position %d is repeated in template %q", templateExercisesSheet, line, position, row[0])
		}

		exerciseID, err := uuid.Parse(row[2])
		if err != nil {
			return fmt.Errorf("sheet %s, row %d: invalid exercise uuid: %w", templateExercisesSheet, line, err)
		}

		sets, err := parseInt(row[4])
		if err != nil || sets < model.MinExerciseSets || sets > model.MaxExerciseSets {
			return fmt.Errorf("sheet %s, row %d: sets must be %d..%d", templateExercisesSheet, line, model.MinExerciseSets, model.MaxExerciseSets)
		}
		reps, err := parseInt(row[5])
		if err != nil || reps < 1 {
			return fmt.Errorf("sheet %s, row %d: reps must be positive", templateExercisesSheet, line)
		}

		exercise := &model.WorkoutTemplateExercise{
			WorkoutTemplateID: template.ID,
			ExerciseID:        exerciseID,
			ExerciseName:      row[3],
			Sets:              sets,
			TargetReps:        reps,
		}

		if row[6] != "" {
			percent, err := parseFloat(row[6])
			if err != nil || percent <= 0 || percent > model.MaxTargetPercent1RM {
				return fmt.Errorf("sheet %s, row %d: percent_1rm must be greater than 0 and not greater than %d", templateExercisesSheet, line, model.MaxTargetPercent1RM)
			}
			exercise.TargetPercent1RM = &percent
		}
		if row[7] != "" {
			rest, err := parseInt(row[7])
			if err != nil || rest < 0 {
				return fmt.Errorf("sheet %s, row %d: rest_seconds must not be negative", templateExercisesSheet, line)
			}
			exercise.RestSeconds = &rest
		}

		positions[row[0]][position] = exercise
	}

	for key, template := range templates {
		byPosition := positions[key]
		if len(byPosition) == 0 {
			return fmt.Errorf("template %q has no exercises", key)
		}

		for position := 1; position <= len(byPosition); position++ {
			exercise, ok := byPosition[position]
			if !ok {
				return fmt.Errorf("template %q: positions of exercises must be 1..%d without gaps", key, len(byPosition))
			}
			exercise.OrderIndex = position - 1
			template.Exercises = append(template.Exercises, exercise)
		}
	}
	return nil
}

func parsePrograms(file *excelize.File, templates map[string]*model.WorkoutTemplate) ([]*model.DefaultProgram, error) {
	rows, err := readSheet(file, programsSheet, programsColumns)
	if err != nil {
		return nil, err
	}

	programs := make([]*model.DefaultProgram, 0, len(rows))
	byKey := make(map[string]*model.DefaultProgram, len(rows))
	for i, row := range rows {
		line := i + 2
		key, name := row[0], row[1]
		if key == "" || name == "" {
			return nil, fmt.Errorf("sheet %s, row %d: key and name are required", programsSheet, line)
		}
		if _, ok := byKey[key]; ok {
			return nil, fmt.Errorf("sheet %s, row %d: duplicate key %q", programsSheet, line, key)
		}

		goal := model.ProgramGoal(row[3])
		if !goal.IsValid() {
			return nil, fmt.Errorf("sheet %s, row %d: invalid goal %q", programsSheet, line, row[3])
		}
		level := model.ProgramLevel(row[4])
		if !level.IsValid() {
			return nil, fmt.Errorf("sheet %s, row %d: invalid level %q", programsSheet, line, row[4])
		}

		program := &model.DefaultProgram{ID: programID(key), Name: name, Goal: goal, Level: level}
		if row[2] != "" {
			program.Description = &row[2]
		}
		programs = append(programs, program)
		byKey[key] = program
	}

	if err = parseProgramDays(file, byKey, templates); err != nil {
		return nil, err
	}
	return programs, nil
}

func parseProgramDays(file *excelize.File, programs map[string]*model.DefaultProgram, templates map[string]*model.WorkoutTemplate) error {
	rows, err := readSheet(file, programDaysSheet, programDaysColumns)
	if err != nil {
		return err
	}

	// days[program][week][position]
	days := make(map[string]map[int]map[int]*model.DefaultProgramDay)
	for i, row := range rows {
		line := i + 2
		program, ok := programs[row[0]]
		if !ok {
			return fmt.Errorf("sheet %s, row %d: unknown program %q", programDaysSheet, line, row[0])
		}
		template, ok := templates[row[3]]
		if !ok {
			return fmt.Errorf("sheet %s, row %d: unknown template %q", programDaysSheet, line, row[3])
		}

		week, err := parseInt(row[1])
		if err != nil || week < 1 || week > model.MaxProgramWeeks {
			return fmt.Errorf("sheet %s, row %d: week must be 1..%d", programDaysSheet, line, model.MaxProgramWeeks)
		}
		position, err := parseInt(row[2])
		if err != nil || position < 1 || position > model.MaxDayOfWeek {
			return fmt.Errorf("sheet %s, row %d: position must be 1..%d", programDaysSheet, line, model.MaxDayOfWeek)
		}

		if days[row[0]] == nil {
			days[row[0]] = make(map[int]map[int]*model.DefaultProgramDay)
		}
		if days[row[0]][week] == nil {
			days[row[0]][week] = make(map[int]*model.DefaultProgramDay)
		}
		if _, ok = days[row[0]][week][position]; ok {
			return fmt.Errorf("sheet %s, row %d: position %d is repeated in week %d of program %q", programDaysSheet, line, position, week, row[0])
		}
		days[row[0]][week][position] = &model.DefaultProgramDay{
			DefaultProgramID:  program.ID,
			WeekNumber:        week,
			OrderIndex:        position - 1,
			WorkoutTemplateID: template.ID,
		}
	}

	for key, program := range programs {
		weeks := days[key]
		if len(weeks) == 0 {
			return fmt.Errorf("program %q has no days", key)
		}
		program.WeeksCount = len(weeks)

		for week := 1; week <= program.WeeksCount; week++ {
			byPosition, ok := weeks[week]
			if !ok {
				return fmt.Errorf("program %q: weeks must be 1..%d without gaps", key, program.WeeksCount)
			}

			if week == 1 {
				program.DaysPerWeek = len(byPosition)
			}
			if len(byPosition) != program.DaysPerWeek {
				return fmt.Errorf("program %q: week %d has %d days, week 1 has %d", key, week, len(byPosition), program.DaysPerWeek)
			}

			for position := 1; position <= len(byPosition); position++ {
				day, ok := byPosition[position]
				if !ok {
					return fmt.Errorf("program %q: positions of days in week %d must be 1..%d without gaps", key, week, len(byPosition))
				}
				program.Days = append(program.Days, day)
			}
		}
	}
	return nil
}

func parseInt(s string) (int, error) {
	return strconv.Atoi(strings.TrimSpace(s))
}

func parseFloat(s string) (float64, error) {
	return strconv.ParseFloat(strings.ReplaceAll(strings.TrimSpace(s), ",", "."), 64)
}
