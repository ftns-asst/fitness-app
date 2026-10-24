package auth

import (
	"context"
	"ftns-asst/internal/config"
	"ftns-asst/internal/model"
	"ftns-asst/internal/repository"
	"ftns-asst/internal/util"
	"ftns-asst/internal/util/jwtutil"
	"ftns-asst/internal/util/testutil"
	"testing"
	"time"

	"github.com/go-openapi/testify/v2/require"
	"github.com/golang-jwt/jwt/v5"
	"github.com/google/uuid"
	"github.com/stretchr/testify/mock"
)

type deps struct {
	userSvc *MockUserProvider
	repo    *MockAuthRepo
	tx      *testutil.MockTransactor
	mailer  *testutil.MockMailer
}

type testData struct {
	cfg  *config.Config
	user *model.User
	pass *string
}

func newTestData(t *testing.T, d *deps, cfg *config.Config, pass *string) *testData {
	testData := &testData{
		cfg:  cfg,
		pass: pass,
	}

	if testData.cfg == nil {
		testData.cfg = &config.Config{
			AccessTokenJWTSecretKey:       "access-secret",
			RefreshTokenJWTSecretKey:      "refresh-secret",
			RefreshTokenHashSecretKey:     "refresh-hash-secret",
			VerificationCodeHashSecretKey: "code-hash-secret",
			ResetTokenHashSecretKey:       "reset-hash-secret",
		}
	}

	if testData.pass == nil {
		testData.pass = new("somePass98")
	}

	passwordHash, err := util.Hash(*testData.pass)
	require.NoError(t, err)
	testData.user = &model.User{
		ID:           uuid.New(),
		Name:         "John",
		Email:        "test@example.com",
		PasswordHash: passwordHash,
		CreatedAt:    time.Now().Add(-time.Hour),
	}

	return testData
}

func newDeps(t *testing.T) *deps {
	return &deps{
		userSvc: NewMockUserProvider(t),
		repo:    NewMockAuthRepo(t),
		tx:      &testutil.MockTransactor{},
		mailer:  &testutil.MockMailer{},
	}
}

func TestSignUp(t *testing.T) {
	t.Parallel()
	dt := newTestData(t, nil, nil, nil)
	email := dt.user.Email
	password := *dt.pass
	username := dt.user.Name

	tests := []struct {
		Name             string
		Email            string
		Password         string
		Username         string
		setupMock        func(d deps)
		ErrorExpected    error
		ErrorKeyExpected *string
	}{
		{
			Name:     "error email taken",
			Email:    email,
			Password: password,
			Username: username,
			setupMock: func(d deps) {
				d.userSvc.EXPECT().CheckUserWithEmailExists(mock.Anything, email).
					Return(true, nil)
			},
			ErrorExpected:    model.ErrBadRequest,
			ErrorKeyExpected: util.Ptr(model.AuthErrorEmailAlreadyUsedKey),
		},
		{
			Name:     "success singup",
			Email:    email,
			Password: password,
			Username: username,
			setupMock: func(d deps) {
				d.userSvc.EXPECT().CheckUserWithEmailExists(mock.Anything, email).
					Return(false, nil)
				d.userSvc.EXPECT().CreateUser(mock.Anything, mock.IsType(&model.User{})).
					RunAndReturn(func(ctx context.Context, user *model.User) (*model.User, error) {
						res := dt.user
						res.PasswordHash = user.PasswordHash
						return res, nil
					})
				d.repo.EXPECT().CreateToken(mock.Anything, mock.IsType(&model.RefreshToken{})).
					RunAndReturn(func(ctx context.Context, token *model.RefreshToken) (*model.RefreshToken, error) {
						require.Equal(t, token.UserID, dt.user.ID)
						token.ID = uuid.New()
						token.UsedAt = nil
						return token, nil
					})
			},

			ErrorExpected:    nil,
			ErrorKeyExpected: nil,
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			t.Parallel()
			deps := newDeps(t)
			if test.setupMock != nil {
				test.setupMock(*deps)
			}

			svc := NewService(&config.Config{}, deps.userSvc, deps.repo, nil, deps.tx)
			res, err := svc.SignUp(context.Background(), &model.SignUpInput{
				AuthInput: model.AuthInput{Email: test.Email, Password: test.Password},
				Name:      test.Username,
			})
			require.ErrorIs(t, err, test.ErrorExpected)
			if test.ErrorKeyExpected != nil {
				testutil.EqualSvcErrKey(t, err, *test.ErrorKeyExpected)
			}
			if test.ErrorExpected == nil {
				require.Equal(t, res.User.Email, email)
				require.Equal(t, res.User.Name, username)
				ok, err := util.CompareHash(password, res.User.PasswordHash)
				require.NoError(t, err)
				require.True(t, ok)
				require.NotNil(t, res.Tokens)
				require.NotEmpty(t, res.Tokens.AccessToken)
				require.NotEmpty(t, res.Tokens.RefreshToken)

			}
		})
	}
}

func TestLogIn(t *testing.T) {
	t.Parallel()
	dt := newTestData(t, nil, nil, nil)
	email := dt.user.Email
	password := *dt.pass

	tests := []struct {
		Name             string
		Email            string
		Password         string
		setupMock        func(d deps)
		ErrorExpected    error
		ErrorKeyExpected *string
	}{
		{
			Name:     "error user not found",
			Email:    email,
			Password: password,
			setupMock: func(d deps) {
				d.userSvc.EXPECT().GetUserByEmail(mock.Anything, email).
					Return(nil, model.ErrNotFound)
			},
			ErrorExpected:    model.ErrNotFound,
			ErrorKeyExpected: nil,
		},
		{
			Name:     "incorrect password",
			Email:    email,
			Password: "incorrect123",
			setupMock: func(d deps) {
				d.userSvc.EXPECT().GetUserByEmail(mock.Anything, email).
					Return(dt.user, nil)
			},

			ErrorExpected:    model.ErrBadRequest,
			ErrorKeyExpected: util.Ptr(model.AuthErrorIncorrectPasswordKey),
		},
		{
			Name:     "success login",
			Email:    email,
			Password: password,
			setupMock: func(d deps) {
				d.userSvc.EXPECT().GetUserByEmail(mock.Anything, email).
					Return(dt.user, nil)

				d.repo.EXPECT().CreateToken(mock.Anything, mock.IsType(&model.RefreshToken{})).
					RunAndReturn(func(ctx context.Context, token *model.RefreshToken) (*model.RefreshToken, error) {
						require.Equal(t, token.UserID, dt.user.ID)
						token.ID = uuid.New()
						token.UsedAt = nil
						return token, nil
					})
			},
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			t.Parallel()
			d := util.UnPtr(newDeps(t))
			if test.setupMock != nil {
				test.setupMock(d)
			}

			svc := NewService(&config.Config{}, d.userSvc, d.repo, nil, d.tx)
			res, err := svc.LogIn(context.Background(), &model.LogInInput{
				AuthInput: model.AuthInput{Email: test.Email, Password: test.Password},
			})
			require.ErrorIs(t, err, test.ErrorExpected)
			if test.ErrorKeyExpected != nil {
				testutil.EqualSvcErrKey(t, err, *test.ErrorKeyExpected)
			}
			if test.ErrorExpected == nil {
				require.Equal(t, res.User.Email, email)
				ok, err := util.CompareHash(password, res.User.PasswordHash)
				require.NoError(t, err)
				require.True(t, ok)
				require.NotNil(t, res.Tokens)
				require.NotEmpty(t, res.Tokens.AccessToken)
				require.NotEmpty(t, res.Tokens.RefreshToken)
			}
		})
	}
}

func TestRefreshTokens(t *testing.T) {
	t.Parallel()
	dt := newTestData(t, nil, nil, nil)
	tokenID := uuid.New()

	validToken, err := jwtutil.GenRefreshToken(dt.user.ID, dt.cfg.RefreshTokenJWTSecretKey)
	require.NoError(t, err)
	validTokenHash, err := util.HashSHA256(validToken.Token, dt.cfg.RefreshTokenHashSecretKey)
	require.NoError(t, err)

	expiredToken, err := jwt.NewWithClaims(jwt.SigningMethodHS256, jwtutil.UserJWTClaims{
		UserID: dt.user.ID,
		RegisteredClaims: jwt.RegisteredClaims{
			ExpiresAt: jwt.NewNumericDate(time.Now().Add(-time.Hour)),
		},
	}).SignedString([]byte(dt.cfg.RefreshTokenJWTSecretKey))
	require.NoError(t, err)

	wrongSecretToken, err := jwtutil.GenRefreshToken(dt.user.ID, "wrong-secret")
	require.NoError(t, err)

	tests := []struct {
		Name             string
		Token            string
		setupMock        func(d deps)
		ErrorExpected    error
		ErrorKeyExpected *string
	}{
		{
			Name:             "error not jwt token",
			Token:            "not-jwt",
			ErrorExpected:    model.ErrUnauthorized,
			ErrorKeyExpected: util.Ptr(model.AuthErrorInvalidTokenKey),
		},
		{
			Name:             "error wrong jwt secret",
			Token:            wrongSecretToken.Token,
			ErrorExpected:    model.ErrUnauthorized,
			ErrorKeyExpected: util.Ptr(model.AuthErrorInvalidTokenKey),
		},
		{
			Name:             "error token expired",
			Token:            expiredToken,
			ErrorExpected:    model.ErrUnauthorized,
			ErrorKeyExpected: util.Ptr(model.AuthErrorTokenExpiredKey),
		},
		{
			Name:  "error token not found or already used",
			Token: validToken.Token,
			setupMock: func(d deps) {
				d.repo.EXPECT().GetNotUsedTokenByUserIDAndHash(mock.Anything, dt.user.ID, validTokenHash).
					Return(nil, repository.ErrNotFound)
			},
			ErrorExpected:    model.ErrUnauthorized,
			ErrorKeyExpected: util.Ptr(model.AuthErrorInvalidTokenKey),
		},
		{
			Name:  "error token concurrently used",
			Token: validToken.Token,
			setupMock: func(d deps) {
				d.repo.EXPECT().GetNotUsedTokenByUserIDAndHash(mock.Anything, dt.user.ID, validTokenHash).
					Return(&model.RefreshToken{ID: tokenID, UserID: dt.user.ID, TokenHash: validTokenHash}, nil)
				d.repo.EXPECT().SetTokenUsedByID(mock.Anything, tokenID).
					Return(repository.ErrNoAffectedRows)
			},
			ErrorExpected:    model.ErrUnauthorized,
			ErrorKeyExpected: util.Ptr(model.AuthErrorInvalidTokenKey),
		},
		{
			Name:  "success refresh",
			Token: validToken.Token,
			setupMock: func(d deps) {
				d.repo.EXPECT().GetNotUsedTokenByUserIDAndHash(mock.Anything, dt.user.ID, validTokenHash).
					Return(&model.RefreshToken{ID: tokenID, UserID: dt.user.ID, TokenHash: validTokenHash}, nil)
				d.repo.EXPECT().SetTokenUsedByID(mock.Anything, tokenID).
					Return(nil)
				d.repo.EXPECT().CreateToken(mock.Anything, mock.IsType(&model.RefreshToken{})).
					RunAndReturn(func(ctx context.Context, token *model.RefreshToken) (*model.RefreshToken, error) {
						require.Equal(t, token.UserID, dt.user.ID)
						token.ID = uuid.New()
						return token, nil
					})
			},
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			t.Parallel()
			d := util.UnPtr(newDeps(t))
			if test.setupMock != nil {
				test.setupMock(d)
			}

			svc := NewService(dt.cfg, d.userSvc, d.repo, nil, d.tx)
			res, err := svc.RefreshTokens(context.Background(), test.Token)
			require.ErrorIs(t, err, test.ErrorExpected)
			if test.ErrorKeyExpected != nil {
				testutil.EqualSvcErrKey(t, err, *test.ErrorKeyExpected)
			}
			if test.ErrorExpected == nil {
				require.NotNil(t, res)
				require.NotEmpty(t, res.AccessToken)
				require.NotEmpty(t, res.RefreshToken)

				info, err := jwtutil.ParseJWTToken(res.RefreshToken, dt.cfg.RefreshTokenJWTSecretKey)
				require.NoError(t, err)
				require.Equal(t, info.UserID, dt.user.ID)
				info, err = jwtutil.ParseJWTToken(res.AccessToken, dt.cfg.AccessTokenJWTSecretKey)
				require.NoError(t, err)
				require.Equal(t, info.UserID, dt.user.ID)
			}
		})
	}
}

func TestStartPasswordRecovery(t *testing.T) {
	t.Parallel()
	dt := newTestData(t, nil, nil, nil)

	tests := []struct {
		Name             string
		Email            string
		setupMock        func(d deps)
		check            func(t *testing.T, d *deps)
		ErrorExpected    error
		ErrorKeyExpected *string
	}{
		{
			Name:  "error user not found",
			Email: "some@example.com",
			setupMock: func(d deps) {
				d.userSvc.EXPECT().GetUserByEmail(mock.Anything, "some@example.com").
					Return(nil, repository.ErrNotFound)
			},
			ErrorExpected: model.ErrNotFound,
		},
		{
			Name:  "success start password recovery",
			Email: dt.user.Email,
			setupMock: func(d deps) {
				d.userSvc.EXPECT().GetUserByEmail(mock.Anything, dt.user.Email).
					Return(dt.user, nil)
				d.repo.EXPECT().CreateVerificationCodeAndRevokeOther(mock.Anything, mock.IsType(&model.VerificationCode{})).
					Return(nil, nil)
			},
			check: func(t *testing.T, d *deps) {
				require.Eventually(t, func() bool {
					return d.mailer.Count() == 1
				}, time.Second, time.Millisecond)
			},
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			d := util.UnPtr(newDeps(t))
			if test.setupMock != nil {
				test.setupMock(d)
			}

			svc := NewService(dt.cfg, d.userSvc, d.repo, d.mailer, d.tx)
			err := svc.StartPasswordRecovery(context.Background(), test.Email)
			require.ErrorIs(t, err, test.ErrorExpected)
			if test.ErrorKeyExpected != nil {
				testutil.EqualSvcErrKey(t, err, *test.ErrorKeyExpected)
			}
			if test.check != nil {
				test.check(t, &d)
			}
		})
	}
}

func TestVerifyCode(t *testing.T) {
	t.Parallel()
	dt := newTestData(t, nil, nil, nil)

	codeValue, err := genNumericCode()
	require.NoError(t, err)

	codeHash, err := util.HashSHA256(codeValue, dt.cfg.VerificationCodeHashSecretKey)
	require.NoError(t, err)

	code := model.VerificationCode{
		ID:        uuid.New(),
		UserID:    dt.user.ID,
		CodeHash:  codeHash,
		UsedAt:    nil,
		ExpiresAt: time.Now().Add(time.Hour),
		RevokedAt: nil,
	}
	codeExpired := code
	codeExpired.ExpiresAt = time.Now().Add(-time.Hour)

	var generatedTokenHash string

	tests := []struct {
		Name             string
		Email            string
		CodeInput        string
		setupMock        func(d deps)
		check            func(t *testing.T, d *deps)
		ErrorExpected    error
		ErrorKeyExpected *string
	}{

		{
			Name:  "error user not found",
			Email: "some@example.com",
			setupMock: func(d deps) {
				d.userSvc.EXPECT().GetUserByEmail(mock.Anything, "some@example.com").
					Return(nil, repository.ErrNotFound)
			},
			ErrorExpected: model.ErrNotFound,
		},
		{
			Name:      "error code used or revoked",
			Email:     dt.user.Email,
			CodeInput: codeValue,
			setupMock: func(d deps) {
				d.userSvc.EXPECT().GetUserByEmail(mock.Anything, dt.user.Email).
					Return(dt.user, nil)
				d.repo.EXPECT().GetCodeNotUsedOrRevokedByUserID(mock.Anything, dt.user.ID).
					Return(nil, repository.ErrNotFound)
			},
			ErrorExpected:    model.ErrBadRequest,
			ErrorKeyExpected: util.Ptr(model.AuthRecoveryCodeInvalidKey),
		},
		{
			Name:      "error code expired",
			Email:     dt.user.Email,
			CodeInput: codeValue,
			setupMock: func(d deps) {
				d.userSvc.EXPECT().GetUserByEmail(mock.Anything, dt.user.Email).
					Return(dt.user, nil)
				d.repo.EXPECT().GetCodeNotUsedOrRevokedByUserID(mock.Anything, dt.user.ID).
					Return(&codeExpired, nil)
			},
			ErrorExpected:    model.ErrBadRequest,
			ErrorKeyExpected: util.Ptr(model.AuthRecoveryCodeExpiredKey),
		},
		{
			Name:      "error wrong code",
			Email:     dt.user.Email,
			CodeInput: "wrong_code",
			setupMock: func(d deps) {
				d.userSvc.EXPECT().GetUserByEmail(mock.Anything, dt.user.Email).
					Return(dt.user, nil)
				d.repo.EXPECT().GetCodeNotUsedOrRevokedByUserID(mock.Anything, dt.user.ID).
					Return(&code, nil)
			},
			ErrorExpected:    model.ErrBadRequest,
			ErrorKeyExpected: util.Ptr(model.AuthRecoveryCodeInvalidKey),
		},
		{
			Name:      "error code used or revoked concurrently",
			Email:     dt.user.Email,
			CodeInput: codeValue,
			setupMock: func(d deps) {
				d.userSvc.EXPECT().GetUserByEmail(mock.Anything, dt.user.Email).
					Return(dt.user, nil)
				d.repo.EXPECT().GetCodeNotUsedOrRevokedByUserID(mock.Anything, dt.user.ID).
					Return(&code, nil)
				d.repo.EXPECT().SetCodeUsedByID(mock.Anything, code.ID).
					Return(repository.ErrNoAffectedRows)
			},
			ErrorExpected:    model.ErrBadRequest,
			ErrorKeyExpected: util.Ptr(model.AuthRecoveryCodeInvalidKey),
		},
		{
			Name:      "success verify code",
			Email:     dt.user.Email,
			CodeInput: codeValue,
			setupMock: func(d deps) {
				d.userSvc.EXPECT().GetUserByEmail(mock.Anything, dt.user.Email).
					Return(dt.user, nil)
				d.repo.EXPECT().GetCodeNotUsedOrRevokedByUserID(mock.Anything, dt.user.ID).
					Return(&code, nil)
				d.repo.EXPECT().SetCodeUsedByID(mock.Anything, code.ID).
					Return(nil)
				d.repo.EXPECT().CreateResetToken(mock.Anything, mock.IsType(&model.ResetToken{})).
					RunAndReturn(func(ctx context.Context, token *model.ResetToken) (*model.ResetToken, error) {
						require.Equal(t, token.UserID, dt.user.ID)
						generatedTokenHash = token.TokenHash
						return token, nil
					})

			},
			ErrorExpected:    nil,
			ErrorKeyExpected: nil,
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			d := util.UnPtr(newDeps(t))
			if test.setupMock != nil {
				test.setupMock(d)
			}

			svc := NewService(dt.cfg, d.userSvc, d.repo, d.mailer, d.tx)
			result, err := svc.VerifyCode(context.Background(), test.Email, test.CodeInput)
			require.ErrorIs(t, err, test.ErrorExpected)
			if test.ErrorKeyExpected != nil {
				testutil.EqualSvcErrKey(t, err, *test.ErrorKeyExpected)
			}
			if test.ErrorExpected == nil {
				require.NotEmpty(t, result)
				resultHash, err := util.HashSHA256(result, dt.cfg.ResetTokenHashSecretKey)
				require.NoError(t, err)
				require.Equal(t, generatedTokenHash, resultHash)
			} else {
				require.Empty(t, result)
			}
		})
	}

}

func TestResetPassword(t *testing.T) {
	dt := newTestData(t, nil, nil, nil)
	newPass := "newPass03"
	targetTokenValue := genResetToken()
	targetTokenHash, err := util.HashSHA256(targetTokenValue, dt.cfg.ResetTokenHashSecretKey)
	require.NoError(t, err)
	targetToken := &model.ResetToken{
		ID:        uuid.New(),
		TokenHash: targetTokenHash,
		UserID:    dt.user.ID,
		ExpiresAt: time.Now().Add(time.Hour),
		UsedAt:    nil,
	}

	tests := []struct {
		Name             string
		PasswordInput    string
		TokenInput       string
		setupMock        func(d deps)
		check            func(t *testing.T, d *deps)
		ErrorExpected    error
		ErrorKeyExpected *string
		ResExpected      *model.SuccessResetPasswordResult
	}{
		{
			Name:          "invalid password",
			PasswordInput: "123",
			TokenInput:    targetTokenValue,
			ErrorExpected: model.ErrBadRequest,
		},
		{
			Name:          "wrong token",
			PasswordInput: newPass,
			TokenInput:    "some_invalid_token",
			setupMock: func(d deps) {
				d.repo.EXPECT().GetNotUsedResetTokenByHash(mock.Anything, mock.IsType("")).
					Return(nil, repository.ErrNotFound)
			},
			ErrorExpected:    model.ErrBadRequest,
			ErrorKeyExpected: util.Ptr(model.AuthResetTokenInvalidKey),
		},
		{
			Name:          "user not found by reset token",
			PasswordInput: newPass,
			TokenInput:    targetTokenValue,
			setupMock: func(d deps) {
				d.repo.EXPECT().GetNotUsedResetTokenByHash(mock.Anything, targetTokenHash).
					Return(targetToken, nil)
				d.userSvc.EXPECT().GetUserByID(mock.Anything, dt.user.ID, false).
					Return(nil, repository.ErrNotFound)
			},
			ErrorExpected:    model.ErrBadRequest,
			ErrorKeyExpected: util.Ptr(model.AuthResetTokenInvalidKey),
		},
		{
			Name:          "error token concurrently used",
			PasswordInput: newPass,
			TokenInput:    targetTokenValue,
			setupMock: func(d deps) {
				d.repo.EXPECT().GetNotUsedResetTokenByHash(mock.Anything, targetTokenHash).
					Return(targetToken, nil)
				d.userSvc.EXPECT().GetUserByID(mock.Anything, dt.user.ID, false).
					Return(&model.UserWithProfile{
						User: *dt.user,
					}, nil)
				d.repo.EXPECT().SetResetTokenUsedByID(mock.Anything, targetToken.ID).
					Return(repository.ErrNoAffectedRows)
			},
			ErrorExpected:    model.ErrBadRequest,
			ErrorKeyExpected: util.Ptr(model.AuthResetTokenInvalidKey),
		},
		{
			Name:          "error not found user while password updating",
			PasswordInput: newPass,
			TokenInput:    targetTokenValue,
			setupMock: func(d deps) {
				d.repo.EXPECT().GetNotUsedResetTokenByHash(mock.Anything, targetTokenHash).
					Return(targetToken, nil)
				d.userSvc.EXPECT().GetUserByID(mock.Anything, dt.user.ID, false).
					Return(&model.UserWithProfile{
						User: *dt.user,
					}, nil)
				d.repo.EXPECT().SetResetTokenUsedByID(mock.Anything, targetToken.ID).
					Return(nil)
				d.userSvc.EXPECT().UpdateUserPasswordHash(mock.Anything, dt.user.ID, mock.IsType("")).
					RunAndReturn(func(ctx context.Context, userID uuid.UUID, newPasswordHash string) error {
						ok := util.IsValidHash(newPasswordHash)
						require.True(t, ok)
						return repository.ErrNoAffectedRows
					})
			},
			ErrorExpected:    model.ErrBadRequest,
			ErrorKeyExpected: util.Ptr(model.AuthResetTokenInvalidKey),
		},
		{
			Name:          "success reset password",
			PasswordInput: newPass,
			TokenInput:    targetTokenValue,
			setupMock: func(d deps) {
				d.repo.EXPECT().GetNotUsedResetTokenByHash(mock.Anything, targetTokenHash).
					Return(targetToken, nil)
				d.userSvc.EXPECT().GetUserByID(mock.Anything, dt.user.ID, false).
					Return(&model.UserWithProfile{
						User: *dt.user,
					}, nil)
				d.repo.EXPECT().SetResetTokenUsedByID(mock.Anything, targetToken.ID).
					Return(nil)
				d.userSvc.EXPECT().UpdateUserPasswordHash(mock.Anything, dt.user.ID, mock.IsType("")).
					RunAndReturn(func(ctx context.Context, userID uuid.UUID, newPasswordHash string) error {
						ok := util.IsValidHash(newPasswordHash)
						require.True(t, ok)
						return nil
					})
				d.repo.EXPECT().SetUserTokensUsed(mock.Anything, dt.user.ID).
					Return(nil)
				d.repo.EXPECT().CreateToken(mock.Anything, mock.IsType(&model.RefreshToken{})).
					RunAndReturn(func(ctx context.Context, token *model.RefreshToken) (*model.RefreshToken, error) {
						require.Equal(t, token.UserID, dt.user.ID)
						token.ID = uuid.New()
						token.UsedAt = nil
						return token, nil
					})
			},
			ErrorExpected:    nil,
			ErrorKeyExpected: nil,
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			d := util.UnPtr(newDeps(t))
			if test.setupMock != nil {
				test.setupMock(d)
			}

			svc := NewService(dt.cfg, d.userSvc, d.repo, d.mailer, d.tx)
			res, err := svc.ResetPassword(context.Background(), test.PasswordInput, test.TokenInput)
			if test.ErrorExpected != nil {
				require.ErrorIs(t, err, test.ErrorExpected)
				require.Nil(t, res)
				return
			}

			require.NoError(t, err)
			require.NotNil(t, res)
			require.Equal(t, res.User, dt.user)
			require.NotNil(t, res.Tokens)

			checkToken(t, res.Tokens.AccessToken, dt.user.ID, dt.cfg.AccessTokenJWTSecretKey)
			checkToken(t, res.Tokens.RefreshToken, dt.user.ID, dt.cfg.RefreshTokenJWTSecretKey)
		})
	}

}

func checkToken(t *testing.T, tokenString string, userID uuid.UUID, secret string) {
	tokenInfo, err := jwtutil.ParseJWTToken(tokenString, secret)
	require.NoError(t, err)
	require.NotNil(t, tokenInfo)
	require.Equal(t, tokenInfo.UserID, userID)
	require.True(t, tokenInfo.ExpiresAt.After(time.Now().Add(time.Minute)))
}
