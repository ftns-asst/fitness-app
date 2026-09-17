package service

import (
	"ftns-asst/internal/config"
	"ftns-asst/internal/model"
	"ftns-asst/internal/repository"
	"ftns-asst/internal/service/auth"
	"ftns-asst/internal/service/users"
)

// all services
//
//nolint:unused
type Services struct {
	userService *users.UserService
	authService *auth.AuthService
}

func NewServices(cfg *config.Config, repositories repository.Repositories, mailer model.Mailer, transactor model.Transactor) *Services {
	user := users.NewService(cfg, repositories.User(), transactor)
	auth := auth.NewService(cfg, user, repositories.Auth(), mailer, transactor)
	return &Services{
		userService: user,
		authService: auth,
	}
}

func (s *Services) User() *users.UserService {
	return s.userService
}

func (s *Services) Auth() *auth.AuthService {
	return s.authService
}
