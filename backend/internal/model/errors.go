package model

import (
	"errors"
	"fmt"
)

var (
	ErrUnauthorized       = errors.New("error unauthorized")
	ErrNotFound           = errors.New("error not found")
	ErrBadRequest         = errors.New("error bad request")
	ErrServiceUnavailable = errors.New("error service unavailable")
	ErrForbidden          = errors.New("error forbidden")
)

type ServiceError struct {
	Err error
	Key string
}

func (e *ServiceError) Error() string {
	return e.Err.Error()
}
func (e *ServiceError) Unwrap() error {
	return e.Err
}

const (
	DefaultErrorKey                  = "undefined_error"
	AuthErrorIncorrectPasswordKey    = "incorrect_password"
	AuthErrorEmailAlreadyUsedKey     = "email_already_used"
	AuthErrorInvalidTokenKey         = "invalid_token"
	AuthErrorTokenExpiredKey         = "token_expired"
	AuthRecoveryCodeInvalidKey       = "recovery_code_invalid"
	AuthRecoveryCodeExpiredKey       = "recovery_code_expired"
	AuthResetTokenInvalidKey         = "reset_token_invalid"
	AuthResetTokenExpiredKey         = "reset_token_expired"
	ExercisesFilterUserIDRequiredKey = "user_id_required_for_filter"
)

func NewError(err error) *ServiceError {
	return &ServiceError{
		Err: err,
		Key: DefaultErrorKey,
	}
}

func AuthErrorIncorrectPassword() *ServiceError {
	return &ServiceError{
		Err: fmt.Errorf("incorrect password: %w", ErrBadRequest),
		Key: AuthErrorIncorrectPasswordKey,
	}
}

func AuthErrorEmailAlreadyUsed() *ServiceError {
	return &ServiceError{
		Err: fmt.Errorf("email already used: %w", ErrBadRequest),
		Key: AuthErrorEmailAlreadyUsedKey,
	}
}

func AuthErrorInvalidToken() *ServiceError {
	return &ServiceError{
		Err: fmt.Errorf("invalid token: %w", ErrUnauthorized),
		Key: AuthErrorInvalidTokenKey,
	}
}

func AuthErrorTokenExpired() *ServiceError {
	return &ServiceError{
		Err: fmt.Errorf("expired token: %w", ErrUnauthorized),
		Key: AuthErrorTokenExpiredKey,
	}
}

func AuthErrorRecoveryCodeInvalid() *ServiceError {
	return &ServiceError{
		Err: fmt.Errorf("invalid recovery code: %w", ErrBadRequest),
		Key: AuthRecoveryCodeInvalidKey,
	}
}

func AuthErrorRecoveryCodeExpired() *ServiceError {
	return &ServiceError{
		Err: fmt.Errorf("expired recovery code: %w", ErrBadRequest),
		Key: AuthRecoveryCodeExpiredKey,
	}
}

func AuthErrorResetTokenInvalid() *ServiceError {
	return &ServiceError{
		Err: fmt.Errorf("invalid reset token: %w", ErrBadRequest),
		Key: AuthResetTokenInvalidKey,
	}
}

func AuthErrorResetTokenExpired() *ServiceError {
	return &ServiceError{
		Err: fmt.Errorf("expired reset token: %w", ErrBadRequest),
		Key: AuthResetTokenExpiredKey,
	}
}

func ExercisesFilterUserIDRequired() *ServiceError {
	return &ServiceError{
		Err: fmt.Errorf("user id is required for filter: %w", ErrBadRequest),
		Key: ExercisesFilterUserIDRequiredKey,
	}
}
