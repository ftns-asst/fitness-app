package users

import (
	"context"
	"errors"
	"fmt"
	"ftns-asst/internal/config"
	"ftns-asst/internal/model"
	"ftns-asst/internal/repository"

	"github.com/google/uuid"
)

type UserService struct {
	cfg      *config.Config
	userRepo *repository.UserRepo
	tx       model.Transactor
}

func NewService(cfg *config.Config,
	userRepo *repository.UserRepo,
	transactor model.Transactor,
) *UserService {
	return &UserService{
		cfg:      cfg,
		userRepo: userRepo,
		tx:       transactor,
	}
}

func (u *UserService) CheckUserWithEmailExists(ctx context.Context, email string) (exists bool, err error) {
	exists, err = u.userRepo.CheckUserWithEmailExists(ctx, email)
	if err != nil {
		return false, fmt.Errorf("failed to check user with email exists: %w", err)
	}
	return exists, nil
}

func (s *UserService) GetUserByID(ctx context.Context, id uuid.UUID, withProfile bool) (*model.UserWithProfile, error) {
	var res *model.UserWithProfile
	var err error

	if withProfile {
		res, err = s.userRepo.GetUserWithProfileByID(ctx, id)
	} else {
		var user *model.User
		user, err = s.userRepo.GetUserByID(ctx, id)
		if user == nil {
			user = &model.User{}
		}
		res = &model.UserWithProfile{
			User: *user,
		}
	}

	if errors.Is(err, repository.ErrNotFound) {
		return nil, model.ErrNotFound
	}
	if err != nil {
		return nil, fmt.Errorf("failed to get user by id: %w", err)
	}

	return res, nil
}
