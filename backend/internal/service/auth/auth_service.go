package auth

import (
	"ftns-asst/internal/config"
	"ftns-asst/internal/model"
	"ftns-asst/internal/service/users"
)

type AuthService struct {
	cfg         *config.Config
	userService *users.UserService
	tx          model.Transactor
}

func NewService(cfg *config.Config,
	userService *users.UserService,
	transactor model.Transactor,
) *AuthService {
	return &AuthService{
		cfg:         cfg,
		userService: userService,
		tx:          transactor,
	}
}
