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

type Mailer interface {
	SendEmail(toEmail string, subjectRaw string, body string) error
}

func CheckRollback(ctx context.Context, tx pgx.Tx) {
	err := tx.Rollback(ctx)
	if err != nil && !errors.Is(err, pgx.ErrTxClosed) {
		log.Printf("rollback failed: %s", err.Error())
	}
}

// shortcut for map[T]struct{}
type Set[T comparable] map[T]struct{}

func NewSet[T comparable](items ...T) Set[T] {
	s := make(map[T]struct{}, len(items))
	for _, item := range items {
		s[item] = struct{}{}
	}
	return s
}

type PageInfo struct {
	Limit  *int
	Offset *int
}

const (
	DefaultPageLimit  = 20
	DefaultPageOffset = 0
	MaxPageLimit      = 100
)

func (p *PageInfo) Normalize() *PageInfo {
	if p.Limit == nil || *p.Limit <= 0 || *p.Limit > MaxPageLimit {
		*p.Limit = DefaultPageLimit
	}

	if p.Offset == nil || *p.Offset < 0 {
		*p.Offset = DefaultPageOffset
	}

	return p
}
