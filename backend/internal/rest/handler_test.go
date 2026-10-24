package rest

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"ftns-asst/internal/config"
	"ftns-asst/internal/model"
	"ftns-asst/internal/util/jwtutil"
	"io"
	"net/http"
	"net/http/httptest"
	"os"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/golang-jwt/jwt/v5"
	"github.com/google/uuid"
	"github.com/stretchr/testify/require"
)

const testSecret = "access-secret"

type deps struct {
	auth *MockAuthService
	user *MockUserService
}

func TestMain(m *testing.M) {
	gin.SetMode(gin.TestMode)
	os.Exit(m.Run())
}

func newDeps(t *testing.T) *deps {
	return &deps{
		auth: NewMockAuthService(t),
		user: NewMockUserService(t),
	}
}

func newRouter(d *deps) *gin.Engine {
	r := gin.New()
	h := &Handlers{
		cfg:  &config.Config{AccessTokenJWTSecretKey: testSecret},
		auth: NewAuthHandler(d.auth, d.user),
		user: NewUserHandler(d.user),
	}
	h.RegisterRoutes(r.Group("/api/v1"))
	return r
}

// body can be a string with raw json or a value to marshal
func doRequest(t *testing.T, r http.Handler, method, path string, body any, token string) *httptest.ResponseRecorder {
	t.Helper()
	var reader io.Reader
	switch b := body.(type) {
	case nil:
	case string:
		reader = bytes.NewBufferString(b)
	default:
		data, err := json.Marshal(b)
		require.NoError(t, err)
		reader = bytes.NewBuffer(data)
	}

	req := httptest.NewRequest(method, path, reader)
	req.Header.Set("Content-Type", "application/json")
	if token != "" {
		req.Header.Set("Authorization", "Bearer "+token)
	}

	w := httptest.NewRecorder()
	r.ServeHTTP(w, req)
	return w
}

func decodeBody[T any](t *testing.T, w *httptest.ResponseRecorder) T {
	t.Helper()
	var res T
	require.NoError(t, json.Unmarshal(w.Body.Bytes(), &res), w.Body.String())
	return res
}

func requireError(t *testing.T, w *httptest.ResponseRecorder, code int, key string) {
	t.Helper()
	require.Equal(t, code, w.Code, w.Body.String())
	resp := decodeBody[ErrorResponse](t, w)
	require.Equal(t, key, resp.Key)
	if code == http.StatusInternalServerError {
		require.Equal(t, "internal server error", resp.Message)
	} else {
		require.NotEmpty(t, resp.Message)
	}
}

func genAccessToken(t *testing.T, userID uuid.UUID, secret string) string {
	t.Helper()
	info, err := jwtutil.GenAccessToken(userID, secret)
	require.NoError(t, err)
	return info.Token
}

func genExpiredToken(t *testing.T, userID uuid.UUID) string {
	t.Helper()
	token := jwt.NewWithClaims(jwt.SigningMethodHS256, jwtutil.UserJWTClaims{
		UserID: userID,
		RegisteredClaims: jwt.RegisteredClaims{
			ExpiresAt: jwt.NewNumericDate(time.Now().Add(-time.Hour)),
		},
	})
	res, err := token.SignedString([]byte(testSecret))
	require.NoError(t, err)
	return res
}

func TestHandleError(t *testing.T) {
	t.Parallel()
	tests := []struct {
		Name         string
		Err          error
		CodeExpected int
		KeyExpected  string
	}{
		{
			Name:         "plain error",
			Err:          errors.New("some error"),
			CodeExpected: http.StatusInternalServerError,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:         "wrapped bad request",
			Err:          fmt.Errorf("invalid input: %w", model.ErrBadRequest),
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:         "not found",
			Err:          fmt.Errorf("user: %w", model.ErrNotFound),
			CodeExpected: http.StatusNotFound,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:         "service unavailable",
			Err:          model.ErrServiceUnavailable,
			CodeExpected: http.StatusServiceUnavailable,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:         "service error with key",
			Err:          model.AuthErrorIncorrectPassword(),
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.AuthErrorIncorrectPasswordKey,
		},
		{
			Name:         "wrapped service error",
			Err:          fmt.Errorf("failed to refresh: %w", model.AuthErrorTokenExpired()),
			CodeExpected: http.StatusUnauthorized,
			KeyExpected:  model.AuthErrorTokenExpiredKey,
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			t.Parallel()
			w := httptest.NewRecorder()
			c, _ := gin.CreateTestContext(w)

			handleError(c, test.Err)

			requireError(t, w, test.CodeExpected, test.KeyExpected)
			if test.CodeExpected != http.StatusInternalServerError {
				require.Equal(t, test.Err.Error(), decodeBody[ErrorResponse](t, w).Message)
			}
		})
	}
}

func TestAuthMiddleware(t *testing.T) {
	t.Parallel()
	userID := uuid.New()
	tests := []struct {
		Name         string
		Header       string
		CodeExpected int
		KeyExpected  string
	}{
		{
			Name:         "no header",
			Header:       "",
			CodeExpected: http.StatusUnauthorized,
			KeyExpected:  model.AuthErrorInvalidTokenKey,
		},
		{
			Name:         "garbage token",
			Header:       "Bearer not-a-jwt",
			CodeExpected: http.StatusUnauthorized,
			KeyExpected:  model.AuthErrorInvalidTokenKey,
		},
		{
			Name:         "wrong secret",
			Header:       "Bearer " + genAccessToken(t, userID, "other-secret"),
			CodeExpected: http.StatusUnauthorized,
			KeyExpected:  model.AuthErrorInvalidTokenKey,
		},
		{
			Name:         "expired token",
			Header:       "Bearer " + genExpiredToken(t, userID),
			CodeExpected: http.StatusUnauthorized,
			KeyExpected:  model.AuthErrorTokenExpiredKey,
		},
		{
			Name:         "valid token",
			Header:       "Bearer " + genAccessToken(t, userID, testSecret),
			CodeExpected: http.StatusOK,
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			t.Parallel()
			r := gin.New()
			var ctxUserID uuid.UUID
			r.GET("/", AuthMiddleware(testSecret), func(c *gin.Context) {
				id, err := tryGetUserIDFromCtx(c.Request.Context())
				require.NoError(t, err)
				ctxUserID = id
				c.Status(http.StatusOK)
			})

			req := httptest.NewRequest(http.MethodGet, "/", nil)
			if test.Header != "" {
				req.Header.Set("Authorization", test.Header)
			}
			w := httptest.NewRecorder()
			r.ServeHTTP(w, req)

			if test.CodeExpected != http.StatusOK {
				requireError(t, w, test.CodeExpected, test.KeyExpected)
				return
			}
			require.Equal(t, http.StatusOK, w.Code)
			require.Equal(t, userID, ctxUserID)
		})
	}
}
