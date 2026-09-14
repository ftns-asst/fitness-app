package rest

import (
	"fmt"
	"ftns-asst/internal/model"
	"ftns-asst/internal/rest/dto"
	"ftns-asst/internal/service/users"
	"net/http"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

type UserHandler struct {
	userService *users.UserService
}

func NewUserHandler(userService users.UserService) *UserHandler {
	return &UserHandler{
		userService: &userService,
	}
}

func (h *UserHandler) RegisterRoutes(r *gin.RouterGroup) {
	r.GET("/users/:id", h.GetUserByID)
}

// GetUserByID godoc
// @Id getUserByID
// @Tags users
// @Summary Get user by ID
// @Accept json
// @Produce json
// @Param id path uuid.UUID true "user id path param"
// @Param withProfile query bool false "if true - resposne with user profile"
// @Success 200 {object} dto.UserWithProfileResponse
// @Failure      400  {object}  ErrorResponse
// @Failure      404  {object}  ErrorResponse
// @Failure      500  {object}  ErrorResponse
// @Router /users/{id} [get]
func (h *UserHandler) GetUserByID(c *gin.Context) {
	id, err := uuid.Parse(c.Param("id"))
	if err != nil {
		handleError(c, fmt.Errorf("failed to parse id param: %w: %w", err, model.ErrBadRequest))
	}

	var p dto.GetUserQueryParams
	err = c.ShouldBindUri(&p)
	if err != nil {
		handleError(c, fmt.Errorf("failed to get query params: %w: %w", err, model.ErrBadRequest))
	}

	user, err := h.userService.GetUserByID(c.Request.Context(), id, p.WithProfile)
	if err != nil {
		handleError(c, err)
	}

	c.JSON(http.StatusOK, user)
}
