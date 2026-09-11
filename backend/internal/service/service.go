package service

import (
	"ftns-asst/internal/config"
	"ftns-asst/internal/service/auth"
	"ftns-asst/internal/service/users"
)

// all services
type Services struct {
	userService users.UserService
	authService auth.AuthService
}

func NewServices(cfg *config.Config) *Services {
	return &Services{}
}
