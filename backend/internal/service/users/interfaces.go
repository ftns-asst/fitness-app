package users

import (
	"context"
	"ftns-asst/internal/model"

	"github.com/google/uuid"
)

type (
	UserRepo interface {
		CheckUserWithEmailExists(ctx context.Context, email string) (exists bool, err error)
		CreateOrUpdateUserProfile(ctx context.Context, userID uuid.UUID, input *model.UpdateUserProfileInput) (*model.UserProfile, error)
		CreateUser(ctx context.Context, user *model.User) (*model.User, error)
		GetUserByEmail(ctx context.Context, email string) (*model.User, error)
		GetUserByID(ctx context.Context, id uuid.UUID) (*model.User, error)
		GetUserListByIDs(ctx context.Context, ids []uuid.UUID) ([]*model.User, error)
		GetUserWithProfileByID(ctx context.Context, id uuid.UUID) (*model.UserWithProfile, error)
	}
)
