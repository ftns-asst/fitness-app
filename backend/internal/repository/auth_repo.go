package repository

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"ftns-asst/internal/model"

	"github.com/google/uuid"
	"github.com/jackc/pgx/v5"
)

type AuthRepo struct {
	database db
}

func NewAuthRepo(db db) *AuthRepo {
	return &AuthRepo{
		database: db,
	}
}

// try get tx from ctx
func (r *AuthRepo) db(ctx context.Context) db {
	res := r.database
	if tx := ctx.Value(model.ContextKeyTx); tx != nil {
		res = tx.(db)
	}
	return res
}

// create token
func (r *AuthRepo) CreateToken(ctx context.Context, token *model.RefreshToken) (*model.RefreshToken, error) {
	query := `
		INSERT INTO tokens(user_id, token_hash, expires_at)
		VALUES ($1, $2, $3)
		RETURNING tokens.id`

	conn := r.db(ctx)
	err := conn.QueryRow(ctx, query, token.UserID, token.TokenHash, token.ExpiresAt).Scan(&token.ID)
	if err != nil {
		return nil, fmt.Errorf("insert scan failed: %w", err)
	}

	return token, nil
}

func (r *AuthRepo) GetNotUsedTokenByUserIDAndHash(ctx context.Context, userID uuid.UUID, hash string) (*model.RefreshToken, error) {
	query := `
		SELECT * FROM tokens
		WHERE tokens.user_id = $1
			AND tokens.token_hash = $2
			AND tokens.used_at IS NULL
		`

	conn := r.db(ctx)
	rows, err := conn.Query(ctx, query, userID, hash)
	if err != nil {
		return nil, fmt.Errorf("select failed: %w", err)
	}
	defer rows.Close()

	token, err := pgx.CollectExactlyOneRow(rows, pgx.RowToAddrOfStructByName[model.RefreshToken])
	if errors.Is(err, sql.ErrNoRows) {
		return nil, ErrNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("failed to collect row: %w", err)
	}
	return token, nil
}

func (r *AuthRepo) SetTokenUsedByID(ctx context.Context, id uuid.UUID) error {
	query := `
		UPDATE tokens
		SET used_at = now()
		WHERE tokens.id = $1 
			AND tokens.used_at IS NULL
		VALUES ($1, $2, $3)`

	conn := r.db(ctx)
	res, err := conn.Exec(ctx, query, id)
	if err != nil {
		return fmt.Errorf("update failed: %w", err)
	}

	if res.RowsAffected() == 0 {
		return ErrNoAffectedRows
	}

	return nil
}
