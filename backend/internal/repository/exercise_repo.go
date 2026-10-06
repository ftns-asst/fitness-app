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
			muscle_groups,
			equipments
		)
		VALUES ($1, $2, $3, $4, $5, $6)
		RETURNING id
	`

	err := conn.QueryRow(ctx, query,
		exercise.OwnerID,
		exercise.IsPublic,
		exercise.Name,
		exercise.Description,
		exercise.MuscleGroups,
		exercise.Equipments,
		exercise.CreatedAt).
		Scan(&exercise.ID)
	if err != nil {
		return nil, fmt.Errorf("insert failed: %w", err)
	}

	return exercise, nil
}

func (r *ExerciseRepo) FillExercises(ctx context.Context, exercises []*model.Exercise) error {
	conn := r.db(ctx)

	count, err := conn.CopyFrom(ctx, pgx.Identifier{"exercises"},
		[]string{"owner_id",
			"is_public",
			"name",
			"description",
			"muscle_groups",
			"equipments",
			"created_at"},
		pgx.CopyFromSlice(len(exercises), func(i int) ([]interface{}, error) {
			ex := exercises[i]
			return []interface{}{
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

	return nil
}

func (r *ExerciseRepo) GetExerciseByID(ctx context.Context, id uuid.UUID) (*model.Exercise, error) {
	conn := r.db(ctx)

	query := `
		SELECT *
		FROM exercises
		WHERE id = $1
	`

	_, err := conn.Query(ctx, query, id)
	if errors.Is(err, pgx.ErrNoRows) {
		return nil, ErrNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("query failed: %w", err)
	}

	rows, err := conn.Query(ctx, query, id)
	if errors.Is(err, pgx.ErrNoRows) {
		return nil, ErrNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("query failed: %w", err)
	}

	defer rows.Close()

	res, err := pgx.CollectOneRow(rows, pgx.RowToAddrOfStructByName[model.Exercise])
	if err != nil {
		return nil, fmt.Errorf("collect failed: %w", err)
	}
	return res, nil
}

func (r *ExerciseRepo) GetExercises(ctx context.Context, filters *model.ExerciseFilters, page *model.PageInfo) ([]*model.Exercise, error) {
	args := []interface{}{}
	query := strings.Builder{}
	query.WriteString(`
		SELECT *
		FROM exercises
		WHERE deleted_at IS NULL
	`)

	// not public exercises available only for own list type
	if filters.ListType != model.ExerciseListTypeOwn {
		query.WriteString(" AND is_public = TRUE")
	}

	switch filters.ListType {
	case model.ExerciseListTypeOwn, model.ExerciseListTypePublicUser:
		query.WriteString(" AND owner_id = $1")
		args = append(args, filters.UserID)
	case model.ExerciseListTypeAll:
		query.WriteString(" AND (is_public = TRUE OR owner_id = $1)")
		args = append(args, filters.UserID)
	case model.ExerciseListTypePublic:
		query.WriteString(" AND is_public = TRUE AND owner_id IS NOT NULL AND owner_id != $1")
		args = append(args, filters.UserID)
	case model.ExerciseListTypeBasic:
		query.WriteString(" AND owner_id IS NULL AND is_public = TRUE")
	}

	// muscle groups filter
	if len(filters.MuscleGroups) > 0 {
		query.WriteString(" AND muscle_groups && $2") // any intersection
		args = append(args, filters.MuscleGroups)
	}

	// equipments filter
	if len(filters.Equipments) > 0 {
		fmt.Fprintf(&query, " AND equipments <@ $%d", len(args)+1)
		args = append(args, filters.Equipments)
	}

	if filters.Search != nil {
		fmt.Fprint(&query, " AND name ILIKE '%' || "+fmt.Sprintf("$%d", len(args)+1)+" || '%' ESCAPE '\\'")
		args = append(args, likeEscaper.Replace(*filters.Search))
	}

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
	conn := r.db(ctx)

	query := `
		UPDATE exercises
		SET
			is_public = COALESCE($2, is_public),
			name = COALESCE($3, name),
			description = COALESCE($4, description),
			muscle_groups = COALESCE($5, muscle_groups),
			equipments = COALESCE($6, equipments)
		WHERE id = $1
		RETURNING *
	`
	muscleGroups := input.MuscleGroups
	if muscleGroups == nil {
		muscleGroups = []model.MuscleGroup{}
	}
	equipments := input.Equipments
	if equipments == nil {
		equipments = []model.Equipment{}
	}

	rows, err := conn.Query(ctx, query,
		input.ID,
		input.IsPublic,
		input.Name,
		input.Description,
		muscleGroups,
		equipments,
	)
	if err != nil {
		return nil, fmt.Errorf("update failed: %w", err)
	}

	defer rows.Close()

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
	`
	res, err := conn.Exec(ctx, query, id)
	if err != nil {
		return fmt.Errorf("delete failed: %w", err)
	}

	if res.RowsAffected() == 0 {
		return ErrNotFound
	}

	return nil
}

func (r *ExerciseRepo) IsUserOwnerCheck(ctx context.Context, userID uuid.UUID, exerciseID uuid.UUID) (bool, error) {
	conn := r.db(ctx)

	query := `
		SELECT EXISTS (
			SELECT 1 FROM exercises
	        WHERE id = $1
	        	AND owner_id = $2
		)
	`

	isOwner := false
	err := conn.QueryRow(ctx, query, exerciseID, userID).Scan(&isOwner)
	if err != nil {
		return false, fmt.Errorf("select failed: %w", err)
	}

	return isOwner, nil
}
