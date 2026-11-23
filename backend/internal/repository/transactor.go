package repository

import (
	"context"
	"errors"
	"fmt"
	"log"

	"ftns-asst/internal/model"

	"github.com/jackc/pgx/v5"
)

type Transactor struct {
	db DB
}

func NewTransactor(db DB) *Transactor {
	return &Transactor{db: db}
}

// WithinTx reuses the caller's transaction. Only the outermost call commits or rolls back.
func (t *Transactor) WithinTx(ctx context.Context, fn func(context.Context) error) error {
	if getTx(ctx) != nil {
		return fn(ctx)
	}
	tx, err := t.db.Begin(ctx)
	if err != nil {
		return fmt.Errorf("failed to begin transaction: %w", err)
	}
	defer func() {
		if err := tx.Rollback(ctx); err != nil && !errors.Is(err, pgx.ErrTxClosed) {
			log.Printf("transaction rollback failed: %v", err)
		}
	}()

	if err := fn(context.WithValue(ctx, model.ContextKeyTx, tx)); err != nil {
		return err
	}
	if err := tx.Commit(ctx); err != nil {
		return fmt.Errorf("commit transaction failed: %w", err)
	}
	return nil
}

func getTx(ctx context.Context) pgx.Tx {
	tx, _ := ctx.Value(model.ContextKeyTx).(pgx.Tx)
	return tx
}

func Conn(ctx context.Context, db DB) DB {
	if tx := getTx(ctx); tx != nil {
		return tx
	}
	return db
}
