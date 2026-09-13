package rest

import (
	"fmt"
	"ftns-asst/internal/model"
	"ftns-asst/internal/rest/dto"
	"ftns-asst/internal/service/auth"
	"ftns-asst/internal/service/users"
	"net/http"

	"github.com/gin-gonic/gin"
)

type AuthHandler struct {
	authService *auth.AuthService
	userService *users.UserService
}

func NewAuthHandler(authService *auth.AuthService, userService *users.UserService) *AuthHandler {
	return &AuthHandler{
		authService: authService,
		userService: userService,
	}
}

func (h *AuthHandler) RegisterRoutes(r *gin.RouterGroup) {
	r.POST("/auth/check-email", h.CheckEmail)
}

// CheckEmail godoc
// @Id checkEmail
// @Tags auth
// @Summary Check if email is already taken
// @Accept json
// @Produce json
// @Param request body dto.CheckEmailRequestBody true "CheckEmail body"
// @Success 200 {object} dto.CheckEmailResponse
// @Failure      400  {object}  map[string]any
// @Failure      404  {object}  map[string]any
// @Failure      500  {object}  map[string]any
// @Router /auth/check-email [post]
func (h *AuthHandler) CheckEmail(c *gin.Context) {
	var body dto.CheckEmailRequestBody
	err := c.ShouldBindJSON(&body)
	if err != nil {
		handleError(c, fmt.Errorf("%w:%w", err, model.ErrBadRequest))
		return
	}

	exists, err := h.userService.CheckUserWithEmailExists(c.Request.Context(), body.Email)
	if err != nil {
		handleError(c, err)
	}

	c.JSON(http.StatusOK, dto.CheckEmailResponse{
		Free: !exists,
	})
}
