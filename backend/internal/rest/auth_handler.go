package rest

import (
	"fmt"
	"ftns-asst/internal/model"
	"ftns-asst/internal/rest/dto"
	"ftns-asst/internal/rest/dto/mapper"
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
	r.POST("/auth/signup", h.SignUp)
	r.POST("/auth/login", h.LogIn)
	r.POST("/auth/refresh", h.LogIn)
	
}

// CheckEmail godoc
// @Id checkEmail
// @Tags auth
// @Summary Check if email is already taken
// @Accept json
// @Produce json
// @Param request body dto.CheckEmailRequestBody true "CheckEmail body"
// @Success 200 {object} dto.CheckEmailResponse
// @Failure      400  {object}  ErrorResponse
// @Failure      404  {object}  ErrorResponse
// @Failure      500  {object}  ErrorResponse
// @Router /auth/check-email [post]
func (h *AuthHandler) CheckEmail(c *gin.Context) {
	var body dto.CheckEmailRequestBody
	err := c.ShouldBindJSON(&body)
	if err != nil {
		handleError(c, fmt.Errorf("failed to read request body: %w:%w", err, model.ErrBadRequest))
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

// SignUp godoc
// @Id signUp
// @Tags auth
// @Summary Sign up a new user
// @Accept json
// @Produce json
// @Param request body dto.SignUpRequestBody true "SignUp body"
// @Success 200 {object} dto.SignUpResponse
// @Failure      400  {object}  ErrorResponse
// @Failure      404  {object}  ErrorResponse
// @Failure      500  {object}  ErrorResponse
// @Router /auth/signup [post]
func (h *AuthHandler) SignUp(c *gin.Context) {
	var body dto.SignUpRequestBody
	err := c.ShouldBindJSON(&body)
	if err != nil {
		handleError(c, fmt.Errorf("failed to read request body: %w:%w", err, model.ErrBadRequest))
		return
	}

	result, err := h.authService.SignUp(c.Request.Context(), mapper.ConvertSignUpRequestBodyFromDTO(&body))
	if err != nil {
		handleError(c, err)
		return
	}

	c.JSON(http.StatusOK, mapper.ConvertSignUpResultToDTO(result))
}

// LogIn godoc
// @Id logIn
// @Tags auth
// @Summary Log in an existing user
// @Accept json
// @Produce json
// @Param request body dto.LogInRequestBody true "LogIn body"
// @Success 200 {object} dto.LogInResponse
// @Failure      400  {object}  ErrorResponse
// @Failure      404  {object}  ErrorResponse
// @Failure      500  {object}  ErrorResponse
// @Router /auth/login [post]
func (h *AuthHandler) LogIn(c *gin.Context) {
	var body dto.LogInRequestBody
	err := c.ShouldBindJSON(&body)
	if err != nil {
		handleError(c, fmt.Errorf("failed to read request body: %w:%w", err, model.ErrBadRequest))
		return
	}

	result, err := h.authService.LogIn(c.Request.Context(), mapper.ConvertLogInRequestBodyFromDTO(&body))
	if err != nil {
		handleError(c, err)
		return
	}

	c.JSON(http.StatusOK, mapper.ConvertLogInResultToDTO(result))
}

// RefreshTokens godoc
// @Id refreshTokens
// @Tags auth
// @Summary Refresh authentication tokens
// @Accept json
// @Produce json
// @Param request body dto.RefreshTokensRequestBody true "RefreshTokens body"
// @Success 200 {object} dto.RefreshTokensResponse
// @Failure      400  {object}  ErrorResponse
// @Failure      404  {object}  ErrorResponse
// @Failure      500  {object}  ErrorResponse
// @Router /auth/refresh [post]
func (h *AuthHandler) RefreshTokens(c *gin.Context) {
	var body dto.RefreshTokensRequestBody
	err := c.ShouldBindJSON(&body)
	if err != nil {
		handleError(c, fmt.Errorf("failed to read request body: %w:%w", err, model.ErrBadRequest))
		return
	}

	result, err := h.authService.RefreshTokens(c.Request.Context(), body.RefreshToken)
	if err != nil {
		handleError(c, err)
		return
	}

	c.JSON(http.StatusOK, mapper.ConvertTokensToDTO(result))
}
