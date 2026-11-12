package program

import (
	"context"
	"errors"
	"fmt"
	"ftns-asst/internal/config"
	"ftns-asst/internal/model"
	"ftns-asst/internal/repository"
	"log"
	"slices"

	"github.com/google/uuid"
)

type ProgramService struct {
	cfg  *config.Config
	repo ProgramRepo
	tx   model.Transactor
}

func NewService(cfg *config.Config, repo ProgramRepo, transactor model.Transactor) *ProgramService {
	return &ProgramService{
		cfg:  cfg,
		repo: repo,
		tx:   transactor,
	}
}

// Start fills default programs from the table, it must run after the exercises are filled
func (s *ProgramService) Start(ctx context.Context) error {
	templates, programs, err := loadDefaultPrograms(s.cfg.DefaultProgramsPath)
	if err != nil {
		return fmt.Errorf("failed to load default programs: %w", err)
	}

	if err = s.repo.FillDefaultPrograms(ctx, templates, programs); err != nil {
		return fmt.Errorf("failed to fill default programs: %w", err)
	}

	log.Printf("Loaded %d workout templates and %d default programs", len(templates), len(programs))
	return nil
}

// run fn in a transaction, repositories take it from ctx
func (s *ProgramService) inTx(ctx context.Context, fn func(ctx context.Context) error) error {
	tx, err := s.tx.Begin(ctx)
	if err != nil {
		return fmt.Errorf("failed begin transaction: %w", err)
	}
	defer model.CheckRollback(ctx, tx)
	ctx = context.WithValue(ctx, model.ContextKeyTx, tx)

	if err = fn(ctx); err != nil {
		return err
	}

	if err = tx.Commit(ctx); err != nil {
		return fmt.Errorf("failed commit transaction: %w", err)
	}
	return nil
}

// GetDefaultPrograms returns catalog; days of week, if set, keep only programs with the same number of days
func (s *ProgramService) GetDefaultPrograms(ctx context.Context, daysOfWeek []int, goal *model.ProgramGoal, level *model.ProgramLevel) ([]*model.DefaultProgram, error) {
	filters := &model.DefaultProgramFilters{}

	if len(daysOfWeek) > 0 {
		if err := model.ValidateDaysOfWeek(daysOfWeek); err != nil {
			return nil, err
		}
		count := len(daysOfWeek)
		filters.DaysPerWeek = &count
	}
	if goal != nil {
		if !goal.IsValid() {
			return nil, fmt.Errorf("invalid goal %q: %w", *goal, model.ErrBadRequest)
		}
		filters.Goal = goal
	}
	if level != nil {
		if !level.IsValid() {
			return nil, fmt.Errorf("invalid level %q: %w", *level, model.ErrBadRequest)
		}
		filters.Level = level
	}

	list, err := s.repo.GetDefaultPrograms(ctx, filters)
	if err != nil {
		return nil, fmt.Errorf("failed to get default programs: %w", err)
	}
	return list, nil
}

func (s *ProgramService) GetDefaultProgramByID(ctx context.Context, id uuid.UUID) (*model.DefaultProgram, error) {
	res, err := s.repo.GetDefaultProgramByID(ctx, id)
	if errors.Is(err, repository.ErrNotFound) {
		return nil, model.ErrNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("failed to get default program with id %s: %w", id, err)
	}
	return res, nil
}

// GetUserPrograms returns programs of user without exercises
func (s *ProgramService) GetUserPrograms(ctx context.Context, userID uuid.UUID, state *model.ProgramState) ([]*model.UserProgram, error) {
	if state != nil && !state.IsValid() {
		return nil, fmt.Errorf("invalid state %q: %w", *state, model.ErrBadRequest)
	}

	list, err := s.repo.GetUserPrograms(ctx, userID, state)
	if err != nil {
		return nil, fmt.Errorf("failed to get programs: %w", err)
	}
	return list, nil
}

func (s *ProgramService) GetUserProgramByID(ctx context.Context, userID uuid.UUID, id uuid.UUID) (*model.UserProgram, error) {
	return s.getOwnedProgram(ctx, userID, id)
}

func (s *ProgramService) getOwnedProgram(ctx context.Context, userID uuid.UUID, id uuid.UUID) (*model.UserProgram, error) {
	program, err := s.repo.GetUserProgramByID(ctx, id)
	if errors.Is(err, repository.ErrNotFound) {
		return nil, model.ErrNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("failed to get program with id %s: %w", id, err)
	}

	if program.UserID != userID {
		return nil, model.ErrForbidden
	}

	if err = s.fillMeasurements(ctx, program); err != nil {
		return nil, err
	}
	return program, nil
}

// not started programs are in these states, others do not need personal records
func isNotStarted(state model.ProgramState) bool {
	return state == model.ProgramStateDraft || state == model.ProgramStateNeedsMeasurement
}

// fillMeasurements sets exercises with percent load in which the user has no personal record
func (s *ProgramService) fillMeasurements(ctx context.Context, program *model.UserProgram) error {
	program.RequiredMeasurements = []*model.MeasurementExercise{}
	if !isNotStarted(program.State) {
		return nil
	}

	missing, err := s.repo.GetMissingMeasurements(ctx, program.ID, program.UserID)
	if err != nil {
		return fmt.Errorf("failed to get missing measurements of program %s: %w", program.ID, err)
	}
	if missing != nil {
		program.RequiredMeasurements = missing
	}
	return nil
}

// syncMeasurementState makes a not started program needs_measurement, if personal records are missing, or draft
func (s *ProgramService) syncMeasurementState(ctx context.Context, program *model.UserProgram) error {
	if err := s.fillMeasurements(ctx, program); err != nil {
		return err
	}
	if !isNotStarted(program.State) {
		return nil
	}

	state := model.ProgramStateDraft
	if len(program.RequiredMeasurements) > 0 {
		state = model.ProgramStateNeedsMeasurement
	}
	if program.State == state {
		return nil
	}

	if err := s.repo.SetUserProgramState(ctx, program.ID, state); err != nil {
		return fmt.Errorf("failed to set state of program %s: %w", program.ID, err)
	}
	program.State = state
	return nil
}

// reloadProgram returns the program after a change, the state of a not started program is synchronized
func (s *ProgramService) reloadProgram(ctx context.Context, id uuid.UUID) (*model.UserProgram, error) {
	program, err := s.repo.GetUserProgramByID(ctx, id)
	if err != nil {
		return nil, fmt.Errorf("failed to get program %s: %w", id, err)
	}

	if err = s.syncMeasurementState(ctx, program); err != nil {
		return nil, err
	}
	return program, nil
}

// RefreshMeasurementStates synchronizes states of not started programs of the user, it is called after personal records change
func (s *ProgramService) RefreshMeasurementStates(ctx context.Context, userID uuid.UUID) error {
	programs, err := s.repo.GetUserPrograms(ctx, userID, nil)
	if err != nil {
		return fmt.Errorf("failed to get programs: %w", err)
	}

	for _, program := range programs {
		if !isNotStarted(program.State) {
			continue
		}
		if err = s.syncMeasurementState(ctx, program); err != nil {
			return err
		}
	}
	return nil
}

// CreateProgram creates a draft with metadata only, the structure is filled later
func (s *ProgramService) CreateProgram(ctx context.Context, userID uuid.UUID, input *model.CreateUserProgramInput) (*model.UserProgram, error) {
	if input == nil {
		return nil, fmt.Errorf("create program input can't be nil")
	}
	if err := input.Validate(); err != nil {
		return nil, err
	}

	weeksCount := 1
	if input.WeeksCount != nil {
		weeksCount = *input.WeeksCount
	}

	return s.createProgram(ctx, &model.UserProgram{
		UserID:     userID,
		Name:       input.Name,
		SourceKind: model.ProgramSourceKindCustom,
		Goal:       input.Goal,
		Level:      input.Level,
		DaysOfWeek: sortedDays(input.DaysOfWeek),
		WeeksCount: weeksCount,
	}, nil)
}

// CreateFromDefault copies default program and lays its workouts of every week out on the selected days of week
func (s *ProgramService) CreateFromDefault(ctx context.Context, userID uuid.UUID, defaultProgramID uuid.UUID, daysOfWeek []int) (*model.UserProgram, error) {
	if err := model.ValidateDaysOfWeek(daysOfWeek); err != nil {
		return nil, err
	}

	def, err := s.GetDefaultProgramByID(ctx, defaultProgramID)
	if err != nil {
		return nil, err
	}

	if len(daysOfWeek) != def.DaysPerWeek {
		return nil, model.ProgramDaysCountMismatch(def.DaysPerWeek, len(daysOfWeek))
	}

	weekdays := sortedDays(daysOfWeek)
	weeks := make([]*model.UserProgramWeek, def.WeeksCount)
	for i := range weeks {
		weeks[i] = &model.UserProgramWeek{}
	}

	// days are ordered by week and position in the week
	for _, day := range def.Days {
		if day.WeekNumber < 1 || day.WeekNumber > def.WeeksCount || day.WorkoutTemplate == nil {
			return nil, fmt.Errorf("default program %s has invalid day %s", def.ID, day.ID)
		}

		week := weeks[day.WeekNumber-1]
		if len(week.Days) >= len(weekdays) {
			return nil, fmt.Errorf("default program %s has more than %d days in week %d", def.ID, def.DaysPerWeek, day.WeekNumber)
		}
		week.Days = append(week.Days, dayFromTemplate(weekdays[len(week.Days)], day.WorkoutTemplate))
	}
	for i, week := range weeks {
		if len(week.Days) != len(weekdays) {
			return nil, fmt.Errorf("default program %s has %d days in week %d, want %d", def.ID, len(week.Days), i+1, def.DaysPerWeek)
		}
	}

	return s.createProgram(ctx, &model.UserProgram{
		UserID:           userID,
		Name:             def.Name,
		SourceKind:       model.ProgramSourceKindDefault,
		DefaultProgramID: &def.ID,
		Goal:             def.Goal,
		Level:            def.Level,
		DaysOfWeek:       weekdays,
		WeeksCount:       def.WeeksCount,
	}, weeks)
}

func (s *ProgramService) createProgram(ctx context.Context, program *model.UserProgram, weeks []*model.UserProgramWeek) (*model.UserProgram, error) {
	var res *model.UserProgram
	err := s.inTx(ctx, func(ctx context.Context) error {
		created, err := s.repo.CreateUserProgram(ctx, program, weeks)
		if err != nil {
			return fmt.Errorf("failed to create program: %w", err)
		}

		// reload to get exercise names
		res, err = s.reloadProgram(ctx, created.ID)
		if err != nil {
			return err
		}
		return nil
	})
	if err != nil {
		return nil, err
	}
	return res, nil
}

// UpdateProgram changes name, goal, level, days of week and number of weeks;
// exercises of removed days and weeks are deleted
func (s *ProgramService) UpdateProgram(ctx context.Context, userID uuid.UUID, input *model.UpdateUserProgramInput) (*model.UserProgram, error) {
	if input == nil {
		return nil, fmt.Errorf("update program input can't be nil")
	}
	if err := input.Validate(); err != nil {
		return nil, err
	}
	input.DaysOfWeek = sortedDays(input.DaysOfWeek)

	var res *model.UserProgram
	err := s.inTx(ctx, func(ctx context.Context) error {
		program, err := s.getOwnedProgram(ctx, userID, input.ID)
		if err != nil {
			return err
		}
		if program.State == model.ProgramStateFinished {
			return model.ProgramReadOnly()
		}

		_, err = s.repo.UpdateUserProgram(ctx, input)
		if errors.Is(err, repository.ErrNotFound) {
			return model.ProgramReadOnly()
		}
		if err != nil {
			return fmt.Errorf("failed to update program with id %s: %w", input.ID, err)
		}

		res, err = s.reloadProgram(ctx, input.ID)
		if err != nil {
			return err
		}
		return nil
	})
	if err != nil {
		return nil, err
	}
	return res, nil
}

// ReplaceStructure fully replaces days of week, number of weeks and exercises of the program,
// the number of weeks is the number of weeks in the structure
func (s *ProgramService) ReplaceStructure(ctx context.Context, userID uuid.UUID, programID uuid.UUID, structure *model.UserProgramStructure) (*model.UserProgram, error) {
	if structure == nil {
		return nil, fmt.Errorf("structure can't be nil")
	}
	if err := structure.Validate(); err != nil {
		return nil, err
	}
	structure.DaysOfWeek = sortedDays(structure.DaysOfWeek)

	var res *model.UserProgram
	err := s.inTx(ctx, func(ctx context.Context) error {
		program, err := s.getOwnedProgram(ctx, userID, programID)
		if err != nil {
			return err
		}
		if program.State == model.ProgramStateFinished {
			return model.ProgramReadOnly()
		}

		if err = s.checkExercisesAvailable(ctx, userID, program, structure.Weeks...); err != nil {
			return err
		}

		err = s.repo.ReplaceUserProgramStructure(ctx, programID, structure)
		if errors.Is(err, repository.ErrNoAffectedRows) {
			return model.ProgramReadOnly()
		}
		if err != nil {
			return fmt.Errorf("failed to replace structure of program %s: %w", programID, err)
		}

		res, err = s.reloadProgram(ctx, programID)
		if err != nil {
			return err
		}
		return nil
	})
	if err != nil {
		return nil, err
	}
	return res, nil
}

// ReplaceWeek replaces exercises of one week, days of the week must be days of the program
func (s *ProgramService) ReplaceWeek(ctx context.Context, userID uuid.UUID, programID uuid.UUID, weekNumber int, week *model.UserProgramWeek) (*model.UserProgram, error) {
	if week == nil {
		return nil, fmt.Errorf("week can't be nil")
	}

	var res *model.UserProgram
	err := s.inTx(ctx, func(ctx context.Context) error {
		program, err := s.getOwnedProgram(ctx, userID, programID)
		if err != nil {
			return err
		}
		if program.State == model.ProgramStateFinished {
			return model.ProgramReadOnly()
		}

		if weekNumber < 1 || weekNumber > program.WeeksCount {
			return fmt.Errorf("week %d is out of range 1..%d: %w", weekNumber, program.WeeksCount, model.ErrBadRequest)
		}
		if err = week.Validate(program.DaysOfWeek); err != nil {
			return err
		}
		if err = s.checkExercisesAvailable(ctx, userID, program, week); err != nil {
			return err
		}

		err = s.repo.ReplaceUserProgramWeek(ctx, programID, weekNumber, week)
		if errors.Is(err, repository.ErrNoAffectedRows) {
			return model.ProgramReadOnly()
		}
		if err != nil {
			return fmt.Errorf("failed to replace week %d of program %s: %w", weekNumber, programID, err)
		}

		res, err = s.reloadProgram(ctx, programID)
		if err != nil {
			return err
		}
		return nil
	})
	if err != nil {
		return nil, err
	}
	return res, nil
}

// exercises already in the program are kept even if they are deleted or hidden later
func (s *ProgramService) checkExercisesAvailable(ctx context.Context, userID uuid.UUID, program *model.UserProgram, weeks ...*model.UserProgramWeek) error {
	existing := model.NewSet[uuid.UUID]()
	for _, ex := range program.Exercises {
		existing[ex.ExerciseID] = struct{}{}
	}

	toCheck := []uuid.UUID{}
	for _, id := range model.ExerciseIDs(weeks...) {
		if _, ok := existing[id]; !ok {
			toCheck = append(toCheck, id)
		}
	}
	if len(toCheck) == 0 {
		return nil
	}

	ok, err := s.repo.AreExercisesAvailable(ctx, userID, toCheck)
	if err != nil {
		return fmt.Errorf("failed to check exercises: %w", err)
	}
	if !ok {
		return model.ProgramExerciseNotAvailable()
	}
	return nil
}

// StartProgram makes a draft in progress and finishes the previous program in progress
func (s *ProgramService) StartProgram(ctx context.Context, userID uuid.UUID, id uuid.UUID) (*model.StartProgramResult, error) {
	var res *model.StartProgramResult
	err := s.inTx(ctx, func(ctx context.Context) error {
		program, err := s.getOwnedProgram(ctx, userID, id)
		if err != nil {
			return err
		}
		if !isNotStarted(program.State) {
			return model.ProgramInvalidStateTransition(program.State, model.ProgramStateInProgress)
		}
		if len(program.RequiredMeasurements) > 0 {
			return model.ProgramMeasurementRequired()
		}
		if !isProgramComplete(program) {
			return model.ProgramIncomplete()
		}

		// the records were added after the program got needs_measurement and the state is not synchronized yet
		if program.State != model.ProgramStateDraft {
			if err = s.repo.SetUserProgramState(ctx, id, model.ProgramStateDraft); err != nil {
				return fmt.Errorf("failed to set state of program %s: %w", id, err)
			}
		}

		finishedID, err := s.repo.StartUserProgram(ctx, userID, id)
		if errors.Is(err, repository.ErrNoAffectedRows) {
			return model.ProgramInvalidStateTransition(program.State, model.ProgramStateInProgress)
		}
		if err != nil {
			return fmt.Errorf("failed to start program %s: %w", id, err)
		}

		started, err := s.repo.GetUserProgramByID(ctx, id)
		if err != nil {
			return fmt.Errorf("failed to get started program: %w", err)
		}
		res = &model.StartProgramResult{Program: started, FinishedProgramID: finishedID}
		return nil
	})
	if err != nil {
		return nil, err
	}
	return res, nil
}

// the program has training days and every day of every week has exercises
func isProgramComplete(program *model.UserProgram) bool {
	if len(program.DaysOfWeek) == 0 {
		return false
	}

	type cell struct{ week, day int }
	filled := make(map[cell]struct{}, len(program.Exercises))
	for _, ex := range program.Exercises {
		filled[cell{ex.WeekNumber, ex.DayOfWeek}] = struct{}{}
	}

	for week := 1; week <= program.WeeksCount; week++ {
		for _, day := range program.DaysOfWeek {
			if _, ok := filled[cell{week, day}]; !ok {
				return false
			}
		}
	}
	return true
}

func (s *ProgramService) FinishProgram(ctx context.Context, userID uuid.UUID, id uuid.UUID) (*model.UserProgram, error) {
	var res *model.UserProgram
	err := s.inTx(ctx, func(ctx context.Context) error {
		program, err := s.getOwnedProgram(ctx, userID, id)
		if err != nil {
			return err
		}
		if program.State != model.ProgramStateInProgress {
			return model.ProgramInvalidStateTransition(program.State, model.ProgramStateFinished)
		}

		err = s.repo.FinishUserProgram(ctx, id)
		if errors.Is(err, repository.ErrNoAffectedRows) {
			return model.ProgramInvalidStateTransition(program.State, model.ProgramStateFinished)
		}
		if err != nil {
			return fmt.Errorf("failed to finish program %s: %w", id, err)
		}

		res, err = s.repo.GetUserProgramByID(ctx, id)
		if err != nil {
			return fmt.Errorf("failed to get finished program: %w", err)
		}
		return nil
	})
	if err != nil {
		return nil, err
	}
	return res, nil
}

func (s *ProgramService) DeleteProgram(ctx context.Context, userID uuid.UUID, id uuid.UUID) error {
	return s.inTx(ctx, func(ctx context.Context) error {
		program, err := s.getOwnedProgram(ctx, userID, id)
		if err != nil {
			return err
		}
		if !isNotStarted(program.State) {
			return model.ProgramNotDraft()
		}

		err = s.repo.DeleteDraftUserProgram(ctx, id)
		if errors.Is(err, repository.ErrNoAffectedRows) {
			return model.ProgramNotDraft()
		}
		if err != nil {
			return fmt.Errorf("failed to delete program %s: %w", id, err)
		}
		return nil
	})
}

// sortedDays returns sorted copy, nil stays nil because it means "not set"
func sortedDays(days []int) []int {
	if days == nil {
		return nil
	}
	res := slices.Clone(days)
	slices.Sort(res)
	return res
}

func dayFromTemplate(dayOfWeek int, template *model.WorkoutTemplate) *model.UserProgramDay {
	day := &model.UserProgramDay{DayOfWeek: dayOfWeek}
	for _, ex := range template.Exercises {
		day.Exercises = append(day.Exercises, &model.UserProgramExercise{
			ExerciseID:       ex.ExerciseID,
			Sets:             ex.Sets,
			TargetReps:       ex.TargetReps,
			TargetPercent1RM: ex.TargetPercent1RM,
			RestSeconds:      ex.RestSeconds,
		})
	}
	return day
}
