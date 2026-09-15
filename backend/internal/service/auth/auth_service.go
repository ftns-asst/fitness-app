package auth

import (
	"context"
	"errors"
	"fmt"
	"ftns-asst/internal/config"
	"ftns-asst/internal/model"
	"ftns-asst/internal/repository"
	"ftns-asst/internal/service/users"
	"ftns-asst/internal/util"
	"time"

	"github.com/google/uuid"
)

type AuthService struct {
	cfg         *config.Config
	userService *users.UserService
	authRepo    *repository.AuthRepo
	tx          model.Transactor
}

func NewService(cfg *config.Config,
	userService *users.UserService,
	authRepo *repository.AuthRepo,
	transactor model.Transactor,
) *AuthService {
	return &AuthService{
		cfg:         cfg,
		userService: userService,
		authRepo:    authRepo,
		tx:          transactor,
	}
}

func (s *AuthService) SignUp(ctx context.Context, data *model.SignUpInput) (*model.SuccessSignUpResult, error) {
	err := data.Validate()
	if err != nil {
		return nil, model.NewError(err)
	}

	tx, err := s.tx.Begin(ctx)
	if err != nil {
		return nil, model.NewError(fmt.Errorf("failed begin transaction: %w", err))
	}
	defer model.CheckRollback(ctx, tx)
	ctx = context.WithValue(ctx, model.ContextKeyTx, tx)

	exists, err := s.userService.CheckUserWithEmailExists(ctx, data.Email)
	if err != nil {
		return nil, model.NewError(err)
	}
	if exists {
		return nil, model.AuthErrorEmailAlreadyUsed()
	}

	hashedPassword, err := util.Hash(data.Password)
	if err != nil {
		return nil, model.NewError(fmt.Errorf("failed to hash password: %w", err))
	}

	user := &model.User{
		Email:        data.Email,
		PasswordHash: hashedPassword,
		Name:         data.Name,
	}
	user, err = s.userService.CreateUser(ctx, user)
	if err != nil {
		return nil, model.NewError(fmt.Errorf("failed create user for sign up: %w", err))
	}

	tokens, err := s.genTokens(user.ID)
	if err != nil {
		return nil, model.NewError(err)
	}

	_, err = s.saveRefreshToken(ctx, tokens.RefreshToken.UserID, tokens.RefreshToken.Token, tokens.RefreshToken.ExpiresAt)
	if err != nil {
		return nil, model.NewError(fmt.Errorf("failed to save refresh token: %w", err))
	}

	err = tx.Commit(ctx)
	if err != nil {
		return nil, model.NewError(fmt.Errorf("failed to commit transaction: %w", err))
	}

	return &model.SuccessSignUpResult{
		User: user,
		Tokens: &model.Tokens{
			AccessToken:  tokens.AccessToken.Token,
			RefreshToken: tokens.RefreshToken.Token,
		},
	}, nil
}

func (s *AuthService) saveRefreshToken(ctx context.Context, userID uuid.UUID, token string, expiresAt time.Time) (refreshToken *model.RefreshToken, err error) {
	hashedToken, err := util.HashSHA256(token, s.cfg.RefreshTokenHashSecretKey)
	if err != nil {
		return nil, fmt.Errorf("failed to hash token: %w", err)
	}

	refreshToken = &model.RefreshToken{
		UserID:    userID,
		TokenHash: hashedToken,
		ExpiresAt: expiresAt,
	}

	refreshToken, err = s.authRepo.CreateToken(ctx, refreshToken)
	if err != nil {
		return nil, fmt.Errorf("failed to create token: %w", err)
	}

	return refreshToken, nil
}

func (s *AuthService) LogIn(ctx context.Context, data *model.LogInInput) (*model.SuccessLogInResult, error) {
	err := data.Validate()
	if err != nil {
		return nil, model.NewError(err)
	}

	tx, err := s.tx.Begin(ctx)
	if err != nil {
		return nil, model.NewError(fmt.Errorf("failed begin transaction: %w", err))
	}
	defer model.CheckRollback(ctx, tx)
	ctx = context.WithValue(ctx, model.ContextKeyTx, tx)

	user, err := s.userService.GetUserByEmail(ctx, data.Email)
	if err != nil {
		return nil, model.NewError(err)
	}

	matches, err := util.CompareHash(data.Password, user.PasswordHash)
	if err != nil {
		return nil, model.NewError(fmt.Errorf("failed to compare hash: %w", err))
	}

	if !matches {
		return nil, model.AuthErrorIncorrectPassword()
	}

	tokens, err := s.genTokens(user.ID)
	if err != nil {
		return nil, model.NewError(err)
	}

	_, err = s.saveRefreshToken(ctx, tokens.RefreshToken.UserID, tokens.RefreshToken.Token, tokens.RefreshToken.ExpiresAt)
	if err != nil {
		return nil, model.NewError(fmt.Errorf("failed to save refresh token: %w", err))
	}

	err = tx.Commit(ctx)
	if err != nil {
		return nil, model.NewError(fmt.Errorf("failed to commit transaction: %w", err))
	}

	return &model.SuccessLogInResult{
		User: user,
		Tokens: &model.Tokens{
			AccessToken:  tokens.AccessToken.Token,
			RefreshToken: tokens.RefreshToken.Token,
		},
	}, nil
}

func (s *AuthService) RefreshTokens(ctx context.Context, tokenString string) (*model.Tokens, error) {
	tokenInfo, err := util.ParseJWTToken(tokenString, s.cfg.RefreshTokenJWTSecretKey)
	if errors.Is(err, util.ErrTokenExpired) {
		return nil, model.AuthErrorTokenExpired()
	}
	if err != nil {
		return nil, model.AuthErrorInvalidToken()
	}

	hash, err := util.HashSHA256(tokenString, s.cfg.RefreshTokenHashSecretKey)
	if err != nil {
		return nil, model.NewError(fmt.Errorf("failed to hash token: %w", err))
	}

	tx, err := s.tx.Begin(ctx)
	if err != nil {
		return nil, model.NewError(fmt.Errorf("failed begin transaction: %w", err))
	}
	defer model.CheckRollback(ctx, tx)
	ctx = context.WithValue(ctx, model.ContextKeyTx, tx)

	// try get not used token for user by hash
	refreshToken, err := s.authRepo.GetNotUsedTokenByUserIDAndHash(ctx, tokenInfo.UserID, hash)
	if errors.Is(err, repository.ErrNotFound) {
		return nil, model.AuthErrorInvalidToken()
	}
	if err != nil {
		return nil, model.NewError(fmt.Errorf("failed to hash token: %w", err))
	}

	// set token used
	err = s.authRepo.SetTokenUsedByID(ctx, refreshToken.ID)
	if errors.Is(err, repository.ErrNoAffectedRows) {
		return nil, model.AuthErrorInvalidToken()
	}
	if err != nil {
		return nil, model.NewError(fmt.Errorf("failed to set token used by id: %w", err))
	}

	tokens, err := s.genTokens(tokenInfo.UserID)
	if err != nil {
		return nil, model.NewError(err)
	}

	_, err = s.saveRefreshToken(ctx, tokens.RefreshToken.UserID, tokens.RefreshToken.Token, tokens.RefreshToken.ExpiresAt)
	if err != nil {
		return nil, model.NewError(fmt.Errorf("failed to save refresh token: %w", err))
	}

	err = tx.Commit(ctx)
	if err != nil {
		return nil, model.NewError(fmt.Errorf("failed to commit transaction: %w", err))
	}

	return &model.Tokens{
		AccessToken:  tokens.AccessToken.Token,
		RefreshToken: tokens.RefreshToken.Token,
	}, nil
}

func (s *AuthService) genTokens(userID uuid.UUID) (*model.TokensWithInfo, error) {
	accessToken, err := util.GenAccessToken(userID, s.cfg.AccessTokenJWTSecretKey)
	if err != nil {
		return nil, fmt.Errorf("failed to gen access token: %w", err)
	}
	refreshToken, err := util.GenRefreshToken(userID, s.cfg.RefreshTokenJWTSecretKey)
	if err != nil {
		return nil, fmt.Errorf("failed to gen refresh token: %w", err)
	}
	return &model.TokensWithInfo{
		AccessToken:  accessToken,
		RefreshToken: refreshToken,
	}, nil
}
