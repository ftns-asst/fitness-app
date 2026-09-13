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

type UserRepo struct {
	database db
}

func NewUserRepo(db db) *UserRepo {
	return &UserRepo{
		database: db,
	}
}

// try get tx from ctx
func (r *UserRepo) db(ctx context.Context) db {
	res := r.database
	if tx := ctx.Value(model.ContextKeyTx); tx != nil {
		res = tx.(db)
	}
	return res
}

// create new user
func (r *UserRepo) CreateUser(ctx context.Context, user *model.User) (*model.User, error) {
	query := `
		INSERT INTO users(email, display_name, pass_hash)
		VALUES ($1, $2, $3)
		RETURNING id, created_at, updated_at;`

	conn := r.db(ctx)
	err := conn.QueryRow(ctx, query, user.Email, user.Name, user.PasswordHash).
		Scan(&user.ID, &user.CreatedAt, &user.UpdatedAt)
	if err != nil {
		return nil, fmt.Errorf("insert scan failed: %w", err)
	}
	return user, nil
}

// create or update user profile with coalesce for partially updating
func (r *UserRepo) CreateOrUpdateUserProfile(ctx context.Context, profile *model.UserProfile) (*model.UserProfile, error) {
	query := `
		INSERT INTO user_profiles(age, gender, height_cm, weight_kd)
		VALUES ($1, $2, $3, $4)
		RETURNING updated_at
		ON CONFLICT (user_id) 
		DO UPDATE SET 
			age = COALESCE(EXCLUDED.age),
			gender = COALESCE(EXCLUDED.gender),
			height_cm = COALESCE(EXCLUDED.height_cm)
			weight_kg = COALESCE(EXCLUDED.weight_kd);`

	conn := r.db(ctx)
	err := conn.QueryRow(ctx, query, profile.Age, profile.Gender, profile.HeightCm, profile.WeightKg).
		Scan(&profile.UpdatedAt)
	if err != nil {
		return nil, fmt.Errorf("insert scan failed: %w", err)
	}
	return profile, nil
}

// get user by id
func (r *UserRepo) GetUserByID(ctx context.Context, id uuid.UUID) (*model.User, error) {
	query := `
		SELECT * FROM users
		WHERE users.id = $1
	`
	conn := r.db(ctx)
	rows, err := conn.Query(ctx, query, id)
	if errors.Is(err, sql.ErrNoRows) {
		return nil, ErrNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("select failed: %w", err)
	}

	res, err := pgx.CollectExactlyOneRow(rows, pgx.RowToAddrOfStructByName[model.User])
	if err != nil {
		return nil, fmt.Errorf("collect row failed: %w", err)
	}
	return res, nil
}

// get list of users by IDs
func (r *UserRepo) GetUserListByIDs(ctx context.Context, ids []uuid.UUID) ([]*model.User, error) {
	if len(ids) == 0 {
		return []*model.User{}, nil
	}
	query := `
		SELECT * FROM users
		WHERE users.id = ANY($1)
		`
	conn := r.db(ctx)
	rows, err := conn.Query(ctx, query, ids)
	if errors.Is(err, sql.ErrNoRows) {
		return nil, ErrNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("select failed: %w", err)
	}

	res, err := pgx.CollectRows(rows, pgx.RowToAddrOfStructByName[model.User])
	if err != nil {
		return nil, fmt.Errorf("collect row failed: %w", err)
	}
	return res, nil
}

type userUserProfile struct {
	model.User
	model.UserProfile
}

func (r *UserRepo) GetUserWithProfileByID(ctx context.Context, id uuid.UUID) (*model.UserWithProfile, error) {
	query := `
		SELECT * FROM users
		LEFT JOIN user_profiles ON user_profiles.user_id = users.id
		WHERE users.id = $1
	`
	conn := r.db(ctx)
	rows, err := conn.Query(ctx, query, id)
	if errors.Is(err, sql.ErrNoRows) {
		return nil, ErrNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("select failed: %w", err)
	}

	res, err := pgx.CollectExactlyOneRow(rows, pgx.RowToAddrOfStructByName[userUserProfile])
	if err != nil {
		return nil, fmt.Errorf("collect row failed: %w", err)
	}
	return &model.UserWithProfile{
		User:    res.User,
		Profile: &res.UserProfile,
	}, nil
}

func (r *UserRepo) CheckUserWithEmailExists(ctx context.Context, email string) (exists bool, err error) {
	conn := r.db(ctx)

	query := `
		SELECT EXISTS(
			SELECT 1 FROM users
			WHERE users.email = $1	
		)
	`
	err = conn.QueryRow(ctx, query, email).Scan(&exists)
	if err != nil {
		return false, fmt.Errorf("select scan failed: %w", err)
	}

	return exists, nil
}
