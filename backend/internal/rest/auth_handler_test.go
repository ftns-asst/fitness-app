package rest

import (
	"errors"
	"fmt"
	"ftns-asst/internal/model"
	"ftns-asst/internal/rest/dto"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/google/uuid"
	"github.com/stretchr/testify/mock"
	"github.com/stretchr/testify/require"
)

type authTestData struct {
	user     *model.User
	password string
	tokens   *model.Tokens
}

func newAuthTestData() authTestData {
	return authTestData{
		user: &model.User{
			ID:           uuid.New(),
			Name:         "John",
			Email:        "test@example.com",
			PasswordHash: "hash",
			CreatedAt:    time.Now().Add(-time.Hour).UTC().Truncate(time.Second),
		},
		password: "somePass98",
		tokens: &model.Tokens{
			AccessToken:  "access",
			RefreshToken: "refresh",
		},
	}
}

func requireAuthResponse(t *testing.T, w *httptest.ResponseRecorder, td authTestData) {
	t.Helper()
	require.Equal(t, http.StatusOK, w.Code, w.Body.String())
	resp := decodeBody[dto.AuthResponseSuccess](t, w)
	require.Equal(t, &dto.UserResponse{
		ID:        td.user.ID,
		Name:      td.user.Name,
		Email:     td.user.Email,
		CreatedAt: td.user.CreatedAt,
	}, resp.User)
	require.Equal(t, &dto.Tokens{
		AccessToken:  td.tokens.AccessToken,
		RefreshToken: td.tokens.RefreshToken,
	}, resp.Tokens)
	require.NotContains(t, w.Body.String(), td.user.PasswordHash)
}

type handlerTest struct {
	Name         string
	Body         any
	Setup        func(*deps)
	CodeExpected int
	KeyExpected  string
	Check        func(*testing.T, *httptest.ResponseRecorder)
}

func runHandlerTests(t *testing.T, method, path string, tests []handlerTest) {
	t.Helper()
	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			t.Parallel()
			d := newDeps(t)
			if test.Setup != nil {
				test.Setup(d)
			}

			w := doRequest(t, newRouter(d), method, path, test.Body, "")

			if test.CodeExpected != http.StatusOK {
				requireError(t, w, test.CodeExpected, test.KeyExpected)
				return
			}
			require.Equal(t, http.StatusOK, w.Code, w.Body.String())
			if test.Check != nil {
				test.Check(t, w)
			}
		})
	}
}

func TestCheckEmail(t *testing.T) {
	t.Parallel()
	email := "test@example.com"
	runHandlerTests(t, http.MethodPost, "/api/v1/auth/check-email", []handlerTest{
		{
			Name:         "invalid json",
			Body:         `{"email":`,
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:         "invalid email",
			Body:         dto.CheckEmailRequestBody{Email: "not-email"},
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name: "email taken",
			Body: dto.CheckEmailRequestBody{Email: email},
			Setup: func(d *deps) {
				d.user.EXPECT().CheckUserWithEmailExists(mock.Anything, email).Return(true, nil)
			},
			CodeExpected: http.StatusOK,
			Check: func(t *testing.T, w *httptest.ResponseRecorder) {
				require.False(t, decodeBody[dto.CheckEmailResponse](t, w).Free)
			},
		},
		{
			Name: "email free",
			Body: dto.CheckEmailRequestBody{Email: email},
			Setup: func(d *deps) {
				d.user.EXPECT().CheckUserWithEmailExists(mock.Anything, email).Return(false, nil)
			},
			CodeExpected: http.StatusOK,
			Check: func(t *testing.T, w *httptest.ResponseRecorder) {
				require.True(t, decodeBody[dto.CheckEmailResponse](t, w).Free)
			},
		},
		{
			Name: "service error",
			Body: dto.CheckEmailRequestBody{Email: email},
			Setup: func(d *deps) {
				d.user.EXPECT().CheckUserWithEmailExists(mock.Anything, email).
					Return(false, errors.New("db is down"))
			},
			CodeExpected: http.StatusInternalServerError,
			KeyExpected:  model.DefaultErrorKey,
		},
	})
}

func TestSignUpHandler(t *testing.T) {
	t.Parallel()
	td := newAuthTestData()
	body := dto.SignUpRequestBody{
		Name:     td.user.Name,
		Email:    td.user.Email,
		Password: td.password,
	}
	input := &model.SignUpInput{
		AuthInput: model.AuthInput{Email: td.user.Email, Password: td.password},
		Name:      td.user.Name,
	}

	runHandlerTests(t, http.MethodPost, "/api/v1/auth/signup", []handlerTest{
		{
			Name:         "missing name",
			Body:         dto.SignUpRequestBody{Email: td.user.Email, Password: td.password},
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:         "short password",
			Body:         dto.SignUpRequestBody{Name: td.user.Name, Email: td.user.Email, Password: "short"},
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name: "email already used",
			Body: body,
			Setup: func(d *deps) {
				d.auth.EXPECT().SignUp(mock.Anything, input).Return(nil, model.AuthErrorEmailAlreadyUsed())
			},
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.AuthErrorEmailAlreadyUsedKey,
		},
		{
			Name: "service validation error",
			Body: body,
			Setup: func(d *deps) {
				d.auth.EXPECT().SignUp(mock.Anything, input).
					Return(nil, fmt.Errorf("password contains invalid character: %w", model.ErrBadRequest))
			},
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name: "success",
			Body: body,
			Setup: func(d *deps) {
				d.auth.EXPECT().SignUp(mock.Anything, input).
					Return(&model.SuccessSignUpResult{User: td.user, Tokens: td.tokens}, nil)
			},
			CodeExpected: http.StatusOK,
			Check: func(t *testing.T, w *httptest.ResponseRecorder) {
				requireAuthResponse(t, w, td)
			},
		},
	})
}

func TestLogInHandler(t *testing.T) {
	t.Parallel()
	td := newAuthTestData()
	body := dto.LogInRequestBody{Email: td.user.Email, Password: td.password}
	input := &model.LogInInput{
		AuthInput: model.AuthInput{Email: td.user.Email, Password: td.password},
	}

	runHandlerTests(t, http.MethodPost, "/api/v1/auth/login", []handlerTest{
		{
			Name:         "invalid email",
			Body:         dto.LogInRequestBody{Email: "john", Password: td.password},
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:         "too long password",
			Body:         dto.LogInRequestBody{Email: td.user.Email, Password: string(make([]byte, 65))},
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name: "incorrect password",
			Body: body,
			Setup: func(d *deps) {
				d.auth.EXPECT().LogIn(mock.Anything, input).Return(nil, model.AuthErrorIncorrectPassword())
			},
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.AuthErrorIncorrectPasswordKey,
		},
		{
			Name: "user not found",
			Body: body,
			Setup: func(d *deps) {
				d.auth.EXPECT().LogIn(mock.Anything, input).
					Return(nil, fmt.Errorf("user not found: %w", model.ErrNotFound))
			},
			CodeExpected: http.StatusNotFound,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name: "success",
			Body: body,
			Setup: func(d *deps) {
				d.auth.EXPECT().LogIn(mock.Anything, input).
					Return(&model.SuccessLogInResult{User: td.user, Tokens: td.tokens}, nil)
			},
			CodeExpected: http.StatusOK,
			Check: func(t *testing.T, w *httptest.ResponseRecorder) {
				requireAuthResponse(t, w, td)
			},
		},
	})
}

func TestRefreshTokensHandler(t *testing.T) {
	t.Parallel()
	td := newAuthTestData()
	oldToken := "old-refresh"
	body := dto.RefreshTokensRequestBody{RefreshToken: oldToken}

	runHandlerTests(t, http.MethodPost, "/api/v1/auth/refresh", []handlerTest{
		{
			Name:         "missing token",
			Body:         `{}`,
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name: "invalid token",
			Body: body,
			Setup: func(d *deps) {
				d.auth.EXPECT().RefreshTokens(mock.Anything, oldToken).Return(nil, model.AuthErrorInvalidToken())
			},
			CodeExpected: http.StatusUnauthorized,
			KeyExpected:  model.AuthErrorInvalidTokenKey,
		},
		{
			Name: "expired token",
			Body: body,
			Setup: func(d *deps) {
				d.auth.EXPECT().RefreshTokens(mock.Anything, oldToken).Return(nil, model.AuthErrorTokenExpired())
			},
			CodeExpected: http.StatusUnauthorized,
			KeyExpected:  model.AuthErrorTokenExpiredKey,
		},
		{
			Name: "success",
			Body: body,
			Setup: func(d *deps) {
				d.auth.EXPECT().RefreshTokens(mock.Anything, oldToken).Return(td.tokens, nil)
			},
			CodeExpected: http.StatusOK,
			Check: func(t *testing.T, w *httptest.ResponseRecorder) {
				require.Equal(t, dto.Tokens{
					AccessToken:  td.tokens.AccessToken,
					RefreshToken: td.tokens.RefreshToken,
				}, decodeBody[dto.Tokens](t, w))
			},
		},
	})
}

func TestPasswordRecoveryHandler(t *testing.T) {
	t.Parallel()
	email := "test@example.com"
	body := dto.PasswordRecoveryRequestBody{Email: email}

	runHandlerTests(t, http.MethodPost, "/api/v1/auth/password-recovery", []handlerTest{
		{
			Name:         "invalid email",
			Body:         dto.PasswordRecoveryRequestBody{Email: "test@"},
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name: "user not found",
			Body: body,
			Setup: func(d *deps) {
				d.auth.EXPECT().StartPasswordRecovery(mock.Anything, email).
					Return(fmt.Errorf("user not found: %w", model.ErrNotFound))
			},
			CodeExpected: http.StatusNotFound,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name: "mail error",
			Body: body,
			Setup: func(d *deps) {
				d.auth.EXPECT().StartPasswordRecovery(mock.Anything, email).
					Return(errors.New("smtp error"))
			},
			CodeExpected: http.StatusInternalServerError,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name: "success",
			Body: body,
			Setup: func(d *deps) {
				d.auth.EXPECT().StartPasswordRecovery(mock.Anything, email).Return(nil)
			},
			CodeExpected: http.StatusOK,
			Check: func(t *testing.T, w *httptest.ResponseRecorder) {
				require.Empty(t, w.Body.String())
			},
		},
	})
}

func TestVerifyCodeHandler(t *testing.T) {
	t.Parallel()
	email := "test@example.com"
	code := "123456"
	body := dto.VerifyRecoveryCodeRequestBody{Email: email, Code: code}

	runHandlerTests(t, http.MethodPost, "/api/v1/auth/password-recovery/verify", []handlerTest{
		{
			Name:         "short code",
			Body:         dto.VerifyRecoveryCodeRequestBody{Email: email, Code: "12345"},
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:         "non numeric code",
			Body:         dto.VerifyRecoveryCodeRequestBody{Email: email, Code: "12345a"},
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name: "invalid code",
			Body: body,
			Setup: func(d *deps) {
				d.auth.EXPECT().VerifyCode(mock.Anything, email, code).
					Return("", model.AuthErrorRecoveryCodeInvalid())
			},
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.AuthRecoveryCodeInvalidKey,
		},
		{
			Name: "expired code",
			Body: body,
			Setup: func(d *deps) {
				d.auth.EXPECT().VerifyCode(mock.Anything, email, code).
					Return("", model.AuthErrorRecoveryCodeExpired())
			},
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.AuthRecoveryCodeExpiredKey,
		},
		{
			Name: "success",
			Body: body,
			Setup: func(d *deps) {
				d.auth.EXPECT().VerifyCode(mock.Anything, email, code).Return("reset-token", nil)
			},
			CodeExpected: http.StatusOK,
			Check: func(t *testing.T, w *httptest.ResponseRecorder) {
				require.Equal(t, "reset-token", decodeBody[dto.VerifyRecoveryCodeResponse](t, w).ResetToken)
			},
		},
	})
}

func TestResetPasswordHandler(t *testing.T) {
	t.Parallel()
	td := newAuthTestData()
	resetToken := "reset-token"
	body := dto.ResetPasswordRequestBody{NewPassword: td.password, ResetToken: resetToken}

	runHandlerTests(t, http.MethodPost, "/api/v1/auth/password-recovery/reset", []handlerTest{
		{
			Name:         "missing reset token",
			Body:         dto.ResetPasswordRequestBody{NewPassword: td.password},
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:         "short password",
			Body:         dto.ResetPasswordRequestBody{NewPassword: "1234567", ResetToken: resetToken},
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name: "invalid reset token",
			Body: body,
			Setup: func(d *deps) {
				d.auth.EXPECT().ResetPassword(mock.Anything, td.password, resetToken).
					Return(nil, model.AuthErrorResetTokenInvalid())
			},
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.AuthResetTokenInvalidKey,
		},
		{
			Name: "expired reset token",
			Body: body,
			Setup: func(d *deps) {
				d.auth.EXPECT().ResetPassword(mock.Anything, td.password, resetToken).
					Return(nil, model.AuthErrorResetTokenExpired())
			},
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.AuthResetTokenExpiredKey,
		},
		{
			Name: "success",
			Body: body,
			Setup: func(d *deps) {
				d.auth.EXPECT().ResetPassword(mock.Anything, td.password, resetToken).
					Return(&model.SuccessResetPasswordResult{User: td.user, Tokens: td.tokens}, nil)
			},
			CodeExpected: http.StatusOK,
			Check: func(t *testing.T, w *httptest.ResponseRecorder) {
				requireAuthResponse(t, w, td)
			},
		},
	})
}
