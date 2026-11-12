package repository

import (
	"context"
	"fmt"
	"ftns-asst/internal/model"

	"github.com/google/uuid"
	"github.com/jackc/pgx/v5"
)

type PersonalRecordRepo struct {
	database db
}

func NewPersonalRecordRepo(db db) *PersonalRecordRepo {
	return &PersonalRecordRepo{
		database: db,
	}
}

// try get tx from ctx
func (r *PersonalRecordRepo) db(ctx context.Context) db {
	res := r.database
	if tx := ctx.Value(model.ContextKeyTx); tx != nil {
		res = tx.(db)
	}
	return res
}

// CreateRecord adds a new entry, the previous ones are kept as history
func (r *PersonalRecordRepo) CreateRecord(ctx context.Context, entry *model.PersonalRecord) (*model.PersonalRecord, error) {
	rows, err := r.db(ctx).Query(ctx, `
		WITH inserted AS (
			INSERT INTO personal_records (user_id, exercise_id, weight_kg, is_calculated)
			VALUES ($1, $2, $3, $4)
			RETURNING *
		)
		SELECT inserted.id, inserted.user_id, inserted.exercise_id, e.name AS exercise_name,
			inserted.weight_kg, inserted.is_calculated, inserted.created_at
		FROM inserted
		JOIN exercises e ON e.id = inserted.exercise_id
	`, entry.UserID, entry.ExerciseID, entry.WeightKg, entry.IsCalculated)
	if err != nil {
		return nil, fmt.Errorf("insert failed: %w", err)
	}

	res, err := pgx.CollectOneRow(rows, pgx.RowToAddrOfStructByName[model.PersonalRecord])
	if err != nil {
		return nil, fmt.Errorf("collect failed: %w", err)
	}

	return res, nil
}

// GetLatestRecords returns the current record for each exercise of the user; empty exerciseIDs mean all exercises
func (r *PersonalRecordRepo) GetLatestRecords(ctx context.Context, userID uuid.UUID, exerciseIDs []uuid.UUID) ([]*model.PersonalRecord, error) {
	// not nil: NULL array would match nothing
	if exerciseIDs == nil {
		exerciseIDs = []uuid.UUID{}
	}

	rows, err := r.db(ctx).Query(ctx, `
		SELECT DISTINCT ON (m.exercise_id)
			m.id, m.user_id, m.exercise_id, e.name AS exercise_name, m.weight_kg, m.is_calculated, m.created_at
		FROM personal_records m
		JOIN exercises e ON e.id = m.exercise_id
		WHERE m.user_id = $1
			AND (cardinality($2::uuid[]) = 0 OR m.exercise_id = ANY($2::uuid[]))
		ORDER BY m.exercise_id, m.created_at DESC, m.id DESC
	`, userID, exerciseIDs)
	if err != nil {
		return nil, fmt.Errorf("query failed: %w", err)
	}

	res, err := pgx.CollectRows(rows, pgx.RowToAddrOfStructByName[model.PersonalRecord])
	if err != nil {
		return nil, fmt.Errorf("collect failed: %w", err)
	}

	return res, nil
}

// GetRecordHistory returns all entries of the user in the exercise, the newest first
func (r *PersonalRecordRepo) GetRecordHistory(ctx context.Context, userID uuid.UUID, exerciseID uuid.UUID) ([]*model.PersonalRecord, error) {
	rows, err := r.db(ctx).Query(ctx, `
		SELECT m.id, m.user_id, m.exercise_id, e.name AS exercise_name, m.weight_kg, m.is_calculated, m.created_at
		FROM personal_records m
		JOIN exercises e ON e.id = m.exercise_id
		WHERE m.user_id = $1
			AND m.exercise_id = $2
		ORDER BY m.created_at DESC, m.id DESC
	`, userID, exerciseID)
	if err != nil {
		return nil, fmt.Errorf("query failed: %w", err)
	}

	res, err := pgx.CollectRows(rows, pgx.RowToAddrOfStructByName[model.PersonalRecord])
	if err != nil {
		return nil, fmt.Errorf("collect failed: %w", err)
	}

	return res, nil
}

// DeleteRecord removes one entry of the user
func (r *PersonalRecordRepo) DeleteRecord(ctx context.Context, userID uuid.UUID, id uuid.UUID) error {
	res, err := r.db(ctx).Exec(ctx, `
		DELETE FROM personal_records
		WHERE id = $1
			AND user_id = $2
	`, id, userID)
	if err != nil {
		return fmt.Errorf("delete failed: %w", err)
	}

	if res.RowsAffected() == 0 {
		return ErrNoAffectedRows
	}

	return nil
}

// AreExercisesAvailable checks that all exercises exist, are not deleted and are basic, public or own
func (r *PersonalRecordRepo) AreExercisesAvailable(ctx context.Context, userID uuid.UUID, exerciseIDs []uuid.UUID) (bool, error) {
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
