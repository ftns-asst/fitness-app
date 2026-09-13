package rest

import (
	"errors"
	"ftns-asst/internal/model"
	"ftns-asst/internal/service"
	"log"
	"net/http"

	"github.com/gin-gonic/gin"
)

type Handlers struct {
	auth *AuthHandler
}

func newHandlers(services *service.Services) *Handlers {
	return &Handlers{
		auth: NewAuthHandler(services.Auth(), services.User()),
	}
}

// write http response based on service error
func handleError(c *gin.Context, err error) {
	responseCode := http.StatusInternalServerError
	msg := "internal"

	switch {
	case errors.Is(err, model.ErrBadRequest):
		responseCode = http.StatusBadRequest
	case errors.Is(err, model.ErrNotFound):
		responseCode = http.StatusNotFound
	case errors.Is(err, model.ErrServiceUnavailable):
		responseCode = http.StatusServiceUnavailable
	}

	if responseCode != http.StatusInternalServerError {
		msg = err.Error()
	}

	log.Println(err)
	c.JSON(responseCode, gin.H{"error": msg})
}

func (h *Handlers) RegisterRoutes(r *gin.RouterGroup) {
	h.auth.RegisterRoutes(r)
}
