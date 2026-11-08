package repository

import (
	"context"
	"errors"
	"fmt"
	"ftns-asst/internal/model"

	"github.com/google/uuid"
	"github.com/jackc/pgx/v5"
)

type FileRepo struct {
	database db
}

func NewFileRepo(db db) *FileRepo {
	return &FileRepo{
		database: db,
	}
}

// try get tx from ctx
func (r *FileRepo) db(ctx context.Context) db {
	res := r.database
	if tx := ctx.Value(model.ContextKeyTx); tx != nil {
		res = tx.(db)
	}
	return res
}

func (r *FileRepo) CreateMedia(ctx context.Context, media *model.Media) (*model.Media, error) {
	query := `
		INSERT INTO media (type, object_key)
		VALUES ($1, $2)
		RETURNING id
	`
	err := r.db(ctx).QueryRow(ctx, query, media.Type, media.ObjectKey).Scan(&media.ID)
	if err != nil {
		return nil, fmt.Errorf("insert failed: %w", err)
	}
	return media, nil
}

func (r *FileRepo) GetMediaByID(ctx context.Context, id uuid.UUID) (*model.Media, error) {
	rows, err := r.db(ctx).Query(ctx, `SELECT id, type, object_key FROM media WHERE id = $1`, id)
	if err != nil {
		return nil, fmt.Errorf("query failed: %w", err)
	}

	res, err := pgx.CollectOneRow(rows, pgx.RowToAddrOfStructByName[model.Media])
	if errors.Is(err, pgx.ErrNoRows) {
		return nil, ErrNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("collect failed: %w", err)
	}
	return res, nil
}

func (r *FileRepo) GetMediaByIDs(ctx context.Context, ids []uuid.UUID) ([]*model.Media, error) {
	rows, err := r.db(ctx).Query(ctx, `SELECT id, type, object_key FROM media WHERE id = ANY($1)`, ids)
	if err != nil {
		return nil, fmt.Errorf("query failed: %w", err)
	}

	res, err := pgx.CollectRows(rows, pgx.RowToAddrOfStructByName[model.Media])
	if err != nil {
		return nil, fmt.Errorf("collect failed: %w", err)
	}
	return res, nil
}

// delete media record and return it, exercises referencing it lose media_id (ON DELETE SET NULL)
func (r *FileRepo) DeleteMedia(ctx context.Context, id uuid.UUID) (*model.Media, error) {
	rows, err := r.db(ctx).Query(ctx, `DELETE FROM media WHERE id = $1 RETURNING id, type, object_key`, id)
	if err != nil {
		return nil, fmt.Errorf("delete failed: %w", err)
	}

	res, err := pgx.CollectOneRow(rows, pgx.RowToAddrOfStructByName[model.Media])
	if errors.Is(err, pgx.ErrNoRows) {
		return nil, ErrNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("collect failed: %w", err)
	}
	return res, nil
}
