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

type PersonalRecordHandler struct {
	personalRecordService PersonalRecordService
}

func NewPersonalRecordHandler(personalRecordService PersonalRecordService) *PersonalRecordHandler {
	return &PersonalRecordHandler{
		personalRecordService: personalRecordService,
	}
}

func (h *PersonalRecordHandler) RegisterRoutes(r *gin.RouterGroup, authMid gin.HandlerFunc) {
	r.GET("/users/:id/prs", authMid, h.ListLatestRecords)
	r.POST("/users/:id/prs", authMid, h.CreateRecord)
	r.POST("/users/:id/prs/bulk", authMid, h.CreateRecords)
	r.GET("/users/:id/prs/:exerciseId", authMid, h.ListRecordHistory)
}

// CreateRecord godoc
// @Id createPersonalRecord
// @Tags personal records
// @Summary Add a personal record in the exercise
// @Description Without reps weight_kg is the lifted weight for one repetition. With reps weight_kg is the weight of the set and the record is estimated by a formula (is_calculated is true for more than one repetition). Programs which wait for records (needs_measurement) become draft when all records are added. Only the user can add own records. Error exercise_not_available
// @Accept json
// @Produce json
// @Security BearerAuth
// @Param id path string true "user id" format(uuid)
// @Param request body dto.CreatePersonalRecordRequestBody true "record"
// @Success 200 {object} dto.PersonalRecordDTO
// @Failure 400 {object} ErrorResponse
// @Failure 401 {object} ErrorResponse
// @Failure 403 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /users/{id}/prs [post]
func (h *PersonalRecordHandler) CreateRecord(c *gin.Context) {
	requesterID, err := tryGetUserIDFromCtx(c.Request.Context())
	if err != nil {
		handleError(c, err)
		return
	}

	userID, err := parseUUIDParam(c, "id")
	if err != nil {
		handleError(c, err)
		return
	}

	var body dto.CreatePersonalRecordRequestBody
	err = c.ShouldBindJSON(&body)
	if err != nil {
		handleError(c, fmt.Errorf("failed to get body from request: %w: %w", err, model.ErrBadRequest))
		return
	}

	res, err := h.personalRecordService.CreateRecord(c.Request.Context(), requesterID, userID, mapper.ConvertCreatePersonalRecordInputFromDTO(&body))
	if err != nil {
		handleError(c, err)
		return
	}

	c.JSON(http.StatusOK, mapper.ConvertPersonalRecordToDTO(res))
}

// CreateRecords godoc
// @Id createPersonalRecords
// @Tags personal records
// @Summary Add personal records in several exercises at once
// @Description Records are added in one transaction, either all of them or none. The records are of different exercises, from 1 to 100, each one is the same as in POST /users/{id}/prs. The result has the order of the request. Error exercise_not_available if any exercise is not available
// @Accept json
// @Produce json
// @Security BearerAuth
// @Param id path string true "user id" format(uuid)
// @Param request body dto.CreatePersonalRecordsRequestBody true "records"
// @Success 200 {array} dto.PersonalRecordDTO
// @Failure 400 {object} ErrorResponse
// @Failure 401 {object} ErrorResponse
// @Failure 403 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /users/{id}/prs/bulk [post]
func (h *PersonalRecordHandler) CreateRecords(c *gin.Context) {
	requesterID, err := tryGetUserIDFromCtx(c.Request.Context())
	if err != nil {
		handleError(c, err)
		return
	}

	userID, err := parseUUIDParam(c, "id")
	if err != nil {
		handleError(c, err)
		return
	}

	var body dto.CreatePersonalRecordsRequestBody
	err = c.ShouldBindJSON(&body)
	if err != nil {
		handleError(c, fmt.Errorf("failed to get body from request: %w: %w", err, model.ErrBadRequest))
		return
	}

	list, err := h.personalRecordService.CreateRecords(c.Request.Context(), requesterID, userID, mapper.ConvertCreatePersonalRecordsInputFromDTO(&body))
	if err != nil {
		handleError(c, err)
		return
	}

	c.JSON(http.StatusOK, convertPersonalRecordList(list))
}

// ListLatestRecords godoc
// @Id listLatestPersonalRecords
// @Tags personal records
// @Summary Get the latest personal record in each exercise of the user
// @Description Only the user can see own records
// @Produce json
// @Security BearerAuth
// @Param id path string true "user id" format(uuid)
// @Success 200 {array} dto.PersonalRecordDTO
// @Failure 400 {object} ErrorResponse
// @Failure 401 {object} ErrorResponse
// @Failure 403 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /users/{id}/prs [get]
func (h *PersonalRecordHandler) ListLatestRecords(c *gin.Context) {
	requesterID, err := tryGetUserIDFromCtx(c.Request.Context())
	if err != nil {
		handleError(c, err)
		return
	}

	userID, err := parseUUIDParam(c, "id")
	if err != nil {
		handleError(c, err)
		return
	}

	list, err := h.personalRecordService.GetLatestRecords(c.Request.Context(), requesterID, userID)
	if err != nil {
		handleError(c, err)
		return
	}

	c.JSON(http.StatusOK, convertPersonalRecordList(list))
}

// ListRecordHistory godoc
// @Id listPersonalRecordHistory
// @Tags personal records
// @Summary Get the history of personal records of the user in the exercise
// @Description The newest record first, an exercise without records gives an empty list. Only the user can see own records
// @Produce json
// @Security BearerAuth
// @Param id path string true "user id" format(uuid)
// @Param exerciseId path string true "exercise id" format(uuid)
// @Success 200 {array} dto.PersonalRecordDTO
// @Failure 400 {object} ErrorResponse
// @Failure 401 {object} ErrorResponse
// @Failure 403 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /users/{id}/prs/{exerciseId} [get]
func (h *PersonalRecordHandler) ListRecordHistory(c *gin.Context) {
	requesterID, err := tryGetUserIDFromCtx(c.Request.Context())
	if err != nil {
		handleError(c, err)
		return
	}

	userID, err := parseUUIDParam(c, "id")
	if err != nil {
		handleError(c, err)
		return
	}

	exerciseID, err := parseUUIDParam(c, "exerciseId")
	if err != nil {
		handleError(c, err)
		return
	}

	list, err := h.personalRecordService.GetRecordHistory(c.Request.Context(), requesterID, userID, exerciseID)
	if err != nil {
		handleError(c, err)
		return
	}

	c.JSON(http.StatusOK, convertPersonalRecordList(list))
}

func convertPersonalRecordList(list []*model.PersonalRecord) []*dto.PersonalRecordDTO {
	res := util.Map(list, mapper.ConvertPersonalRecordToDTO)
	if res == nil {
		res = make([]*dto.PersonalRecordDTO, 0)
	}
	return res
}

func parseUUIDParam(c *gin.Context, name string) (uuid.UUID, error) {
	id, err := uuid.Parse(c.Param(name))
	if err != nil {
		return uuid.Nil, fmt.Errorf("failed to parse %s param: %w: %w", name, err, model.ErrBadRequest)
	}
	return id, nil
}
