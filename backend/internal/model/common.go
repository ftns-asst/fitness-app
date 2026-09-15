package model

import (
	"context"
	"errors"
	"log"

	"github.com/jackc/pgx/v5"
)

type ContextKey string

const (
	ContextKeyTx     ContextKey = "tx"
	ContextKeyUserID ContextKey = "userID"
)

type Transactor interface {
	Begin(ctx context.Context) (pgx.Tx, error)
}

func CheckRollback(ctx context.Context, tx pgx.Tx) {
	err := tx.Rollback(ctx)
	if err != nil && !errors.Is(err, pgx.ErrTxClosed) {
		log.Printf("rollback failed: %s", err.Error())
	}
}
