package rest

import (
	"ftns-asst/internal/config"
	"ftns-asst/internal/service"
	"net/http"
)

func NewServer(cfg *config.Config, services service.Services) *http.Server {
	return &http.Server{}
}
