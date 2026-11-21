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
	ProgramReadOnlyKey               = "program_read_only"
	ProgramInvalidStateTransitionKey = "invalid_state_transition"
	ProgramIncompleteKey             = "program_incomplete"
	ProgramNotDraftKey               = "program_not_draft"
	ProgramExerciseNotAvailableKey   = "exercise_not_available"
	ProgramDaysCountMismatchKey      = "days_count_mismatch"
	ProgramMeasurementRequiredKey    = "measurement_required"
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

func ProgramReadOnly() *ServiceError {
	return &ServiceError{
		Err: fmt.Errorf("program is finished and read only: %w", ErrBadRequest),
		Key: ProgramReadOnlyKey,
	}
}

func ProgramInvalidStateTransition(from ProgramState, to ProgramState) *ServiceError {
	return &ServiceError{
		Err: fmt.Errorf("transition from %s to %s is not allowed: %w", from, to, ErrBadRequest),
		Key: ProgramInvalidStateTransitionKey,
	}
}

func ProgramIncomplete() *ServiceError {
	return &ServiceError{
		Err: fmt.Errorf("program must have scheduled days and each of them must have exercises: %w", ErrBadRequest),
		Key: ProgramIncompleteKey,
	}
}

func ProgramNotDraft() *ServiceError {
	return &ServiceError{
		Err: fmt.Errorf("only draft program can be deleted: %w", ErrBadRequest),
		Key: ProgramNotDraftKey,
	}
}

func ProgramExerciseNotAvailable() *ServiceError {
	return &ServiceError{
		Err: fmt.Errorf("exercise is not found, deleted or not available for user: %w", ErrBadRequest),
		Key: ProgramExerciseNotAvailableKey,
	}
}

func ProgramMeasurementRequired() *ServiceError {
	return &ServiceError{
		Err: fmt.Errorf("personal records are required in exercises with percent load before start: %w", ErrBadRequest),
		Key: ProgramMeasurementRequiredKey,
	}
}

func ProgramDaysCountMismatch(want int, got int) *ServiceError {
	return &ServiceError{
		Err: fmt.Errorf("program requires %d days per week, got %d: %w", want, got, ErrBadRequest),
		Key: ProgramDaysCountMismatchKey,
	}
}

func ExercisesFilterUserIDRequired() *ServiceError {
	return &ServiceError{
		Err: fmt.Errorf("user id is required for filter: %w", ErrBadRequest),
		Key: ExercisesFilterUserIDRequiredKey,
	}
}
