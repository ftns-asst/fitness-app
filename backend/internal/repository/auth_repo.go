package repository

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"ftns-asst/internal/model"
	"ftns-asst/internal/util"

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
			AND tokens.used_at IS NULL`

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

// save new verification code, set other codes as expired
func (r *AuthRepo) CreateVerificationCodeAndSetOtherExpired(ctx context.Context, code *model.VerificationCode) (*model.VerificationCode, error) {
	query := `
		WITH expired AS (
			UPDATE verification_codes
			SET expires_at = now() - interval '1 minute'
			WHERE user_id = $1
				AND used_at IS NULL
				AND expires_at > now()
		)
		INSERT INTO verification_codes (user_id, code_hash, expires_at)
		VALUES ($1, $2, $3)
		RETURNING id`

	hash, err := util.HashSHA256(code.Code, "dfjdksldfjsdf")
	if err != nil {
		return nil, fmt.Errorf("failed to hash code: %w", err)
	}
	conn := r.db(ctx)
	err = conn.QueryRow(ctx, query, code.UserID, hash, code.ExpiresAt).Scan(&code.ID)
	if err != nil {
		return nil, fmt.Errorf("insert scan failed: %w", err)
	}

	return code, nil
}

// set code used
func (r *AuthRepo) SetCodeUsedByID(ctx context.Context, id uuid.UUID) error {
	query := `
		UPDATE verification_codes vc
		SET used_at = now()
		WHERE vc.id = $1
			AND vc.used_at IS NULL
			AND vc.expires_at > now()`

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

// get code that not used or expired (there should be only ONE such)
func (r *AuthRepo) GetCodeNotUsedNotExpiredByUserID(ctx context.Context, userID uuid.UUID) (*model.VerificationCode, error) {
	query := `
		SELECT * FROM verification_codes vc
		WHERE vc.user_id = $1
			AND vc.used_at IS NULL,
			AND vc.expires_at > now();
		`

	conn := r.db(ctx)
	rows, err := conn.Query(ctx, query, userID)
	if err != nil {
		return nil, fmt.Errorf("select failed: %w", err)
	}
	defer rows.Close()

	code, err := pgx.CollectExactlyOneRow(rows, pgx.RowToAddrOfStructByName[model.VerificationCode])
	if errors.Is(err, sql.ErrNoRows) {
		return nil, ErrNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("failed to collect row: %w", err)
	}
	return code, nil
}

// create reset token
func (r *AuthRepo) CreateResetToken(ctx context.Context, token *model.ResetToken) (*model.ResetToken, error) {
	query := `
		INSERT INTO reset_tokens(user_id, token_hash, expires_at)
		VALUES ($1, $2, $3)
		RETURNING reset_tokens.id`

	conn := r.db(ctx)
	err := conn.QueryRow(ctx, query, token.UserID, token.TokenHash, token.ExpiresAt).Scan(&token.ID)
	if err != nil {
		return nil, fmt.Errorf("insert scan failed: %w", err)
	}

	return token, nil
}

func (r *AuthRepo) GetNotUsedResetTokenByUserIDAndHash(ctx context.Context, userID uuid.UUID, hash string) (*model.ResetToken, error) {
	query := `
		SELECT * FROM reset_tokens
		WHERE reset_tokens.user_id = $1
			AND reset_tokens.token_hash = $2
			AND reset_tokens.used_at IS NULL
		`

	conn := r.db(ctx)
	rows, err := conn.Query(ctx, query, userID, hash)
	if err != nil {
		return nil, fmt.Errorf("select failed: %w", err)
	}
	defer rows.Close()

	token, err := pgx.CollectExactlyOneRow(rows, pgx.RowToAddrOfStructByName[model.ResetToken])
	if errors.Is(err, sql.ErrNoRows) {
		return nil, ErrNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("failed to collect row: %w", err)
	}
	return token, nil
}

func (r *AuthRepo) SetResetTokenUsedByID(ctx context.Context, id uuid.UUID) error {
	query := `
		UPDATE reset_tokens
		SET used_at = now()
		WHERE reset_tokens.id = $1 
			AND reset_tokens.used_at IS NULL`

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
