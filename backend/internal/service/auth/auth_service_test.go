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
}

func TestSignUp(t *testing.T) {
	t.Parallel()
	username := "John"
	email := "test@example.com"
	password := "somePass1"
	createdUser := &model.User{
		ID:    uuid.New(),
		Name:  username,
		Email: email,
	}

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
						res := createdUser
						res.PasswordHash = user.PasswordHash
						return res, nil
					})
				d.repo.EXPECT().CreateToken(mock.Anything, mock.IsType(&model.RefreshToken{})).
					RunAndReturn(func(ctx context.Context, token *model.RefreshToken) (*model.RefreshToken, error) {
						require.Equal(t, token.UserID, createdUser.ID)
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
			d := deps{
				repo:    NewMockAuthRepo(t),
				userSvc: NewMockUserProvider(t),
			}
			if test.setupMock != nil {
				test.setupMock(d)
			}

			svc := NewService(&config.Config{}, d.userSvc, d.repo, nil, d.tx)
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
	email := "test@example.com"
	password := "somePass1"
	passwordHash, err := util.Hash(password)
	require.NoError(t, err)
	user := &model.User{
		ID:           uuid.New(),
		Name:         "SomeName",
		Email:        email,
		PasswordHash: passwordHash,
		CreatedAt:    time.Now().Add(-time.Hour),
	}

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
					Return(user, nil)
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
					Return(user, nil)

				d.repo.EXPECT().CreateToken(mock.Anything, mock.IsType(&model.RefreshToken{})).
					RunAndReturn(func(ctx context.Context, token *model.RefreshToken) (*model.RefreshToken, error) {
						require.Equal(t, token.UserID, user.ID)
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
			d := deps{
				repo:    NewMockAuthRepo(t),
				userSvc: NewMockUserProvider(t),
			}
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

var testCfg = &config.Config{
	AccessTokenJWTSecretKey:       "access-secret",
	RefreshTokenJWTSecretKey:      "refresh-secret",
	RefreshTokenHashSecretKey:     "refresh-hash-secret",
	VerificationCodeHashSecretKey: "code-hash-secret",
	ResetTokenHashSecretKey:       "reset-hash-secret",
}

func TestRefreshTokens(t *testing.T) {
	t.Parallel()
	userID := uuid.New()
	tokenID := uuid.New()

	validToken, err := jwtutil.GenRefreshToken(userID, testCfg.RefreshTokenJWTSecretKey)
	require.NoError(t, err)
	validTokenHash, err := util.HashSHA256(validToken.Token, testCfg.RefreshTokenHashSecretKey)
	require.NoError(t, err)

	expiredToken, err := jwt.NewWithClaims(jwt.SigningMethodHS256, jwtutil.UserJWTClaims{
		UserID: userID,
		RegisteredClaims: jwt.RegisteredClaims{
			ExpiresAt: jwt.NewNumericDate(time.Now().Add(-time.Hour)),
		},
	}).SignedString([]byte(testCfg.RefreshTokenJWTSecretKey))
	require.NoError(t, err)

	wrongSecretToken, err := jwtutil.GenRefreshToken(userID, "wrong-secret")
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
				d.repo.EXPECT().GetNotUsedTokenByUserIDAndHash(mock.Anything, userID, validTokenHash).
					Return(nil, repository.ErrNotFound)
			},
			ErrorExpected:    model.ErrUnauthorized,
			ErrorKeyExpected: util.Ptr(model.AuthErrorInvalidTokenKey),
		},
		{
			Name:  "error token concurrently used",
			Token: validToken.Token,
			setupMock: func(d deps) {
				d.repo.EXPECT().GetNotUsedTokenByUserIDAndHash(mock.Anything, userID, validTokenHash).
					Return(&model.RefreshToken{ID: tokenID, UserID: userID, TokenHash: validTokenHash}, nil)
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
				d.repo.EXPECT().GetNotUsedTokenByUserIDAndHash(mock.Anything, userID, validTokenHash).
					Return(&model.RefreshToken{ID: tokenID, UserID: userID, TokenHash: validTokenHash}, nil)
				d.repo.EXPECT().SetTokenUsedByID(mock.Anything, tokenID).
					Return(nil)
				d.repo.EXPECT().CreateToken(mock.Anything, mock.IsType(&model.RefreshToken{})).
					RunAndReturn(func(ctx context.Context, token *model.RefreshToken) (*model.RefreshToken, error) {
						require.Equal(t, token.UserID, userID)
						token.ID = uuid.New()
						return token, nil
					})
			},
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			t.Parallel()
			d := deps{
				repo:    NewMockAuthRepo(t),
				userSvc: NewMockUserProvider(t),
			}
			if test.setupMock != nil {
				test.setupMock(d)
			}

			svc := NewService(testCfg, d.userSvc, d.repo, nil, d.tx)
			res, err := svc.RefreshTokens(context.Background(), test.Token)
			require.ErrorIs(t, err, test.ErrorExpected)
			if test.ErrorKeyExpected != nil {
				testutil.EqualSvcErrKey(t, err, *test.ErrorKeyExpected)
			}
			if test.ErrorExpected == nil {
				require.NotNil(t, res)
				require.NotEmpty(t, res.AccessToken)
				require.NotEmpty(t, res.RefreshToken)

				info, err := jwtutil.ParseJWTToken(res.RefreshToken, testCfg.RefreshTokenJWTSecretKey)
				require.NoError(t, err)
				require.Equal(t, info.UserID, userID)
				info, err = jwtutil.ParseJWTToken(res.AccessToken, testCfg.AccessTokenJWTSecretKey)
				require.NoError(t, err)
				require.Equal(t, info.UserID, userID)
			}
		})
	}
}
