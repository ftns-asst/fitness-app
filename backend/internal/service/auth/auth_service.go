package auth

import (
	"context"
	"crypto/rand"
	"errors"
	"fmt"
	"ftns-asst/internal/config"
	"ftns-asst/internal/model"
	"ftns-asst/internal/repository"
	"ftns-asst/internal/service/users"
	"ftns-asst/internal/util"
	"log"
	"math/big"
	"time"

	"github.com/google/uuid"
)

type AuthService struct {
	cfg         *config.Config
	userService UserProvider
	authRepo    AuthRepo
	mailer      model.Mailer
	tx          model.Transactor
}

func NewService(cfg *config.Config,
	userService *users.UserService,
	authRepo AuthRepo,
	mailer model.Mailer,
	transactor model.Transactor,
) *AuthService {
	return &AuthService{
		cfg:         cfg,
		userService: userService,
		authRepo:    authRepo,
		mailer:      mailer,
		tx:          transactor,
	}
}

func (s *AuthService) SignUp(ctx context.Context, data *model.SignUpInput) (*model.SuccessSignUpResult, error) {
	err := data.Validate()
	if err != nil {
		return nil, err
	}

	tx, err := s.tx.Begin(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed begin transaction: %w", err)
	}
	defer model.CheckRollback(ctx, tx)
	ctx = context.WithValue(ctx, model.ContextKeyTx, tx)

	exists, err := s.userService.CheckUserWithEmailExists(ctx, data.Email)
	if err != nil {
		return nil, err
	}
	if exists {
		return nil, model.AuthErrorEmailAlreadyUsed()
	}

	hashedPassword, err := util.Hash(data.Password)
	if err != nil {
		return nil, fmt.Errorf("failed to hash password: %w", err)
	}

	user := &model.User{
		Email:        data.Email,
		PasswordHash: hashedPassword,
		Name:         data.Name,
	}
	user, err = s.userService.CreateUser(ctx, user)
	if err != nil {
		return nil, fmt.Errorf("failed create user for sign up: %w", err)
	}

	tokens, err := s.genTokens(user.ID)
	if err != nil {
		return nil, err
	}

	_, err = s.saveRefreshToken(ctx, tokens.RefreshToken.UserID, tokens.RefreshToken.Token, tokens.RefreshToken.ExpiresAt)
	if err != nil {
		return nil, fmt.Errorf("failed to save refresh token: %w", err)
	}

	err = tx.Commit(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed to commit transaction: %w", err)
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
		return nil, err
	}

	tx, err := s.tx.Begin(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed begin transaction: %w", err)
	}
	defer model.CheckRollback(ctx, tx)
	ctx = context.WithValue(ctx, model.ContextKeyTx, tx)

	user, err := s.userService.GetUserByEmail(ctx, data.Email)
	if err != nil {
		return nil, err
	}

	matches, err := util.CompareHash(data.Password, user.PasswordHash)
	if err != nil {
		return nil, fmt.Errorf("failed to compare hash: %w", err)
	}

	if !matches {
		return nil, model.AuthErrorIncorrectPassword()
	}

	tokens, err := s.genTokens(user.ID)
	if err != nil {
		return nil, err
	}

	_, err = s.saveRefreshToken(ctx, tokens.RefreshToken.UserID, tokens.RefreshToken.Token, tokens.RefreshToken.ExpiresAt)
	if err != nil {
		return nil, fmt.Errorf("failed to save refresh token: %w", err)
	}

	err = tx.Commit(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed to commit transaction: %w", err)
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
		return nil, fmt.Errorf("failed to hash token: %w", err)
	}

	tx, err := s.tx.Begin(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed begin transaction: %w", err)
	}
	defer model.CheckRollback(ctx, tx)
	ctx = context.WithValue(ctx, model.ContextKeyTx, tx)

	// try get not used token for user by hash
	refreshToken, err := s.authRepo.GetNotUsedTokenByUserIDAndHash(ctx, tokenInfo.UserID, hash)
	if errors.Is(err, repository.ErrNotFound) {
		return nil, model.AuthErrorInvalidToken()
	}
	if err != nil {
		return nil, fmt.Errorf("failed to hash token: %w", err)
	}

	// set token used
	err = s.authRepo.SetTokenUsedByID(ctx, refreshToken.ID)
	if errors.Is(err, repository.ErrNoAffectedRows) {
		return nil, model.AuthErrorInvalidToken()
	}
	if err != nil {
		return nil, fmt.Errorf("failed to set token used by id: %w", err)
	}

	tokens, err := s.genTokens(tokenInfo.UserID)
	if err != nil {
		return nil, err
	}

	_, err = s.saveRefreshToken(ctx, tokens.RefreshToken.UserID, tokens.RefreshToken.Token, tokens.RefreshToken.ExpiresAt)
	if err != nil {
		return nil, fmt.Errorf("failed to save refresh token: %w", err)
	}

	err = tx.Commit(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed to commit transaction: %w", err)
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

func genNumericCode() (string, error) {
	max := big.NewInt(999999)
	value, err := rand.Int(rand.Reader, max)
	if err != nil {
		return "", err
	}
	return fmt.Sprintf("%0*d", 6, value), nil
}

func genResetToken() string {
	return util.RandStr()
}

func (s *AuthService) StartPasswordRecovery(ctx context.Context, email string) error {
	tx, err := s.tx.Begin(ctx)
	if err != nil {
		return fmt.Errorf("failed begin transaction: %w", err)
	}
	defer model.CheckRollback(ctx, tx)
	ctx = context.WithValue(ctx, model.ContextKeyTx, tx)

	user, err := s.userService.GetUserByEmail(ctx, email)
	if errors.Is(err, repository.ErrNotFound) {
		return fmt.Errorf("not found user with email: %w", model.ErrNotFound)
	}
	if err != nil {
		return fmt.Errorf("failed to get user by email: %w", model.ErrNotFound)
	}

	codeValue, err := genNumericCode()
	if err != nil {
		return fmt.Errorf("failed to gen code: %w", err)
	}

	hashedCode, err := util.HashSHA256(codeValue, s.cfg.VerificationCodeHashSecretKey)
	if err != nil {
		return fmt.Errorf("failed to hash code: %w", err)
	}

	_, err = s.authRepo.CreateVerificationCodeAndSetOtherExpired(ctx,
		&model.VerificationCode{
			UserID:    user.ID,
			CodeHash:  hashedCode,
			ExpiresAt: time.Now().Add(model.VerificationCodeExpireTime),
		},
	)
	if err != nil {
		return fmt.Errorf("failed to save new verification code: %w", err)
	}

	err = tx.Commit(ctx)
	if err != nil {
		return fmt.Errorf("failed to commit transaction: %w", err)
	}

	// async email sending
	go func() {
		err := s.mailer.SendEmail(email, "Код подтверждения", fmt.Sprintf("Ваш код подтверждения: %s", codeValue))
		if err != nil {
			log.Printf("failed to send email for password recovery: %s", err.Error())
		}
	}()

	return nil
}

func (s *AuthService) VerifyCode(ctx context.Context, email string, codeValue string) (resetToken string, err error) {
	tx, err := s.tx.Begin(ctx)
	if err != nil {
		return "", fmt.Errorf("failed begin transaction: %w", err)
	}
	defer model.CheckRollback(ctx, tx)
	ctx = context.WithValue(ctx, model.ContextKeyTx, tx)

	user, err := s.userService.GetUserByEmail(ctx, email)
	if errors.Is(err, repository.ErrNotFound) {
		return "", fmt.Errorf("not found user with email: %w", model.ErrNotFound)
	}
	if err != nil {
		return "", fmt.Errorf("failed to get user by email: %w", model.ErrNotFound)
	}

	code, err := s.authRepo.GetCodeNotUsedByUserID(ctx, user.ID)
	if errors.Is(err, repository.ErrNotFound) {
		return "", model.AuthErrorRecoveryCodeInvalid()
	}
	if err != nil {
		return "", fmt.Errorf("failed to get code from repo: %w", err)
	}

	if code.ExpiresAt.Before(time.Now()) {
		return "", model.AuthErrorRecoveryCodeExpired()
	}

	inputCodeHash, err := util.HashSHA256(codeValue, s.cfg.VerificationCodeHashSecretKey)
	if err != nil {
		return "", fmt.Errorf("failed to hash input code: %w", err)
	}

	if code.CodeHash != inputCodeHash {
		return "", model.AuthErrorRecoveryCodeInvalid()
	}

	err = s.authRepo.SetCodeUsedByID(ctx, code.ID)
	if err != nil {
		return "", fmt.Errorf("failed to set code used by ID: %w", err)
	}

	tokenValue := genResetToken()
	hashedToken, err := util.HashSHA256(tokenValue, s.cfg.ResetTokenHashSecretKey)
	if err != nil {
		return "", fmt.Errorf("failed to hash token: %w", err)
	}
	resetPasswordToken := &model.ResetToken{
		UserID:    user.ID,
		TokenHash: hashedToken,
		ExpiresAt: time.Now().Add(model.ResetTokenExpireTime),
	}

	// save token
	_, err = s.authRepo.CreateResetToken(ctx, resetPasswordToken)
	if err != nil {
		return "", fmt.Errorf("failed to save reset token: %w", err)
	}

	err = tx.Commit(ctx)
	if err != nil {
		return "", fmt.Errorf("failed to commit transaction: %w", err)
	}

	return tokenValue, nil
}

func (s *AuthService) ResetPassword(ctx context.Context, newPassword string, resetToken string) (res *model.SuccessResetPasswordResult, err error) {
	tx, err := s.tx.Begin(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed begin transaction: %w", err)
	}
	defer model.CheckRollback(ctx, tx)
	ctx = context.WithValue(ctx, model.ContextKeyTx, tx)

	hashedToken, err := util.HashSHA256(resetToken, s.cfg.ResetTokenHashSecretKey)
	if err != nil {
		return nil, fmt.Errorf("failed to hash input code: %w", err)
	}

	token, err := s.authRepo.GetNotUsedResetTokenByHash(ctx, hashedToken)
	if errors.Is(err, repository.ErrNotFound) {
		return nil, model.AuthErrorResetTokenInvalid()
	}
	if err != nil {
		return nil, fmt.Errorf("failed to reset token repo: %w", err)
	}

	if token.ExpiresAt.Before(time.Now()) {
		return nil, model.AuthErrorResetTokenExpired()
	}

	user, err := s.userService.GetUserByID(ctx, token.UserID, false)
	if errors.Is(err, repository.ErrNotFound) {
		return nil, model.AuthErrorResetTokenInvalid()
	}
	if err != nil {
		return nil, fmt.Errorf("failed to get user by ID: %w", err)
	}

	err = s.authRepo.SetResetTokenUsedByID(ctx, token.ID)
	if err != nil {
		return nil, fmt.Errorf("failed to mark reset token used by ID: %w", err)
	}

	tokens, err := s.genTokens(token.UserID)
	if err != nil {
		return nil, err
	}

	_, err = s.saveRefreshToken(ctx, tokens.RefreshToken.UserID, tokens.RefreshToken.Token, tokens.RefreshToken.ExpiresAt)
	if err != nil {
		return nil, fmt.Errorf("failed to save refresh token: %w", err)
	}

	err = tx.Commit(ctx)
	if err != nil {
		return nil, fmt.Errorf("failed to commit transaction: %w", err)
	}

	return &model.SuccessResetPasswordResult{
		User: &user.User,
		Tokens: &model.Tokens{
			AccessToken:  tokens.AccessToken.Token,
			RefreshToken: tokens.RefreshToken.Token,
		},
	}, nil
}
