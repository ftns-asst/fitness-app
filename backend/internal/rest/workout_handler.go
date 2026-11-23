package rest

import (
	"fmt"
	"net/http"

	"ftns-asst/internal/model"
	"ftns-asst/internal/rest/dto"
	"ftns-asst/internal/rest/dto/mapper"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

type WorkoutHandler struct {
	service WorkoutService
}

func NewWorkoutHandler(service WorkoutService) *WorkoutHandler {
	return &WorkoutHandler{service: service}
}

func (h *WorkoutHandler) RegisterRoutes(r *gin.RouterGroup, authMid gin.HandlerFunc) {
	workouts := r.Group("/workouts", authMid)
	workouts.PUT("/:id", h.SaveWorkout)
	workouts.GET("/:id", h.GetWorkout)
	workouts.GET("", h.ListWorkouts)
}

// SaveWorkout godoc
// @Id saveWorkout
// @Tags workouts
// @Summary Save a completed workout
// @Description Saves all sets and new personal records atomically. Retrying the same workout returns the existing result. Changing its contents returns 409.
// @Accept json
// @Produce json
// @Security BearerAuth
// @Param id path string true "client-generated workout id" format(uuid)
// @Param request body dto.SaveWorkoutRequestBody true "completed exercises and sets in order"
// @Success 201 {object} dto.WorkoutDTO "Created"
// @Success 200 {object} dto.WorkoutDTO "Already saved"
// @Failure 400 {object} ErrorResponse
// @Failure 401 {object} ErrorResponse
// @Failure 404 {object} ErrorResponse
// @Failure 409 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /workouts/{id} [put]
func (h *WorkoutHandler) SaveWorkout(c *gin.Context) {
	ctx := c.Request.Context()
	userID, err := tryGetUserIDFromCtx(ctx)
	if err != nil {
		handleError(c, err)
		return
	}
	id, err := parseIDParam(c)
	if err != nil {
		handleError(c, err)
		return
	}
	var body dto.SaveWorkoutRequestBody
	if err := c.ShouldBindJSON(&body); err != nil {
		handleError(c, fmt.Errorf("invalid workout body: %w: %w", err, model.ErrBadRequest))
		return
	}
	workout, created, err := h.service.SaveWorkout(ctx, userID, id, mapper.ConvertSaveWorkoutInputFromDTO(&body))
	if err != nil {
		handleError(c, err)
		return
	}
	status := http.StatusOK
	if created {
		status = http.StatusCreated
	}
	c.JSON(status, mapper.ConvertWorkoutToDTO(workout))
}

// GetWorkout godoc
// @Id getWorkout
// @Tags workouts
// @Summary Get a completed workout with exercises and sets
// @Produce json
// @Security BearerAuth
// @Param id path string true "workout id" format(uuid)
// @Success 200 {object} dto.WorkoutDTO
// @Failure 400 {object} ErrorResponse
// @Failure 401 {object} ErrorResponse
// @Failure 404 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /workouts/{id} [get]
func (h *WorkoutHandler) GetWorkout(c *gin.Context) {
	ctx := c.Request.Context()
	userID, err := tryGetUserIDFromCtx(ctx)
	if err != nil {
		handleError(c, err)
		return
	}
	id, err := parseIDParam(c)
	if err != nil {
		handleError(c, err)
		return
	}
	workout, err := h.service.GetWorkout(ctx, userID, id)
	if err != nil {
		handleError(c, err)
		return
	}
	c.JSON(http.StatusOK, mapper.ConvertWorkoutToDTO(workout))
}

// ListWorkouts godoc
// @Id listWorkouts
// @Tags workouts
// @Summary List current user's workouts, newest completion first
// @Produce json
// @Security BearerAuth
// @Param program_id query string false "filter by program id" format(uuid)
// @Param limit query int false "page size" minimum(1) maximum(1000) default(20)
// @Param offset query int false "page offset" minimum(0) default(0)
// @Success 200 {array} dto.WorkoutSummaryDTO
// @Failure 400 {object} ErrorResponse
// @Failure 401 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /workouts [get]
func (h *WorkoutHandler) ListWorkouts(c *gin.Context) {
	ctx := c.Request.Context()
	userID, err := tryGetUserIDFromCtx(ctx)
	if err != nil {
		handleError(c, err)
		return
	}
	var params dto.ListWorkoutsQueryParams
	if err := c.ShouldBindQuery(&params); err != nil {
		handleError(c, fmt.Errorf("invalid workout filters: %w: %w", err, model.ErrBadRequest))
		return
	}
	var programID *uuid.UUID
	if params.ProgramID != nil {
		id, err := uuid.Parse(*params.ProgramID)
		if err != nil {
			handleError(c, fmt.Errorf("invalid program_id: %w: %w", err, model.ErrBadRequest))
			return
		}
		programID = &id
	}
	list, err := h.service.ListWorkouts(ctx, userID, programID, model.PageInfo{Limit: params.Limit, Offset: params.Offset})
	if err != nil {
		handleError(c, err)
		return
	}
	result := make([]dto.WorkoutSummaryDTO, len(list))
	for i, workout := range list {
		result[i] = dto.WorkoutSummaryDTO(workout)
	}
	c.JSON(http.StatusOK, result)
}
