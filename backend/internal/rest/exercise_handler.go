package rest

import (
	"fmt"
	"ftns-asst/internal/model"
	"ftns-asst/internal/rest/dto"
	"ftns-asst/internal/rest/dto/mapper"
	"ftns-asst/internal/util"
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

	r.PUT("/admin/exercise/:id/media", authMid, h.UpdateExerciseMedia)
	r.DELETE("/admin/exercise/:id/media", authMid, h.DeleteExerciseMedia)
}

// GetMuscleGroups godoc
// @Id getMuscleGroups
// @Tags exercises
// @Summary Get all muscle groups
// @Produce json
// @Success 200 {array} dto.MuscleGroup
// @Router /muscle-groups [get]
func (h *ExerciseHandler) GetMuscleGroups(c *gin.Context) {
	res := mapper.ConvertMuscleGroupListToDTO(h.exerciseService.GetMuscleGroups())
	if res == nil {
		res = make([]dto.MuscleGroup, 0)
	}

	c.JSON(http.StatusOK, res)
}

// GetEquipments godoc
// @Id getEquipments
// @Tags exercises
// @Summary Get all equipment types
// @Produce json
// @Success 200 {array} dto.Equipment
// @Router /equipments [get]
func (h *ExerciseHandler) GetEquipments(c *gin.Context) {
	res := mapper.ConvertEquipmentListToDTO(h.exerciseService.GetEquipment())
	if res == nil {
		res = make([]dto.Equipment, 0)
	}
	c.JSON(http.StatusOK, res)
}

// ListExercises godoc
// @Id listExercises
// @Tags exercises
// @Summary List exercises
// @Description Authorization is optional: with a token, own exercises are included in the "own" and "all" lists
// @Accept json
// @Produce json
// @Param type query string true "list type" Enums(own, public, basic, all)
// @Param muscle_group query []string false "filter by muscle groups" collectionFormat(multi)
// @Param equipment query []string false "filter by equipment" collectionFormat(multi)
// @Param search query string false "search by name"
// @Param limit query int true "page size" minimum(1) maximum(1000)
// @Param offset query int true "page offset" minimum(0) maximum(100000)
// @Success 200 {array} dto.ExerciseDTO
// @Failure 400 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /exercises [get]
func (h *ExerciseHandler) ListExercises(c *gin.Context) {
	userID, _ := tryGetUserIDFromCtx(c.Request.Context())

	var params dto.ListExerciseQueryParams
	err := c.ShouldBindQuery(&params)
	if err != nil {
		handleError(c, fmt.Errorf("failed to get query params from request: %w: %w", err, model.ErrBadRequest))
		return
	}
	// log.Printf("%v", params)
	filters, page := mapper.ConvertListExerciseQueryParams(&params, &userID)
	list, err := h.exerciseService.GetExercises(c.Request.Context(),
		filters, page)
	if err != nil {
		handleError(c, err)
		return
	}

	res := util.Map(list, mapper.ConvertExerciseToDTO)
	if res == nil {
		res = make([]*dto.ExerciseDTO, 0)
	}
	c.JSON(http.StatusOK, res)
}

// CreateExercise godoc
// @Id createExercise
// @Tags exercises
// @Summary Create exercise
// @Accept json
// @Produce json
// @Security BearerAuth
// @Param request body dto.CreateExerciseRequestBody true "exercise body"
// @Success 200 {object} dto.ExerciseDTO
// @Failure 400 {object} ErrorResponse
// @Failure 401 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /exercises [post]
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
		mapper.ConvertExerciseFromCreateDTO(&body, &userID))
	if err != nil {
		handleError(c, err)
		return
	}

	c.JSON(http.StatusOK, mapper.ConvertExerciseToDTO(res))
}

// GetExerciseByID godoc
// @Id getExerciseByID
// @Tags exercises
// @Summary Get exercise by ID
// @Produce json
// @Security BearerAuth
// @Param id path string true "exercise id" format(uuid)
// @Success 200 {object} dto.ExerciseDTO
// @Failure 400 {object} ErrorResponse
// @Failure 401 {object} ErrorResponse
// @Failure 403 {object} ErrorResponse
// @Failure 404 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /exercises/{id} [get]
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

// UpdateExercise godoc
// @Id updateExercise
// @Tags exercises
// @Summary Update exercise
// @Accept json
// @Produce json
// @Security BearerAuth
// @Param id path string true "exercise id" format(uuid)
// @Param request body dto.UpdateExerciseRequestBody true "exercise body, id must match path id"
// @Success 200 {object} dto.ExerciseDTO
// @Failure 400 {object} ErrorResponse
// @Failure 401 {object} ErrorResponse
// @Failure 403 {object} ErrorResponse
// @Failure 404 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /exercises/{id} [patch]
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
		handleError(c, fmt.Errorf("different 'id' value from param(%s) and body(%s): %w", id, body.ID, model.ErrBadRequest))
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

// DeleteExercise godoc
// @Id deleteExercise
// @Tags exercises
// @Summary Delete exercise
// @Security BearerAuth
// @Param id path string true "exercise id" format(uuid)
// @Success 200
// @Failure 400 {object} ErrorResponse
// @Failure 401 {object} ErrorResponse
// @Failure 403 {object} ErrorResponse
// @Failure 404 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /exercises/{id} [delete]
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

// UpdateExerciseMedia godoc
// @Id updateExerciseMedia
// @Tags exercises
// @Summary Get url for uploading exercise media
// @Description Creates media if exercise has none. The file must be uploaded to upload_url with PUT, the stored media type is updated if it differs
// @Accept json
// @Produce json
// @Security BearerAuth
// @Param id path string true "exercise id" format(uuid)
// @Param request body dto.UpdateExerciseMediaRequestBody true "media body"
// @Success 200 {object} dto.UpdateExerciseMediaResponse
// @Failure 400 {object} ErrorResponse
// @Failure 401 {object} ErrorResponse
// @Failure 404 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /admin/exercise/{id}/media [put]
func (h *ExerciseHandler) UpdateExerciseMedia(c *gin.Context) {
	id, err := uuid.Parse(c.Param("id"))
	if err != nil {
		handleError(c, fmt.Errorf("failed to parse id param: %w: %w", err, model.ErrBadRequest))
		return
	}

	var body dto.UpdateExerciseMediaRequestBody
	err = c.ShouldBindJSON(&body)
	if err != nil {
		handleError(c, fmt.Errorf("failed to get body from request: %w: %w", err, model.ErrBadRequest))
		return
	}

	media, uploadURL, err := h.exerciseService.UpdateMedia(c.Request.Context(), id, model.MediaType(body.MediaType))
	if err != nil {
		handleError(c, err)
		return
	}

	c.JSON(http.StatusOK, dto.UpdateExerciseMediaResponse{
		Media:     *mapper.ConvertMediaToDTO(media),
		UploadURL: uploadURL,
	})
}

// DeleteExerciseMedia godoc
// @Id deleteExerciseMedia
// @Tags exercises
// @Summary Delete exercise media
// @Security BearerAuth
// @Param id path string true "exercise id" format(uuid)
// @Success 200
// @Failure 400 {object} ErrorResponse
// @Failure 401 {object} ErrorResponse
// @Failure 404 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /admin/exercise/{id}/media [delete]
func (h *ExerciseHandler) DeleteExerciseMedia(c *gin.Context) {
	id, err := uuid.Parse(c.Param("id"))
	if err != nil {
		handleError(c, fmt.Errorf("failed to parse id param: %w: %w", err, model.ErrBadRequest))
		return
	}

	err = h.exerciseService.DeleteMedia(c.Request.Context(), id)
	if err != nil {
		handleError(c, err)
		return
	}

	c.Status(http.StatusOK)
}
