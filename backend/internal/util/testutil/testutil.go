package testutil

import (
	"context"
	"ftns-asst/internal/model"
	"sync/atomic"
	"testing"

	"github.com/go-openapi/testify/v2/require"
)

type MockTransactor struct{}

func (m *MockTransactor) WithinTx(ctx context.Context, fn func(context.Context) error) error {
	return fn(ctx)
}

func EqualSvcErrKey(t *testing.T, err error, targetKey string) {
	serr := new(model.ServiceError)
	require.ErrorAs(t, err, &serr)
	require.Equal(t, serr.Key, targetKey)
}

type MockMailer struct {
	sentCount atomic.Int64
}

func (m *MockMailer) SendEmail(toEmail string, subjectRaw string, body string) error {
	m.sentCount.Add(1)
	return nil
}

func (m *MockMailer) Count() int {
	return int(m.sentCount.Load())
}
