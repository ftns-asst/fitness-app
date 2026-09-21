package repository

import (
	"context"
	"database/sql"
	"errors"
	"fmt"
	"ftns-asst/internal/model"
	"time"

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
func (r *UserRepo) CreateOrUpdateUserProfile(ctx context.Context, userID uuid.UUID, input *model.UpdateUserProfileInput) (*model.UserProfile, error) {
	query := `
		INSERT INTO user_profiles(user_id, age, gender, height_cm, weight_kg)
		VALUES ($1, $2, $3, $4, $5)
		ON CONFLICT (user_id) 
		DO UPDATE SET 
			age = COALESCE(EXCLUDED.age, user_profiles.age),
			gender = COALESCE(EXCLUDED.gender, user_profiles.gender),
			height_cm = COALESCE(EXCLUDED.height_cm, user_profiles.height_cm),
			weight_kg = COALESCE(EXCLUDED.weight_kg, user_profiles.weight_kg)
		RETURNING *;`

	conn := r.db(ctx)

	rows, err := conn.Query(ctx, query, userID, input.Age, input.Gender, input.HeightCm, input.WeightKg)
	if errors.Is(err, sql.ErrNoRows) {
		return nil, ErrNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("insert failed: %w", err)
	}

	res, err := pgx.CollectExactlyOneRow(rows, pgx.RowToAddrOfStructByName[model.UserProfile])
	if err != nil {
		return nil, fmt.Errorf("collect row failed: %w", err)
	}
	return res, nil
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

// get user by email
func (r *UserRepo) GetUserByEmail(ctx context.Context, email string) (*model.User, error) {
	query := `
		SELECT * FROM users
		WHERE users.email = $1
	`
	conn := r.db(ctx)
	rows, err := conn.Query(ctx, query, email)
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

type userWithProfile struct {
	ID            uuid.UUID `db:"user_id"`
	Name          string    `db:"display_name"`
	Email         string    `db:"email"`
	PasswordHash  string    `db:"pass_hash"`
	CreatedAt     time.Time `db:"created_at"`
	UserUpdatedAt time.Time `db:"user_updated_at"`

	ProfileUserID    *uuid.UUID `db:"profile_user_id"`
	Age              *uint      `db:"age"`
	Gender           *string    `db:"gender"`
	HeightCm         *uint      `db:"height_cm"`
	WeightKg         *uint      `db:"weight_kg"`
	ProfileUpdatedAt *time.Time `db:"profile_updated_at"`
}

func (u *userWithProfile) ToModel() *model.UserWithProfile {
	var profile *model.UserProfile
	if u.ProfileUserID != nil {
		profile = &model.UserProfile{
			UserID:    *u.ProfileUserID,
			Age:       u.Age,
			Gender:    (*model.Gender)(u.Gender),
			HeightCm:  u.HeightCm,
			WeightKg:  u.WeightKg,
			UpdatedAt: u.ProfileUpdatedAt,
		}
	}
	return &model.UserWithProfile{
		User: model.User{
			ID:           u.ID,
			Name:         u.Name,
			Email:        u.Email,
			PasswordHash: u.PasswordHash,
			UpdatedAt:    u.UserUpdatedAt,
		},

		Profile: profile,
	}
}

func (r *UserRepo) GetUserWithProfileByID(ctx context.Context, id uuid.UUID) (*model.UserWithProfile, error) {
	query := `
		SELECT u.id AS user_id, 
			u.display_name AS display_name,
			u.email,
			u.pass_hash,
			u.created_at,
			u.updated_at AS user_updated_at,
        	p.user_id AS profile_user_id, 
			p.age,
			p.gender,
			p.height_cm,
			p.weight_kg,
			p.updated_at AS profile_updated_at
		FROM users u
		LEFT JOIN user_profiles p ON p.user_id = u.id
		WHERE u.id = $1
	`
	conn := r.db(ctx)
	rows, err := conn.Query(ctx, query, id)
	if errors.Is(err, sql.ErrNoRows) {
		return nil, ErrNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("select failed: %w", err)
	}

	res, err := pgx.CollectExactlyOneRow(rows, pgx.RowToAddrOfStructByName[userWithProfile])
	if err != nil {
		return nil, fmt.Errorf("collect row failed: %w", err)
	}
	return res.ToModel(), nil
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
