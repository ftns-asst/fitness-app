package model

import (
	"fmt"
	"strings"
	"time"

	"github.com/google/uuid"
)

const (
	AccessTokenExpireTime  time.Duration = 30 * time.Minute
	RefreshTokenExpireTime time.Duration = 10 * 24 * time.Hour

	VerificationCodeExpireTime time.Duration = 10 * time.Minute
)

// jwt authentication tokens
type Tokens struct {
	AccessToken  string
	RefreshToken string
}
type TokensWithInfo struct {
	AccessToken  *TokenInfo
	RefreshToken *TokenInfo
}

type TokenInfo struct {
	Token     string
	UserID    uuid.UUID
	ExpiresAt time.Time
}

type Token struct {
	ID        uuid.UUID  `db:"id"`
	UserID    uuid.UUID  `db:"user_id"`
	TokenHash string     `db:"token_hash"`
	UsedAt    *time.Time `db:"used_at"`
	ExpiresAt time.Time  `db:"expires_at"`
}

type RefreshToken Token

// password recovery token
type ResetToken Token

const ValidPasswordChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()-_=+[]{}|;:,.<>?/"

type AuthInput struct {
	Email    string
	Password string
}

// validate auth input, email checked in gin validation
func (i *AuthInput) Validate() error {
	if strings.TrimSpace(i.Email) == "" || strings.TrimSpace(i.Password) == "" {
		return fmt.Errorf("email or password must be not blank: %w", ErrBadRequest)
	}
	if len(i.Password) < 8 {
		return fmt.Errorf("password must be at least 8 characters long: %w", ErrBadRequest)
	}
	if len(i.Password) > 64 {
		return fmt.Errorf("password must be at most 64 characters long: %w", ErrBadRequest)
	}

	for _, c := range i.Password {
		if !strings.ContainsRune(ValidPasswordChars, c) {
			return fmt.Errorf("password contains invalid character: %w", ErrBadRequest)
		}
	}

	return nil
}

// input for user registration
type SignUpInput struct {
	AuthInput
	Name string
}

func (i *SignUpInput) Validate() error {
	if err := i.AuthInput.Validate(); err != nil {
		return err
	}
	if strings.TrimSpace(i.Name) == "" {
		return fmt.Errorf("user name must be not blank: %w", ErrBadRequest)
	}
	return nil
}

type SuccessSignUpResult struct {
	User   *User
	Tokens *Tokens
}

// input for user login
type LogInInput struct {
	AuthInput
}

type SuccessLogInResult struct {
	User   *User
	Tokens *Tokens
}

// password recovery result status
type PasswordRecoveryStatus string

const (
	PasswordRecoveryStatusSuccess   PasswordRecoveryStatus = "success"
	PasswordRecoveryStatusIncorrect PasswordRecoveryStatus = "incorrect"
	PasswordRecoveryStatusExpired   PasswordRecoveryStatus = "expired"
)

// verification code for password recovery
type VerificationCode struct {
	ID        uuid.UUID  `db:"id"`
	UserID    uuid.UUID  `db:"user_id"`
	Code      string     `db:"code"`
	UsedAt    *time.Time `db:"used_at"`
	ExpiresAt time.Time  `db:"expires_at"`
}
