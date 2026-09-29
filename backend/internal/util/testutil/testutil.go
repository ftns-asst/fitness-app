package testutil

import (
	"context"
	"ftns-asst/internal/model"
	"testing"

	"github.com/go-openapi/testify/v2/require"
	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgconn"
)

type MockTransactor struct{}

func (m *MockTransactor) Begin(context.Context) (pgx.Tx, error) {
	return &MockTx{}, nil
}

type MockTx struct {
	pgx.Tx
}

var _ pgx.Tx = (*MockTx)(nil)

func (m *MockTx) Commit(context.Context) error   { return nil }
func (m *MockTx) Rollback(context.Context) error { return nil }
func (m *MockTx) Exec(context.Context, string, ...any) (pgconn.CommandTag, error) {
	return pgconn.CommandTag{}, nil
}
func (m *MockTx) Query(context.Context, string, ...any) (pgx.Rows, error) { return nil, nil }
func (m *MockTx) QueryRow(context.Context, string, ...any) pgx.Row        { return nil }
func (m *MockTx) CopyFrom(context.Context, pgx.Identifier, []string, pgx.CopyFromSource) (int64, error) {
	return 0, nil
}
func (m *MockTx) SendBatch(context.Context, *pgx.Batch) pgx.BatchResults { return nil }

func EqualSvcErrKey(t *testing.T, err error, targetKey string) {
	serr := new(model.ServiceError)
	require.ErrorAs(t, err, &serr)
	require.Equal(t, serr.Key, targetKey)
}
