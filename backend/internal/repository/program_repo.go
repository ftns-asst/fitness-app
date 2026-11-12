package repository

import (
	"context"
	"errors"
	"fmt"
	"ftns-asst/internal/model"
	"strings"

	"github.com/google/uuid"
	"github.com/jackc/pgx/v5"
)

type ProgramRepo struct {
	database db
}

func NewProgramRepo(db db) *ProgramRepo {
	return &ProgramRepo{
		database: db,
	}
}

// try get tx from ctx
func (r *ProgramRepo) db(ctx context.Context) db {
	res := r.database
	if tx := ctx.Value(model.ContextKeyTx); tx != nil {
		res = tx.(db)
	}
	return res
}

func (r *ProgramRepo) GetDefaultPrograms(ctx context.Context, filters *model.DefaultProgramFilters) ([]*model.DefaultProgram, error) {
	args := []interface{}{}
	query := strings.Builder{}
	query.WriteString(`
		SELECT *
		FROM default_programs
		WHERE TRUE
	`)

	if filters.DaysPerWeek != nil {
		fmt.Fprintf(&query, " AND days_per_week = $%d", len(args)+1)
		args = append(args, *filters.DaysPerWeek)
	}

	if filters.Goal != nil {
		fmt.Fprintf(&query, " AND goal = $%d", len(args)+1)
		args = append(args, *filters.Goal)
	}

	if filters.Level != nil {
		fmt.Fprintf(&query, " AND level = $%d", len(args)+1)
		args = append(args, *filters.Level)
	}

	query.WriteString(" ORDER BY name")

	rows, err := r.db(ctx).Query(ctx, query.String(), args...)
	if err != nil {
		return nil, fmt.Errorf("query failed: %w", err)
	}

	res, err := pgx.CollectRows(rows, pgx.RowToAddrOfStructByName[model.DefaultProgram])
	if err != nil {
		return nil, fmt.Errorf("collect failed: %w", err)
	}

	return res, nil
}

func (r *ProgramRepo) GetDefaultProgramByID(ctx context.Context, id uuid.UUID) (*model.DefaultProgram, error) {
	conn := r.db(ctx)

	rows, err := conn.Query(ctx, `SELECT * FROM default_programs WHERE id = $1`, id)
	if err != nil {
		return nil, fmt.Errorf("query failed: %w", err)
	}

	res, err := pgx.CollectOneRow(rows, pgx.RowToAddrOfStructByName[model.DefaultProgram])
	if errors.Is(err, pgx.ErrNoRows) {
		return nil, ErrNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("collect failed: %w", err)
	}

	rows, err = conn.Query(ctx, `
		SELECT *
		FROM default_program_days
		WHERE default_program_id = $1
		ORDER BY week_number, order_index
	`, id)
	if err != nil {
		return nil, fmt.Errorf("days query failed: %w", err)
	}

	res.Days, err = pgx.CollectRows(rows, pgx.RowToAddrOfStructByName[model.DefaultProgramDay])
	if err != nil {
		return nil, fmt.Errorf("days collect failed: %w", err)
	}

	templateIDs := make([]uuid.UUID, 0, len(res.Days))
	for _, day := range res.Days {
		templateIDs = append(templateIDs, day.WorkoutTemplateID)
	}

	rows, err = conn.Query(ctx, `SELECT * FROM workout_templates WHERE id = ANY($1::uuid[])`, templateIDs)
	if err != nil {
		return nil, fmt.Errorf("templates query failed: %w", err)
	}

	templates, err := pgx.CollectRows(rows, pgx.RowToAddrOfStructByName[model.WorkoutTemplate])
	if err != nil {
		return nil, fmt.Errorf("templates collect failed: %w", err)
	}

	if err = r.fillTemplateExercises(ctx, conn, templates); err != nil {
		return nil, err
	}

	templatesByID := make(map[uuid.UUID]*model.WorkoutTemplate, len(templates))
	for _, template := range templates {
		templatesByID[template.ID] = template
	}
	for _, day := range res.Days {
		day.WorkoutTemplate = templatesByID[day.WorkoutTemplateID]
	}

	return res, nil
}

func (r *ProgramRepo) fillTemplateExercises(ctx context.Context, conn db, templates []*model.WorkoutTemplate) error {
	templatesByID := make(map[uuid.UUID]*model.WorkoutTemplate, len(templates))
	ids := make([]uuid.UUID, 0, len(templates))
	for _, template := range templates {
		templatesByID[template.ID] = template
		ids = append(ids, template.ID)
	}

	rows, err := conn.Query(ctx, `
		SELECT te.id, te.workout_template_id, te.exercise_id, e.name AS exercise_name,
			te.order_index, te.sets, te.target_reps, te.target_percent_1rm, te.rest_seconds
		FROM workout_template_exercises te
		JOIN exercises e ON e.id = te.exercise_id
		WHERE te.workout_template_id = ANY($1::uuid[])
		ORDER BY te.workout_template_id, te.order_index
	`, ids)
	if err != nil {
		return fmt.Errorf("template exercises query failed: %w", err)
	}

	exercises, err := pgx.CollectRows(rows, pgx.RowToAddrOfStructByName[model.WorkoutTemplateExercise])
	if err != nil {
		return fmt.Errorf("template exercises collect failed: %w", err)
	}

	for _, exercise := range exercises {
		template := templatesByID[exercise.WorkoutTemplateID]
		template.Exercises = append(template.Exercises, exercise)
	}

	return nil
}

// CreateUserProgram saves the program and its weeks, weeks may be nil for an empty program
func (r *ProgramRepo) CreateUserProgram(ctx context.Context, program *model.UserProgram, weeks []*model.UserProgramWeek) (*model.UserProgram, error) {
	tx, err := r.db(ctx).Begin(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed begin transaction: %w", err)
	}
	defer model.CheckRollback(ctx, tx)

	// not nil: NULL violates not null constraint
	daysOfWeek := program.DaysOfWeek
	if daysOfWeek == nil {
		daysOfWeek = []int{}
	}
	if program.WeeksCount == 0 {
		program.WeeksCount = 1
	}

	err = tx.QueryRow(ctx, `
		INSERT INTO user_programs (user_id, name, source_kind, default_program_id, goal, level, days_of_week, weeks_count)
		VALUES ($1, $2, $3, $4, $5, $6, $7::smallint[], $8)
		RETURNING id, state, created_at, updated_at
	`,
		program.UserID,
		program.Name,
		program.SourceKind,
		program.DefaultProgramID,
		program.Goal,
		program.Level,
		daysOfWeek,
		program.WeeksCount).
		Scan(&program.ID, &program.State, &program.CreatedAt, &program.UpdatedAt)
	if err != nil {
		return nil, fmt.Errorf("insert failed: %w", err)
	}
	program.DaysOfWeek = daysOfWeek

	if err = r.saveWeeks(ctx, tx, program.ID, weeks); err != nil {
		return nil, err
	}

	if err = tx.Commit(ctx); err != nil {
		return nil, fmt.Errorf("failed commit transaction: %w", err)
	}

	return program, nil
}

func (r *ProgramRepo) GetUserPrograms(ctx context.Context, userID uuid.UUID, state *model.ProgramState) ([]*model.UserProgram, error) {
	rows, err := r.db(ctx).Query(ctx, `
		SELECT *
		FROM user_programs
		WHERE user_id = $1
			AND ($2::text IS NULL OR state = $2)
		ORDER BY updated_at DESC, created_at DESC
	`, userID, state)
	if err != nil {
		return nil, fmt.Errorf("query failed: %w", err)
	}

	res, err := pgx.CollectRows(rows, pgx.RowToAddrOfStructByName[model.UserProgram])
	if err != nil {
		return nil, fmt.Errorf("collect failed: %w", err)
	}

	return res, nil
}

// GetUserProgramByID returns the program with all exercises ordered by week, day and position
func (r *ProgramRepo) GetUserProgramByID(ctx context.Context, id uuid.UUID) (*model.UserProgram, error) {
	conn := r.db(ctx)

	rows, err := conn.Query(ctx, `SELECT * FROM user_programs WHERE id = $1`, id)
	if err != nil {
		return nil, fmt.Errorf("query failed: %w", err)
	}

	res, err := pgx.CollectOneRow(rows, pgx.RowToAddrOfStructByName[model.UserProgram])
	if errors.Is(err, pgx.ErrNoRows) {
		return nil, ErrNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("collect failed: %w", err)
	}

	rows, err = conn.Query(ctx, `
		SELECT pe.id, pe.program_id, pe.week_number, pe.day_of_week, pe.exercise_id, e.name AS exercise_name,
			pe.order_index, pe.sets, pe.target_reps, pe.target_weight_kg, pe.target_percent_1rm, pe.rest_seconds
		FROM user_program_exercises pe
		JOIN exercises e ON e.id = pe.exercise_id
		WHERE pe.program_id = $1
		ORDER BY pe.week_number, pe.day_of_week, pe.order_index
	`, id)
	if err != nil {
		return nil, fmt.Errorf("exercises query failed: %w", err)
	}

	res.Exercises, err = pgx.CollectRows(rows, pgx.RowToAddrOfStructByName[model.UserProgramExercise])
	if err != nil {
		return nil, fmt.Errorf("exercises collect failed: %w", err)
	}

	return res, nil
}

// UpdateUserProgram changes set fields; exercises of removed days and weeks are deleted
func (r *ProgramRepo) UpdateUserProgram(ctx context.Context, input *model.UpdateUserProgramInput) (*model.UserProgram, error) {
	tx, err := r.db(ctx).Begin(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed begin transaction: %w", err)
	}
	defer model.CheckRollback(ctx, tx)

	rows, err := tx.Query(ctx, `
		UPDATE user_programs
		SET
			name = COALESCE($2, name),
			goal = COALESCE($3, goal),
			level = COALESCE($4, level),
			days_of_week = COALESCE($5::smallint[], days_of_week),
			weeks_count = COALESCE($6, weeks_count),
			updated_at = NOW()
		WHERE id = $1
			AND state <> 'finished'
		RETURNING *
	`, input.ID, input.Name, input.Goal, input.Level, input.DaysOfWeek, input.WeeksCount)
	if err != nil {
		return nil, fmt.Errorf("update failed: %w", err)
	}

	res, err := pgx.CollectOneRow(rows, pgx.RowToAddrOfStructByName[model.UserProgram])
	if errors.Is(err, pgx.ErrNoRows) {
		return nil, ErrNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("collect failed: %w", err)
	}

	if input.DaysOfWeek != nil || input.WeeksCount != nil {
		_, err = tx.Exec(ctx, `
			DELETE FROM user_program_exercises
			WHERE program_id = $1
				AND (week_number > $2 OR NOT (day_of_week = ANY($3::smallint[])))
		`, res.ID, res.WeeksCount, res.DaysOfWeek)
		if err != nil {
			return nil, fmt.Errorf("exercises delete failed: %w", err)
		}
	}

	if err = tx.Commit(ctx); err != nil {
		return nil, fmt.Errorf("failed commit transaction: %w", err)
	}

	return res, nil
}

// ReplaceUserProgramStructure replaces days of week, number of weeks and all exercises
func (r *ProgramRepo) ReplaceUserProgramStructure(ctx context.Context, programID uuid.UUID, structure *model.UserProgramStructure) error {
	tx, err := r.db(ctx).Begin(ctx)
	if err != nil {
		return fmt.Errorf("failed begin transaction: %w", err)
	}
	defer model.CheckRollback(ctx, tx)

	daysOfWeek := structure.DaysOfWeek
	if daysOfWeek == nil {
		daysOfWeek = []int{}
	}

	res, err := tx.Exec(ctx, `
		UPDATE user_programs
		SET days_of_week = $2::smallint[], weeks_count = $3, updated_at = NOW()
		WHERE id = $1
			AND state <> 'finished'
	`, programID, daysOfWeek, structure.WeeksCount())
	if err != nil {
		return fmt.Errorf("update failed: %w", err)
	}

	if res.RowsAffected() == 0 {
		return ErrNoAffectedRows
	}

	_, err = tx.Exec(ctx, `DELETE FROM user_program_exercises WHERE program_id = $1`, programID)
	if err != nil {
		return fmt.Errorf("exercises delete failed: %w", err)
	}

	if err = r.saveWeeks(ctx, tx, programID, structure.Weeks); err != nil {
		return err
	}

	if err = tx.Commit(ctx); err != nil {
		return fmt.Errorf("failed commit transaction: %w", err)
	}

	return nil
}

// ReplaceUserProgramWeek replaces exercises of one week, the week must exist in the program
func (r *ProgramRepo) ReplaceUserProgramWeek(ctx context.Context, programID uuid.UUID, weekNumber int, week *model.UserProgramWeek) error {
	tx, err := r.db(ctx).Begin(ctx)
	if err != nil {
		return fmt.Errorf("failed begin transaction: %w", err)
	}
	defer model.CheckRollback(ctx, tx)

	res, err := tx.Exec(ctx, `
		UPDATE user_programs
		SET updated_at = NOW()
		WHERE id = $1
			AND state <> 'finished'
			AND weeks_count >= $2
	`, programID, weekNumber)
	if err != nil {
		return fmt.Errorf("update failed: %w", err)
	}

	if res.RowsAffected() == 0 {
		return ErrNoAffectedRows
	}

	_, err = tx.Exec(ctx, `DELETE FROM user_program_exercises WHERE program_id = $1 AND week_number = $2`, programID, weekNumber)
	if err != nil {
		return fmt.Errorf("exercises delete failed: %w", err)
	}

	if err = r.saveWeek(ctx, tx, programID, weekNumber, week); err != nil {
		return err
	}

	if err = tx.Commit(ctx); err != nil {
		return fmt.Errorf("failed commit transaction: %w", err)
	}

	return nil
}

// saveWeeks saves exercises, position of the week in weeks is week number - 1
func (r *ProgramRepo) saveWeeks(ctx context.Context, conn db, programID uuid.UUID, weeks []*model.UserProgramWeek) error {
	for i, week := range weeks {
		if err := r.saveWeek(ctx, conn, programID, i+1, week); err != nil {
			return err
		}
	}
	return nil
}

func (r *ProgramRepo) saveWeek(ctx context.Context, conn db, programID uuid.UUID, weekNumber int, week *model.UserProgramWeek) error {
	rows := [][]interface{}{}
	for _, day := range week.Days {
		for i, exercise := range day.Exercises {
			rows = append(rows, []interface{}{
				programID,
				weekNumber,
				day.DayOfWeek,
				exercise.ExerciseID,
				i,
				exercise.Sets,
				exercise.TargetReps,
				exercise.TargetWeightKg,
				exercise.TargetPercent1RM,
				exercise.RestSeconds,
			})
		}
	}

	_, err := conn.CopyFrom(ctx, pgx.Identifier{"user_program_exercises"},
		[]string{
			"program_id",
			"week_number",
			"day_of_week",
			"exercise_id",
			"order_index",
			"sets",
			"target_reps",
			"target_weight_kg",
			"target_percent_1rm",
			"rest_seconds",
		},
		pgx.CopyFromRows(rows),
	)
	if err != nil {
		return fmt.Errorf("exercises copy failed: %w", err)
	}

	return nil
}

// StartUserProgram makes a draft in progress, finishes the previous one and returns its id, if any
func (r *ProgramRepo) StartUserProgram(ctx context.Context, userID uuid.UUID, programID uuid.UUID) (*uuid.UUID, error) {
	tx, err := r.db(ctx).Begin(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed begin transaction: %w", err)
	}
	defer model.CheckRollback(ctx, tx)

	var finishedID *uuid.UUID
	err = tx.QueryRow(ctx, `
		UPDATE user_programs
		SET state = 'finished', updated_at = NOW()
		WHERE user_id = $1
			AND state = 'in_progress'
			AND id <> $2
		RETURNING id
	`, userID, programID).Scan(&finishedID)
	if err != nil && !errors.Is(err, pgx.ErrNoRows) {
		return nil, fmt.Errorf("finish previous failed: %w", err)
	}

	res, err := tx.Exec(ctx, `
		UPDATE user_programs
		SET state = 'in_progress', start_date = CURRENT_DATE, updated_at = NOW()
		WHERE id = $1
			AND user_id = $2
			AND state = 'draft'
	`, programID, userID)
	if err != nil {
		return nil, fmt.Errorf("start failed: %w", err)
	}

	if res.RowsAffected() == 0 {
		return nil, ErrNoAffectedRows
	}

	if err = tx.Commit(ctx); err != nil {
		return nil, fmt.Errorf("failed commit transaction: %w", err)
	}

	return finishedID, nil
}

func (r *ProgramRepo) FinishUserProgram(ctx context.Context, id uuid.UUID) error {
	res, err := r.db(ctx).Exec(ctx, `
		UPDATE user_programs
		SET state = 'finished', updated_at = NOW()
		WHERE id = $1
			AND state = 'in_progress'
	`, id)
	if err != nil {
		return fmt.Errorf("finish failed: %w", err)
	}

	if res.RowsAffected() == 0 {
		return ErrNoAffectedRows
	}

	return nil
}

// GetMissingMeasurements returns exercises of the program with percent load in which the user has no personal record
func (r *ProgramRepo) GetMissingMeasurements(ctx context.Context, programID uuid.UUID, userID uuid.UUID) ([]*model.MeasurementExercise, error) {
	rows, err := r.db(ctx).Query(ctx, `
		SELECT DISTINCT e.id AS exercise_id, e.name AS exercise_name
		FROM user_program_exercises pe
		JOIN exercises e ON e.id = pe.exercise_id
		WHERE pe.program_id = $1
			AND pe.target_percent_1rm IS NOT NULL
			AND NOT EXISTS (
				SELECT 1
				FROM personal_records pr
				WHERE pr.user_id = $2
					AND pr.exercise_id = pe.exercise_id
			)
		ORDER BY e.name, e.id
	`, programID, userID)
	if err != nil {
		return nil, fmt.Errorf("query failed: %w", err)
	}

	res, err := pgx.CollectRows(rows, pgx.RowToAddrOfStructByName[model.MeasurementExercise])
	if err != nil {
		return nil, fmt.Errorf("collect failed: %w", err)
	}

	return res, nil
}

// SetUserProgramState switches a not started program between draft and needs_measurement,
// updated_at stays the same: the program was not changed by the user
func (r *ProgramRepo) SetUserProgramState(ctx context.Context, id uuid.UUID, state model.ProgramState) error {
	_, err := r.db(ctx).Exec(ctx, `
		UPDATE user_programs
		SET state = $2
		WHERE id = $1
			AND state IN ('draft', 'needs_measurement')
	`, id, state)
	if err != nil {
		return fmt.Errorf("update state failed: %w", err)
	}

	return nil
}

func (r *ProgramRepo) DeleteDraftUserProgram(ctx context.Context, id uuid.UUID) error {
	res, err := r.db(ctx).Exec(ctx, `
		DELETE FROM user_programs
		WHERE id = $1
			AND state IN ('draft', 'needs_measurement')
	`, id)
	if err != nil {
		return fmt.Errorf("delete failed: %w", err)
	}

	if res.RowsAffected() == 0 {
		return ErrNoAffectedRows
	}

	return nil
}

// checks that all exercises exist, are not deleted and are basic, public or own
func (r *ProgramRepo) AreExercisesAvailable(ctx context.Context, userID uuid.UUID, exerciseIDs []uuid.UUID) (bool, error) {
	unique := model.NewSet(exerciseIDs...)

	count := 0
	err := r.db(ctx).QueryRow(ctx, `
		SELECT COUNT(*)
		FROM exercises
		WHERE id = ANY($1::uuid[])
			AND deleted_at IS NULL
			AND (is_public = TRUE OR owner_id = $2)
	`, exerciseIDs, userID).Scan(&count)
	if err != nil {
		return false, fmt.Errorf("select failed: %w", err)
	}

	return count == len(unique), nil
}
