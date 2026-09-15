package repository

import (
	"context"
	"errors"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgconn"
)

type db interface {
	Exec(ctx context.Context, sql string, args ...any) (pgconn.CommandTag, error)
	Query(ctx context.Context, sql string, args ...any) (pgx.Rows, error)
	QueryRow(ctx context.Context, sql string, args ...any) pgx.Row
	CopyFrom(ctx context.Context, tableName pgx.Identifier, columnNames []string, rowSrc pgx.CopyFromSource) (int64, error)
	SendBatch(ctx context.Context, b *pgx.Batch) pgx.BatchResults
	// begin real or pseudo transaction
	Begin(ctx context.Context) (pgx.Tx, error)
}

var (
	ErrNotFound       = errors.New("not found repo eror")
	ErrNoAffectedRows = errors.New("no affected rows repo error")
)

type Repositories struct {
	user *UserRepo
	auth *AuthRepo
}

func NewRepositories(db db) *Repositories {
	return &Repositories{
		user: NewUserRepo(db),
		auth: NewAuthRepo(db),
	}
}

func (r *Repositories) User() *UserRepo {
	return r.user
}

func (r *Repositories) Auth() *AuthRepo {
	return r.auth
}
