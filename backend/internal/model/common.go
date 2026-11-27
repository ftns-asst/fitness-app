package model

import (
	"context"
	"ftns-asst/internal/util"
)

type ContextKey string

const (
	ContextKeyTx     ContextKey = "tx"
	ContextKeyUserID ContextKey = "userID"
)

type Transactor interface {
	WithinTx(ctx context.Context, fn func(context.Context) error) error
}

type Mailer interface {
	SendEmail(toEmail string, subjectRaw string, body string) error
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
	MaxPageLimit      = 1000
)

func (p *PageInfo) Normalize() *PageInfo {
	if p.Limit == nil || *p.Limit <= 0 || *p.Limit > MaxPageLimit {
		p.Limit = util.Ptr(DefaultPageLimit)
	}

	if p.Offset == nil || *p.Offset < 0 {
		p.Offset = util.Ptr(DefaultPageOffset)
	}

	return p
}
