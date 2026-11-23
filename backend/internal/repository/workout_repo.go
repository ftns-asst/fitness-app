package repository

import (
	"bytes"
	"context"
	"crypto/sha256"
	"encoding/json"
	"errors"
	"fmt"
	"time"

	"ftns-asst/internal/model"

	"github.com/google/uuid"
	"github.com/jackc/pgx/v5"
)

type WorkoutRepo struct {
	database DB
}

func NewWorkoutRepo(database DB) *WorkoutRepo {
	return &WorkoutRepo{database: database}
}

// SaveWorkout claims the client ID and writes the complete session in the same transaction.
// The hash survives catalog edits and removal of the associated program.
func (r *WorkoutRepo) SaveWorkout(ctx context.Context, userID, id uuid.UUID, input model.SaveWorkoutInput) (*model.Workout, bool, error) {
	input.StartedAt = input.StartedAt.UTC().Truncate(time.Microsecond)
	input.CompletedAt = input.CompletedAt.UTC().Truncate(time.Microsecond)
	payload, err := json.Marshal(input)
	if err != nil {
		return nil, false, fmt.Errorf("encode workout: %w", err)
	}
	hash := sha256.Sum256(payload)
	var workout *model.Workout
	var created bool
	err = NewTransactor(r.database).WithinTx(ctx, func(ctx context.Context) error {
		conn := Conn(ctx, r.database)
		tag, err := conn.Exec(ctx, `
			INSERT INTO workouts (id, user_id, program_id, week_number, day_of_week, started_at, completed_at, request_hash)
			SELECT $1, $2, $3, $4, $5, $6, $7, $8
			WHERE $3::uuid IS NULL OR EXISTS (SELECT 1 FROM user_programs WHERE id = $3 AND user_id = $2 FOR KEY SHARE)
			ON CONFLICT (id) DO NOTHING
		`, id, userID, input.ProgramID, input.WeekNumber, input.DayOfWeek, input.StartedAt, input.CompletedAt, hash[:])
		if err != nil {
			return fmt.Errorf("insert workout: %w", err)
		}
		created = tag.RowsAffected() == 1
		if created {
			if err := insertWorkoutExercises(ctx, conn, userID, id, input.Exercises); err != nil {
				return err
			}
		} else {
			var savedHash []byte
			err := conn.QueryRow(ctx, "SELECT request_hash FROM workouts WHERE user_id = $1 AND id = $2", userID, id).Scan(&savedHash)
			if errors.Is(err, pgx.ErrNoRows) {
				return ErrNotFound
			}
			if err != nil {
				return err
			}
			if !bytes.Equal(hash[:], savedHash) {
				return model.ErrConflict
			}
		}
		workout, err = readWorkout(ctx, conn, userID, id)
		return err
	})
	if err != nil {
		return nil, false, err
	}
	return workout, created, nil
}

func insertWorkoutExercises(ctx context.Context, conn DB, userID, workoutID uuid.UUID, exercises []model.WorkoutExerciseInput) error {
	ids := make([]uuid.UUID, 0, len(exercises))
	exerciseRows := make([][]any, 0, len(exercises))
	var setRows [][]any
	for i, e := range exercises {
		id := uuid.New()
		ids = append(ids, e.ExerciseID)
		exerciseRows = append(exerciseRows, []any{id, workoutID, e.ExerciseID, i, e.TargetSets, e.TargetReps, e.TargetWeightKg})
		for j, set := range e.Sets {
			setRows = append(setRows, []any{id, j + 1, set.Reps, set.WeightKg})
		}
	}
	ok, err := areExercisesAvailable(ctx, conn, userID, ids)
	if err != nil {
		return err
	}
	if !ok {
		return model.ProgramExerciseNotAvailable()
	}
	_, err = conn.CopyFrom(ctx, pgx.Identifier{"workout_exercises"},
		[]string{"id", "workout_id", "exercise_id", "order_index", "target_sets", "target_reps", "target_weight_kg"}, pgx.CopyFromRows(exerciseRows))
	if err != nil {
		return fmt.Errorf("insert workout exercises: %w", err)
	}
	_, err = conn.CopyFrom(ctx, pgx.Identifier{"workout_sets"},
		[]string{"workout_exercise_id", "set_number", "reps", "weight_kg"}, pgx.CopyFromRows(setRows))
	if err != nil {
		return fmt.Errorf("insert workout sets: %w", err)
	}
	return nil
}

const workoutColumns = "id, program_id, week_number, day_of_week, started_at, completed_at"

func (r *WorkoutRepo) ListWorkouts(ctx context.Context, userID uuid.UUID, programID *uuid.UUID, page model.PageInfo) ([]model.WorkoutSummary, error) {
	page.Normalize()
	rows, err := Conn(ctx, r.database).Query(ctx, `SELECT `+workoutColumns+` FROM workouts
		WHERE user_id = $1 AND ($2::uuid IS NULL OR program_id = $2)
		ORDER BY completed_at DESC, id DESC LIMIT $3 OFFSET $4`, userID, programID, page.Limit, page.Offset)
	if err != nil {
		return nil, err
	}
	return pgx.CollectRows(rows, pgx.RowToStructByName[model.WorkoutSummary])
}

func (r *WorkoutRepo) GetWorkout(ctx context.Context, userID, id uuid.UUID) (*model.Workout, error) {
	return readWorkout(ctx, Conn(ctx, r.database), userID, id)
}

func readWorkout(ctx context.Context, conn DB, userID, id uuid.UUID) (*model.Workout, error) {
	rows, err := conn.Query(ctx, `SELECT `+workoutColumns+` FROM workouts WHERE user_id = $1 AND id = $2`, userID, id)
	if err != nil {
		return nil, err
	}
	header, err := pgx.CollectOneRow(rows, pgx.RowToStructByName[model.WorkoutSummary])
	if errors.Is(err, pgx.ErrNoRows) {
		return nil, ErrNotFound
	}
	if err != nil {
		return nil, err
	}
	rows, err = conn.Query(ctx, `
		SELECT we.id, we.exercise_id, e.name, we.target_sets, we.target_reps, we.target_weight_kg,
			s.id, s.set_number, s.reps, s.weight_kg
		FROM workout_exercises we
		JOIN workouts w ON w.id = we.workout_id
		JOIN exercises e ON e.id = we.exercise_id
		JOIN workout_sets s ON s.workout_exercise_id = we.id
		WHERE we.workout_id = $1 AND w.user_id = $2
		ORDER BY we.order_index, s.set_number`, id, userID)
	if err != nil {
		return nil, err
	}
	defer rows.Close()
	result := &model.Workout{WorkoutSummary: header}
	for rows.Next() {
		var exercise model.WorkoutExercise
		var set model.WorkoutSet
		err := rows.Scan(&exercise.ID, &exercise.ExerciseID, &exercise.ExerciseName, &exercise.TargetSets, &exercise.TargetReps, &exercise.TargetWeightKg,
			&set.ID, &set.SetNumber, &set.Reps, &set.WeightKg)
		if err != nil {
			return nil, err
		}
		last := len(result.Exercises) - 1
		if last < 0 || result.Exercises[last].ID != exercise.ID {
			result.Exercises = append(result.Exercises, exercise)
			last++
		}
		result.Exercises[last].Sets = append(result.Exercises[last].Sets, set)
	}
	if err := rows.Err(); err != nil {
		return nil, err
	}
	return result, nil
}
