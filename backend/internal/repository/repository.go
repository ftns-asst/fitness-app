package repository

import (
	"context"
	"errors"
	"strings"

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
	ErrNotFound       = errors.New("not found repo error")
	ErrNoAffectedRows = errors.New("no affected rows repo error")
)

type Repositories struct {
	user           *UserRepo
	auth           *AuthRepo
	exercise       *ExerciseRepo
	file           *FileRepo
	program        *ProgramRepo
	personalRecord *PersonalRecordRepo
}

func NewRepositories(db db) *Repositories {
	return &Repositories{
		user:           NewUserRepo(db),
		auth:           NewAuthRepo(db),
		exercise:       NewExerciseRepo(db),
		file:           NewFileRepo(db),
		program:        NewProgramRepo(db),
		personalRecord: NewPersonalRecordRepo(db),
	}
}

func (r *Repositories) User() *UserRepo {
	return r.user
}

func (r *Repositories) Auth() *AuthRepo {
	return r.auth
}

func (r *Repositories) Exercise() *ExerciseRepo {
	return r.exercise
}

func (r *Repositories) File() *FileRepo {
	return r.file
}

func (r *Repositories) Program() *ProgramRepo {
	return r.program
}

func (r *Repositories) PersonalRecord() *PersonalRecordRepo {
	return r.personalRecord
}

var likeEscaper = strings.NewReplacer(`\`, `\\`, `%`, `\%`, `_`, `\_`)
