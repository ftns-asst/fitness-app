package rest

import (
	"fmt"
	"ftns-asst/internal/model"
	"ftns-asst/internal/rest/dto"
	"ftns-asst/internal/rest/dto/mapper"
	"net/http"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

type ExerciseHandler struct {
	exerciseService ExerciseService
}

func NewExerciseHandler(exerciseService ExerciseService) *ExerciseHandler {
	return &ExerciseHandler{
		exerciseService: exerciseService,
	}
}

func (h *ExerciseHandler) RegisterRoutes(r *gin.RouterGroup, authMid gin.HandlerFunc, optionalAuthMid gin.HandlerFunc) {
	r.GET("/muscle-groups", h.GetMuscleGroups)
	r.GET("/equipments", h.GetEquipments)

	r.GET("/exercises", optionalAuthMid, h.ListExercises)
	r.POST("/exercises", authMid, h.CreateExercise)
	r.GET("/exercises/:id", authMid, h.GetExerciseByID)
	r.PATCH("/exercises/:id", authMid, h.UpdateExercise)
	r.DELETE("/exercises/:id", authMid, h.DeleteExercise)
}

func (h *ExerciseHandler) GetMuscleGroups(c *gin.Context) {
	res := h.exerciseService.GetMuscleGroups()

	c.JSON(http.StatusOK, res)
}

func (h *ExerciseHandler) GetEquipments(c *gin.Context) {
	res := h.exerciseService.GetEquipment()

	c.JSON(http.StatusOK, res)
}

func (h *ExerciseHandler) ListExercises(c *gin.Context) {
	userID, _ := tryGetUserIDFromCtx(c.Request.Context())

	var params dto.ListExerciseQueryParams
	err := c.ShouldBindQuery(&params)
	if err != nil {
		handleError(c, fmt.Errorf("failed to get query params from request: %w: %w", err, model.ErrBadRequest))
		return
	}

	filters, page := mapper.ConvertListExerciseQueryParams(&params, &userID)
	res, err := h.exerciseService.GetExercises(c.Request.Context(),
		filters, page)
	if err != nil {
		handleError(c, err)
		return
	}

	c.JSON(http.StatusOK, res)
}

func (h *ExerciseHandler) CreateExercise(c *gin.Context) {
	userID, err := tryGetUserIDFromCtx(c.Request.Context())
	if err != nil {
		handleError(c, err)
		return
	}

	var body dto.CreateExerciseRequestBody
	err = c.ShouldBindJSON(&body)
	if err != nil {
		handleError(c, fmt.Errorf("failed to get body from request: %w: %w", err, model.ErrBadRequest))
		return
	}

	res, err := h.exerciseService.CreateExercise(c.Request.Context(), userID,
		mapper.ConvertExerciseFromDTO(&body.ExerciseDTO))
	if err != nil {
		handleError(c, err)
		return
	}

	c.JSON(http.StatusOK, mapper.ConvertExerciseToDTO(res))
}

func (h *ExerciseHandler) GetExerciseByID(c *gin.Context) {
	userID, err := tryGetUserIDFromCtx(c.Request.Context())
	if err != nil {
		handleError(c, err)
		return
	}

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

	res, err := h.exerciseService.GetExerciseByID(c.Request.Context(), userID, id)
	if err != nil {
		handleError(c, err)
		return
	}

	c.JSON(http.StatusOK, mapper.ConvertExerciseToDTO(res))
}

func (h *ExerciseHandler) UpdateExercise(c *gin.Context) {
	userID, err := tryGetUserIDFromCtx(c.Request.Context())
	if err != nil {
		handleError(c, err)
		return
	}

	id, err := uuid.Parse(c.Param("id"))
	if err != nil {
		handleError(c, fmt.Errorf("failed to parse id param: %w: %w", err, model.ErrBadRequest))
		return
	}

	var body dto.UpdateExerciseRequestBody
	err = c.ShouldBindJSON(&body)
	if err != nil {
		handleError(c, fmt.Errorf("failed to get body from request: %w: %w", err, model.ErrBadRequest))
		return
	}
	if id != body.ID {
		handleError(c, fmt.Errorf("different 'id' value from param(%s) and body(%s)", id, body.ID))
		return
	}

	res, err := h.exerciseService.UpdateExercise(c.Request.Context(), userID,
		mapper.ConvertUpdateExerciseInputFromDTO(&body))
	if err != nil {
		handleError(c, err)
		return
	}

	c.JSON(http.StatusOK, mapper.ConvertExerciseToDTO(res))
}

func (h *ExerciseHandler) DeleteExercise(c *gin.Context) {
	userID, err := tryGetUserIDFromCtx(c.Request.Context())
	if err != nil {
		handleError(c, err)
		return
	}

	id, err := uuid.Parse(c.Param("id"))
	if err != nil {
		handleError(c, fmt.Errorf("failed to parse id param: %w: %w", err, model.ErrBadRequest))
		return
	}

	err = h.exerciseService.DeleteExercise(c.Request.Context(), userID, id)
	if err != nil {
		handleError(c, err)
		return
	}

	c.Status(http.StatusOK)
}
