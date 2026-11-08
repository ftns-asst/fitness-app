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

type ExerciseRepo struct {
	database db
}

func NewExerciseRepo(db db) *ExerciseRepo {
	return &ExerciseRepo{
		database: db,
	}
}

const exerciseColumns = `
	id, owner_id, media_id, is_public, name, description,
	muscle_group, equipment, created_at, deleted_at`

// try get tx from ctx
func (r *ExerciseRepo) db(ctx context.Context) db {
	res := r.database
	if tx := ctx.Value(model.ContextKeyTx); tx != nil {
		res = tx.(db)
	}
	return res
}

func (r *ExerciseRepo) CreateExercise(ctx context.Context, exercise *model.Exercise) (*model.Exercise, error) {
	conn := r.db(ctx)

	query := `
		INSERT INTO exercises (
			owner_id,
			is_public,
			name,
			description,
			muscle_group,
			equipment
		)
		VALUES ($1, $2, $3, $4, COALESCE($5::text[], '{}'), COALESCE($6::text[], '{}'))
		RETURNING id, created_at
	`

	err := conn.QueryRow(ctx, query,
		exercise.OwnerID,
		exercise.IsPublic,
		exercise.Name,
		exercise.Description,
		exercise.MuscleGroups,
		exercise.Equipments).
		Scan(&exercise.ID, &exercise.CreatedAt)
	if err != nil {
		return nil, fmt.Errorf("insert failed: %w", err)
	}

	if exercise.MuscleGroups == nil {
		exercise.MuscleGroups = []model.MuscleGroupKey{}
	}
	if exercise.Equipments == nil {
		exercise.Equipments = []model.EquipmentKey{}
	}
	return exercise, nil
}

func (r *ExerciseRepo) FillExercises(ctx context.Context, exercises []*model.Exercise) error {
	conn := r.db(ctx)

	tx, err := conn.Begin(ctx)
	if err != nil {
		return fmt.Errorf("failed begin transaction: %w", err)
	}
	defer model.CheckRollback(ctx, tx)

	conn = tx
	_, err = conn.Exec(ctx, `
		CREATE TEMP TABLE tmp_exercises (LIKE exercises INCLUDING DEFAULTS)
		ON COMMIT DROP`)
	if err != nil {
		return err
	}

	count, err := conn.CopyFrom(ctx, pgx.Identifier{"tmp_exercises"},
		[]string{
			"id",
			"owner_id",
			"is_public",
			"name",
			"description",
			"muscle_group",
			"equipment",
			"created_at"},
		pgx.CopyFromSlice(len(exercises), func(i int) ([]interface{}, error) {
			ex := exercises[i]
			return []interface{}{
				ex.ID,
				ex.OwnerID,
				ex.IsPublic,
				ex.Name,
				ex.Description,
				ex.MuscleGroups,
				ex.Equipments,
				ex.CreatedAt,
			}, nil
		}),
	)
	if err != nil {
		return fmt.Errorf("copy failed: %w", err)
	}

	if count != int64(len(exercises)) {
		return fmt.Errorf("copy failed: inserted %d, expected %d", count, len(exercises))
	}

	_, err = conn.Exec(ctx, `
        INSERT INTO exercises (id, owner_id, is_public, name, description, muscle_group, equipment)
		SELECT id, NULL, TRUE, name, description, muscle_group, equipment
		FROM tmp_exercises
		ON CONFLICT (id) DO UPDATE SET
		    name         = EXCLUDED.name,
		    description  = EXCLUDED.description,
		    muscle_group = EXCLUDED.muscle_group,
		    equipment    = EXCLUDED.equipment,
		    is_public    = TRUE,
		    deleted_at   = NULL
		WHERE exercises.owner_id IS NULL
    `)

	if err != nil {
		return fmt.Errorf("insert from tmp table failed: %w", err)
	}

	err = tx.Commit(ctx)
	if err != nil {
		return fmt.Errorf("failed commit transaction: %w", err)
	}
	return nil
}

func (r *ExerciseRepo) GetExerciseByID(ctx context.Context, id uuid.UUID) (*model.Exercise, error) {
	rows, err := r.db(ctx).Query(ctx, `SELECT `+exerciseColumns+` FROM exercises WHERE id = $1`, id)
	if err != nil {
		return nil, fmt.Errorf("query failed: %w", err)
	}

	res, err := pgx.CollectOneRow(rows, pgx.RowToAddrOfStructByName[model.Exercise])
	if errors.Is(err, pgx.ErrNoRows) {
		return nil, ErrNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("collect failed: %w", err)
	}
	return res, nil
}

func (r *ExerciseRepo) GetExercises(ctx context.Context, filters *model.ExerciseFilters, page *model.PageInfo) ([]*model.Exercise, error) {
	args := []interface{}{}
	query := strings.Builder{}
	query.WriteString(`SELECT ` + exerciseColumns + ` FROM exercises WHERE deleted_at IS NULL`)

	switch filters.ListType {
	case model.ExerciseListTypeOwn:
		query.WriteString(" AND owner_id = $1")
		args = append(args, filters.UserID)
	case model.ExerciseListTypePublicUser:
		query.WriteString(" AND is_public = TRUE AND owner_id = $1")
		args = append(args, filters.UserID)
	case model.ExerciseListTypeAll:
		query.WriteString(" AND (is_public = TRUE OR owner_id = $1)")
		args = append(args, filters.UserID)
	case model.ExerciseListTypePublic:
		query.WriteString(" AND is_public = TRUE AND owner_id IS NOT NULL")
		if filters.UserID != nil { // if user authenticated, do not return own in public list
			query.WriteString(" AND owner_id != $1")
			args = append(args, filters.UserID)
		}
	case model.ExerciseListTypeBasic:
		query.WriteString(" AND owner_id IS NULL AND is_public = TRUE")
	}

	// muscle groups filter
	if len(filters.MuscleGroups) > 0 {
		fmt.Fprintf(&query, " AND muscle_group && $%d", len(args)+1) // any intersection
		args = append(args, filters.MuscleGroups)
	}

	// equipments filter
	if len(filters.Equipments) > 0 {
		fmt.Fprintf(&query, " AND equipment <@ $%d", len(args)+1)
		args = append(args, filters.Equipments)
	}

	if filters.Search != nil {
		fmt.Fprint(&query, " AND name ILIKE '%' || "+fmt.Sprintf("$%d", len(args)+1)+" || '%' ESCAPE '\\'")
		args = append(args, likeEscaper.Replace(*filters.Search))
	}

	query.WriteString(" ORDER BY name")

	fmt.Fprintf(&query, " LIMIT $%d OFFSET $%d", len(args)+1, len(args)+2)
	args = append(args, page.Limit, page.Offset)

	rows, err := r.db(ctx).Query(ctx, query.String(), args...)
	if err != nil {
		return nil, fmt.Errorf("query failed: %w", err)
	}

	res, err := pgx.CollectRows(rows, pgx.RowToAddrOfStructByName[model.Exercise])
	if err != nil {
		return nil, fmt.Errorf("collect failed: %w", err)
	}

	return res, nil
}

func (r *ExerciseRepo) UpdateExercise(ctx context.Context, input *model.UpdateExerciseInput) (*model.Exercise, error) {
	query := `
		UPDATE exercises
		SET
			is_public = COALESCE($2, is_public),
			name = COALESCE($3, name),
			description = COALESCE($4, description),
			muscle_group = COALESCE($5, muscle_group),
			equipment = COALESCE($6, equipment)
		WHERE id = $1
		RETURNING ` + exerciseColumns

	rows, err := r.db(ctx).Query(ctx, query,
		input.ID,
		input.IsPublic,
		input.Name,
		input.Description,
		input.MuscleGroups,
		input.Equipments,
	)
	if err != nil {
		return nil, fmt.Errorf("update failed: %w", err)
	}

	res, err := pgx.CollectOneRow(rows, pgx.RowToAddrOfStructByName[model.Exercise])
	if errors.Is(err, pgx.ErrNoRows) {
		return nil, ErrNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("collect failed: %w", err)
	}

	return res, nil
}

func (r *ExerciseRepo) DeleteExercise(ctx context.Context, id uuid.UUID) error {
	conn := r.db(ctx)

	query := `
		UPDATE exercises
		SET deleted_at = NOW()
		WHERE id = $1
		AND deleted_at IS NULL
	`
	res, err := conn.Exec(ctx, query, id)
	if err != nil {
		return fmt.Errorf("delete failed: %w", err)
	}

	if res.RowsAffected() == 0 {
		return ErrNoAffectedRows
	}

	return nil
}

func (r *ExerciseRepo) IsUserOwnerCheck(ctx context.Context, userID uuid.UUID, exerciseID uuid.UUID) (bool, error) {
	conn := r.db(ctx)

	query := `
		SELECT EXISTS (
		    SELECT 1 FROM exercises
		             WHERE id = $1
		), 
		       EXISTS (
			SELECT 1 FROM exercises
	        WHERE id = $1
	        	AND owner_id = $2
		)
	`

	isExerciseExists, isOwner := false, false
	err := conn.QueryRow(ctx, query, exerciseID, userID).Scan(&isExerciseExists, &isOwner)
	if err != nil {
		return false, fmt.Errorf("select failed: %w", err)
	}

	if !isExerciseExists {
		return false, ErrNotFound
	}
	return isOwner, nil
}

func (r *ExerciseRepo) UpdateMediaID(ctx context.Context, id uuid.UUID, mediaID uuid.UUID) error {
	conn := r.db(ctx)
	query := `
			UPDATE exercises
			SET
				media_id = $1
			WHERE id = $2`
	res, err := conn.Exec(ctx, query, mediaID, id)
	if res.RowsAffected() == 0 {
		return ErrNoAffectedRows
	}
	if err != nil {
		return fmt.Errorf("update failed: %w", err)
	}

	return nil
}
