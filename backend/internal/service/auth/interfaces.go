package auth

import (
	"context"
	"ftns-asst/internal/model"

	"github.com/google/uuid"
)

type (
	AuthRepo interface {
		CreateResetToken(ctx context.Context, token *model.ResetToken) (*model.ResetToken, error)
		CreateToken(ctx context.Context, token *model.RefreshToken) (*model.RefreshToken, error)
		CreateVerificationCodeAndSetOtherExpired(ctx context.Context, code *model.VerificationCode) (*model.VerificationCode, error)
		GetCodeNotUsedByUserID(ctx context.Context, userID uuid.UUID) (*model.VerificationCode, error)
		GetNotUsedResetTokenByHash(ctx context.Context, hash string) (*model.ResetToken, error)
		GetNotUsedTokenByUserIDAndHash(ctx context.Context, userID uuid.UUID, hash string) (*model.RefreshToken, error)
		SetCodeUsedByID(ctx context.Context, id uuid.UUID) error
		SetResetTokenUsedByID(ctx context.Context, id uuid.UUID) error
		SetTokenUsedByID(ctx context.Context, id uuid.UUID) error
	}

	UserProvider interface {
		CheckUserWithEmailExists(ctx context.Context, email string) (exists bool, err error)
		CreateUser(ctx context.Context, user *model.User) (*model.User, error)
		GetUserByEmail(ctx context.Context, email string) (res *model.User, err error)
		GetUserByID(ctx context.Context, id uuid.UUID, withProfile bool) (*model.UserWithProfile, error)
	}
)
