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

func (h *Handlers) RegisterRoutes(r *gin.RouterGroup) {
	h.auth.RegisterRoutes(r)
}

type ErrorResponse struct {
	Key     string `json:"key"`
	Message string `json:"message"`
}

// func handleError(c *gin.Context, err error) {
// 	handleServiceError(c, model.NewError(err))
// }

// write http response based on service error
func handleError(c *gin.Context, err error) {
	serr := &model.ServiceError{}
	if !errors.As(err, &serr) {
		serr = model.NewError(err)
	}
	responseCode := http.StatusInternalServerError
	resp := ErrorResponse{
		Key:     serr.Key,
		Message: "internal server error",
	}

	switch {
	case errors.Is(serr.Err, model.ErrUnauthorized):
		responseCode = http.StatusUnauthorized
	case errors.Is(serr.Err, model.ErrBadRequest):
		responseCode = http.StatusBadRequest
	case errors.Is(serr.Err, model.ErrNotFound):
		responseCode = http.StatusNotFound
	case errors.Is(serr.Err, model.ErrServiceUnavailable):
		responseCode = http.StatusServiceUnavailable
	}

	if responseCode != http.StatusInternalServerError {
		resp.Message = err.Error()
	}

	log.Printf("Error: %#v", err)
	c.JSON(responseCode, resp)
}
