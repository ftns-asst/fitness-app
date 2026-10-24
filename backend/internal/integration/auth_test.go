//go:build integration

package integration

import (
	"ftns-asst/internal/model"
	"ftns-asst/internal/rest/dto"
	"net/http"
	"testing"

	"github.com/stretchr/testify/require"
)

const password = "somePass98"

func signUp(t *testing.T, c *client, email string) dto.SignUpResponse {
	t.Helper()
	var resp dto.SignUpResponse
	c.requireOK(http.MethodPost, "/auth/signup", dto.SignUpRequestBody{
		Name:     "John",
		Email:    email,
		Password: password,
	}, &resp)
	return resp
}

func logIn(t *testing.T, c *client, email, pass string) dto.LogInResponse {
	t.Helper()
	var resp dto.LogInResponse
	c.requireOK(http.MethodPost, "/auth/login", dto.LogInRequestBody{Email: email, Password: pass}, &resp)
	return resp
}

func TestSignUpAndLogIn(t *testing.T) {
	t.Parallel()
	c := newClient(t)
	email := newEmail()

	var check dto.CheckEmailResponse
	c.requireOK(http.MethodPost, "/auth/check-email", dto.CheckEmailRequestBody{Email: email}, &check)
	require.True(t, check.Free)

	signed := signUp(t, c, email)
	require.NotEmpty(t, signed.User.ID)
	require.Equal(t, email, signed.User.Email)
	require.False(t, signed.User.CreatedAt.IsZero())
	require.NotEmpty(t, signed.Tokens.AccessToken)
	require.NotEmpty(t, signed.Tokens.RefreshToken)

	c.requireOK(http.MethodPost, "/auth/check-email", dto.CheckEmailRequestBody{Email: email}, &check)
	require.False(t, check.Free)

	c.requireError(http.MethodPost, "/auth/signup", dto.SignUpRequestBody{
		Name: "Other", Email: email, Password: password,
	}, http.StatusBadRequest, model.AuthErrorEmailAlreadyUsedKey)

	logged := logIn(t, c, email, password)
	require.Equal(t, signed.User.ID, logged.User.ID)

	c.requireError(http.MethodPost, "/auth/login", dto.LogInRequestBody{
		Email: email, Password: "wrongPass1",
	}, http.StatusBadRequest, model.AuthErrorIncorrectPasswordKey)

	c.token = logged.Tokens.AccessToken
	var me dto.UserWithProfileResponse
	c.requireOK(http.MethodGet, "/users/me", nil, &me)
	require.Equal(t, signed.User.ID, me.ID)
}

func TestLogInUnknownEmail(t *testing.T) {
	t.Parallel()
	c := newClient(t)

	c.requireError(http.MethodPost, "/auth/login", dto.LogInRequestBody{
		Email: newEmail(), Password: password,
	}, http.StatusNotFound, model.DefaultErrorKey)
}

func TestRefreshTokensRotation(t *testing.T) {
	t.Parallel()
	c := newClient(t)
	signed := signUp(t, c, newEmail())
	oldRefresh := signed.Tokens.RefreshToken

	var tokens dto.Tokens
	c.requireOK(http.MethodPost, "/auth/refresh", dto.RefreshTokensRequestBody{RefreshToken: oldRefresh}, &tokens)
	require.NotEmpty(t, tokens.AccessToken)
	require.NotEqual(t, oldRefresh, tokens.RefreshToken, "rotated refresh token must differ from the old one")

	c.requireError(http.MethodPost, "/auth/refresh", dto.RefreshTokensRequestBody{RefreshToken: oldRefresh},
		http.StatusUnauthorized, model.AuthErrorInvalidTokenKey)

	c.requireOK(http.MethodPost, "/auth/refresh", dto.RefreshTokensRequestBody{RefreshToken: tokens.RefreshToken}, &tokens)

	c.token = tokens.AccessToken
	c.requireOK(http.MethodGet, "/users/me", nil, nil)
}

func TestRefreshWithAccessToken(t *testing.T) {
	t.Parallel()
	c := newClient(t)
	signed := signUp(t, c, newEmail())

	c.requireError(http.MethodPost, "/auth/refresh", dto.RefreshTokensRequestBody{RefreshToken: signed.Tokens.AccessToken},
		http.StatusUnauthorized, model.AuthErrorInvalidTokenKey)
}

func TestPasswordRecovery(t *testing.T) {
	t.Parallel()
	c := newClient(t)
	email := newEmail()
	signed := signUp(t, c, email)
	newPassword := "newPass123"

	c.requireOK(http.MethodPost, "/auth/password-recovery", dto.PasswordRecoveryRequestBody{Email: email}, nil)
	code := mailer.waitCode(t, email)

	wrong := "000000"
	if code == wrong {
		wrong = "111111"
	}
	c.requireError(http.MethodPost, "/auth/password-recovery/verify", dto.VerifyRecoveryCodeRequestBody{
		Email: email, Code: wrong,
	}, http.StatusBadRequest, model.AuthRecoveryCodeInvalidKey)

	var verified dto.VerifyRecoveryCodeResponse
	c.requireOK(http.MethodPost, "/auth/password-recovery/verify", dto.VerifyRecoveryCodeRequestBody{
		Email: email, Code: code,
	}, &verified)
	require.NotEmpty(t, verified.ResetToken)

	c.requireError(http.MethodPost, "/auth/password-recovery/verify", dto.VerifyRecoveryCodeRequestBody{
		Email: email, Code: code,
	}, http.StatusBadRequest, model.AuthRecoveryCodeInvalidKey)

	var reset dto.ResetPasswordResponse
	c.requireOK(http.MethodPost, "/auth/password-recovery/reset", dto.ResetPasswordRequestBody{
		NewPassword: newPassword, ResetToken: verified.ResetToken,
	}, &reset)
	require.Equal(t, signed.User.ID, reset.User.ID)
	require.NotEmpty(t, reset.Tokens.RefreshToken)

	c.requireError(http.MethodPost, "/auth/password-recovery/reset", dto.ResetPasswordRequestBody{
		NewPassword: "anotherPass1", ResetToken: verified.ResetToken,
	}, http.StatusBadRequest, model.AuthResetTokenInvalidKey)

	c.requireError(http.MethodPost, "/auth/login", dto.LogInRequestBody{
		Email: email, Password: password,
	}, http.StatusBadRequest, model.AuthErrorIncorrectPasswordKey)
	logIn(t, c, email, newPassword)

	c.requireError(http.MethodPost, "/auth/refresh", dto.RefreshTokensRequestBody{RefreshToken: signed.Tokens.RefreshToken},
		http.StatusUnauthorized, model.AuthErrorInvalidTokenKey)
}

func TestPasswordRecoveryNewCodeRevokesOld(t *testing.T) {
	t.Parallel()
	c := newClient(t)
	email := newEmail()
	signUp(t, c, email)

	c.requireOK(http.MethodPost, "/auth/password-recovery", dto.PasswordRecoveryRequestBody{Email: email}, nil)
	oldCode := mailer.waitCode(t, email)
	c.requireOK(http.MethodPost, "/auth/password-recovery", dto.PasswordRecoveryRequestBody{Email: email}, nil)
	newCode := mailer.waitCode(t, email)

	if oldCode != newCode {
		c.requireError(http.MethodPost, "/auth/password-recovery/verify", dto.VerifyRecoveryCodeRequestBody{
			Email: email, Code: oldCode,
		}, http.StatusBadRequest, model.AuthRecoveryCodeInvalidKey)
	}
	c.requireOK(http.MethodPost, "/auth/password-recovery/verify", dto.VerifyRecoveryCodeRequestBody{
		Email: email, Code: newCode,
	}, nil)
}

func TestPasswordRecoveryUnknownEmail(t *testing.T) {
	t.Parallel()
	c := newClient(t)

	c.requireError(http.MethodPost, "/auth/password-recovery", dto.PasswordRecoveryRequestBody{Email: newEmail()},
		http.StatusNotFound, model.DefaultErrorKey)
}
