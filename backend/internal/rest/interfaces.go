package rest

import (
	"context"
	"ftns-asst/internal/model"

	"github.com/google/uuid"
)

type (
	AuthService interface {
		LogIn(ctx context.Context, data *model.LogInInput) (*model.SuccessLogInResult, error)
		RefreshTokens(ctx context.Context, tokenString string) (*model.Tokens, error)
		ResetPassword(ctx context.Context, newPassword string, resetToken string) (res *model.SuccessResetPasswordResult, err error)
		SignUp(ctx context.Context, data *model.SignUpInput) (*model.SuccessSignUpResult, error)
		StartPasswordRecovery(ctx context.Context, email string) error
		VerifyCode(ctx context.Context, email string, codeValue string) (resetToken string, err error)
	}
	UserService interface {
		CheckUserWithEmailExists(ctx context.Context, email string) (exists bool, err error)
		CreateUser(ctx context.Context, user *model.User) (*model.User, error)
		GetUserByEmail(ctx context.Context, email string) (res *model.User, err error)
		GetUserByID(ctx context.Context, id uuid.UUID, withProfile bool) (*model.UserWithProfile, error)
		UpdateUserProfile(ctx context.Context, userID uuid.UUID, input *model.UpdateUserProfileInput) (*model.UserProfile, error)
	}
	ExerciseService interface {
		GetMuscleGroups() []model.MuscleGroup
		GetEquipment() []model.Equipment
		CreateExercise(ctx context.Context, userID uuid.UUID, exercise *model.Exercise) (*model.Exercise, error)
		GetExerciseByID(ctx context.Context, userID uuid.UUID, exerciseID uuid.UUID) (*model.Exercise, error)
		GetExercises(ctx context.Context, filters *model.ExerciseFilters, page *model.PageInfo) ([]*model.Exercise, error)
		UpdateExercise(ctx context.Context, userID uuid.UUID, input *model.UpdateExerciseInput) (*model.Exercise, error)
		DeleteExercise(ctx context.Context, userID uuid.UUID, id uuid.UUID) error
	}
)
