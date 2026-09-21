package rest

import (
	"fmt"
	"ftns-asst/internal/model"
	"ftns-asst/internal/rest/dto"
	"ftns-asst/internal/rest/dto/mapper"
	"ftns-asst/internal/service/users"
	"net/http"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

type UserHandler struct {
	userService *users.UserService
}

func NewUserHandler(userService *users.UserService) *UserHandler {
	return &UserHandler{
		userService: userService,
	}
}

func (h *UserHandler) RegisterRoutes(r *gin.RouterGroup, authMid gin.HandlerFunc) {
	r.GET("/users/:id", authMid, h.GetUserByID)
	r.PATCH("/users/me/profile", authMid, h.UpdateUserProfile)
}

// GetUserByID godoc
// @Id getUserByID
// @Tags users
// @Summary Get user by ID
// @Accept json
// @Produce json
// @Param id path string true "user id" format(uuid)
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
		return
	}

	var p dto.GetUserQueryParams
	err = c.ShouldBindQuery(&p)
	if err != nil {
		handleError(c, fmt.Errorf("failed to get query params: %w: %w", err, model.ErrBadRequest))
		return
	}

	user, err := h.userService.GetUserByID(c.Request.Context(), id, p.WithProfile)
	if err != nil {
		handleError(c, err)
		return
	}

	c.JSON(http.StatusOK, mapper.ConvertUserWithProfileToDTO(user))
}

// UpdateUserProfile godoc
// @Id updateUserProfile
// @Tags users
// @Summary Update user profile
// @Accept json
// @Produce json
// @Param request body dto.UpdateUserProfileRequestBody true "request body"
// @Success 200 {object} dto.UserProfileResponse
// @Failure      400  {object}  ErrorResponse
// @Failure      404  {object}  ErrorResponse
// @Failure      500  {object}  ErrorResponse
// @Router /users/me/profile [patch]
func (h *UserHandler) UpdateUserProfile(c *gin.Context) {
	userID, err := tryGetUserIDFromCtx(c.Request.Context())
	if err != nil {
		handleError(c, err)
		return
	}

	var body dto.UpdateUserProfileRequestBody
	err = c.ShouldBindJSON(&body)
	if err != nil {
		handleError(c, fmt.Errorf("failed to get body from request: %w: %w", err, model.ErrBadRequest))
		return
	}

	profile, err := h.userService.UpdateUserProfile(c.Request.Context(), userID,
		mapper.ConvertUpdateUserProfileInputFromDTO(&body))
	if err != nil {
		handleError(c, err)
		return
	}

	c.JSON(http.StatusOK, mapper.ConvertProfileToDTO(profile))
}
