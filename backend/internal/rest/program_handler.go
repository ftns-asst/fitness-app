package rest

import (
	"fmt"
	"ftns-asst/internal/model"
	"ftns-asst/internal/rest/dto"
	"ftns-asst/internal/rest/dto/mapper"
	"ftns-asst/internal/util"
	"net/http"
	"strconv"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
)

type ProgramHandler struct {
	programService ProgramService
}

func NewProgramHandler(programService ProgramService) *ProgramHandler {
	return &ProgramHandler{
		programService: programService,
	}
}

func (h *ProgramHandler) RegisterRoutes(r *gin.RouterGroup, authMid gin.HandlerFunc) {
	r.GET("/default-programs", h.ListDefaultPrograms)
	r.GET("/default-programs/:id", h.GetDefaultProgramByID)

	r.GET("/programs", authMid, h.ListPrograms)
	r.POST("/programs", authMid, h.CreateProgram)
	r.POST("/programs/from-default", authMid, h.CreateProgramFromDefault)
	r.GET("/programs/:id", authMid, h.GetProgramByID)
	r.PATCH("/programs/:id", authMid, h.UpdateProgram)
	r.PUT("/programs/:id/structure", authMid, h.ReplaceProgramStructure)
	r.PUT("/programs/:id/weeks/:week", authMid, h.ReplaceProgramWeek)
	r.POST("/programs/:id/start", authMid, h.StartProgram)
	r.POST("/programs/:id/finish", authMid, h.FinishProgram)
	r.DELETE("/programs/:id", authMid, h.DeleteProgram)
}

// ListDefaultPrograms godoc
// @Id listDefaultPrograms
// @Tags programs
// @Summary List default programs
// @Description With day_of_week only programs with the same number of days per week are returned
// @Produce json
// @Param day_of_week query []int false "selected days of week, 1 - Monday, 7 - Sunday" collectionFormat(multi) minimum(1) maximum(7)
// @Param goal query string false "filter by goal" Enums(strength, muscle, fitness, fat_loss, endurance)
// @Param level query string false "filter by level" Enums(beginner, intermediate, advanced)
// @Success 200 {array} dto.DefaultProgramSummaryDTO
// @Failure 400 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /default-programs [get]
func (h *ProgramHandler) ListDefaultPrograms(c *gin.Context) {
	var params dto.ListDefaultProgramsQueryParams
	err := c.ShouldBindQuery(&params)
	if err != nil {
		handleError(c, fmt.Errorf("failed to get query params from request: %w: %w", err, model.ErrBadRequest))
		return
	}

	var goal *model.ProgramGoal
	if params.Goal != nil {
		goal = util.Ptr(model.ProgramGoal(*params.Goal))
	}
	var level *model.ProgramLevel
	if params.Level != nil {
		level = util.Ptr(model.ProgramLevel(*params.Level))
	}

	list, err := h.programService.GetDefaultPrograms(c.Request.Context(), params.DaysOfWeek, goal, level)
	if err != nil {
		handleError(c, err)
		return
	}

	res := util.Map(list, mapper.ConvertDefaultProgramToSummaryDTO)
	if res == nil {
		res = make([]*dto.DefaultProgramSummaryDTO, 0)
	}
	c.JSON(http.StatusOK, res)
}

// GetDefaultProgramByID godoc
// @Id getDefaultProgramByID
// @Tags programs
// @Summary Get default program with days
// @Produce json
// @Param id path string true "default program id" format(uuid)
// @Success 200 {object} dto.DefaultProgramDTO
// @Failure 400 {object} ErrorResponse
// @Failure 404 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /default-programs/{id} [get]
func (h *ProgramHandler) GetDefaultProgramByID(c *gin.Context) {
	id, err := parseIDParam(c)
	if err != nil {
		handleError(c, err)
		return
	}

	res, err := h.programService.GetDefaultProgramByID(c.Request.Context(), id)
	if err != nil {
		handleError(c, err)
		return
	}

	c.JSON(http.StatusOK, mapper.ConvertDefaultProgramToDTO(res))
}

// ListPrograms godoc
// @Id listPrograms
// @Tags programs
// @Summary List programs of current user without exercises
// @Produce json
// @Security BearerAuth
// @Param state query string false "filter by state" Enums(draft, needs_measurement, in_progress, finished)
// @Success 200 {array} dto.ProgramSummaryDTO
// @Failure 400 {object} ErrorResponse
// @Failure 401 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /programs [get]
func (h *ProgramHandler) ListPrograms(c *gin.Context) {
	userID, err := tryGetUserIDFromCtx(c.Request.Context())
	if err != nil {
		handleError(c, err)
		return
	}

	var params dto.ListProgramsQueryParams
	err = c.ShouldBindQuery(&params)
	if err != nil {
		handleError(c, fmt.Errorf("failed to get query params from request: %w: %w", err, model.ErrBadRequest))
		return
	}

	var state *model.ProgramState
	if params.State != nil {
		state = util.Ptr(model.ProgramState(*params.State))
	}

	list, err := h.programService.GetUserPrograms(c.Request.Context(), userID, state)
	if err != nil {
		handleError(c, err)
		return
	}

	res := util.Map(list, mapper.ConvertProgramToSummaryDTO)
	if res == nil {
		res = make([]*dto.ProgramSummaryDTO, 0)
	}
	c.JSON(http.StatusOK, res)
}

// GetProgramByID godoc
// @Id getProgramByID
// @Tags programs
// @Summary Get program with weeks, days and exercises
// @Description Every week has all days from days_of_week, a day without exercises has an empty list
// @Produce json
// @Security BearerAuth
// @Param id path string true "program id" format(uuid)
// @Success 200 {object} dto.ProgramDTO
// @Failure 400 {object} ErrorResponse
// @Failure 401 {object} ErrorResponse
// @Failure 403 {object} ErrorResponse
// @Failure 404 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /programs/{id} [get]
func (h *ProgramHandler) GetProgramByID(c *gin.Context) {
	userID, err := tryGetUserIDFromCtx(c.Request.Context())
	if err != nil {
		handleError(c, err)
		return
	}

	id, err := parseIDParam(c)
	if err != nil {
		handleError(c, err)
		return
	}

	res, err := h.programService.GetUserProgramByID(c.Request.Context(), userID, id)
	if err != nil {
		handleError(c, err)
		return
	}

	c.JSON(http.StatusOK, mapper.ConvertProgramToDTO(res))
}

// CreateProgramFromDefault godoc
// @Id createProgramFromDefault
// @Tags programs
// @Summary Create draft from default program
// @Description Workouts of every week of the default program are laid out on the selected days of week in order. Selected days count must be equal to days_per_week of the program (days_count_mismatch)
// @Accept json
// @Produce json
// @Security BearerAuth
// @Param request body dto.CreateProgramFromDefaultRequestBody true "default program and selected days"
// @Success 200 {object} dto.ProgramDTO
// @Failure 400 {object} ErrorResponse
// @Failure 401 {object} ErrorResponse
// @Failure 404 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /programs/from-default [post]
func (h *ProgramHandler) CreateProgramFromDefault(c *gin.Context) {
	userID, err := tryGetUserIDFromCtx(c.Request.Context())
	if err != nil {
		handleError(c, err)
		return
	}

	var body dto.CreateProgramFromDefaultRequestBody
	err = c.ShouldBindJSON(&body)
	if err != nil {
		handleError(c, fmt.Errorf("failed to get body from request: %w: %w", err, model.ErrBadRequest))
		return
	}

	res, err := h.programService.CreateFromDefault(c.Request.Context(), userID, body.DefaultProgramID, body.DaysOfWeek)
	if err != nil {
		handleError(c, err)
		return
	}

	c.JSON(http.StatusOK, mapper.ConvertProgramToDTO(res))
}

// CreateProgram godoc
// @Id createProgram
// @Tags programs
// @Summary Create draft with metadata only
// @Description The program appears in the list at once, the structure is filled by PUT /programs/{id}/structure or PUT /programs/{id}/weeks/{week}. weeks_count is 1 by default
// @Accept json
// @Produce json
// @Security BearerAuth
// @Param request body dto.CreateProgramRequestBody true "program body"
// @Success 200 {object} dto.ProgramDTO
// @Failure 400 {object} ErrorResponse
// @Failure 401 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /programs [post]
func (h *ProgramHandler) CreateProgram(c *gin.Context) {
	userID, err := tryGetUserIDFromCtx(c.Request.Context())
	if err != nil {
		handleError(c, err)
		return
	}

	var body dto.CreateProgramRequestBody
	err = c.ShouldBindJSON(&body)
	if err != nil {
		handleError(c, fmt.Errorf("failed to get body from request: %w: %w", err, model.ErrBadRequest))
		return
	}

	res, err := h.programService.CreateProgram(c.Request.Context(), userID, mapper.ConvertCreateProgramInputFromDTO(&body))
	if err != nil {
		handleError(c, err)
		return
	}

	c.JSON(http.StatusOK, mapper.ConvertProgramToDTO(res))
}

// UpdateProgram godoc
// @Id updateProgram
// @Tags programs
// @Summary Update metadata of program
// @Description days_of_week and weeks_count are common for all weeks; exercises of removed days and weeks are deleted. Error program_read_only for finished program
// @Accept json
// @Produce json
// @Security BearerAuth
// @Param id path string true "program id" format(uuid)
// @Param request body dto.UpdateProgramRequestBody true "fields to change"
// @Success 200 {object} dto.ProgramDTO
// @Failure 400 {object} ErrorResponse
// @Failure 401 {object} ErrorResponse
// @Failure 403 {object} ErrorResponse
// @Failure 404 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /programs/{id} [patch]
func (h *ProgramHandler) UpdateProgram(c *gin.Context) {
	userID, err := tryGetUserIDFromCtx(c.Request.Context())
	if err != nil {
		handleError(c, err)
		return
	}

	id, err := parseIDParam(c)
	if err != nil {
		handleError(c, err)
		return
	}

	var body dto.UpdateProgramRequestBody
	err = c.ShouldBindJSON(&body)
	if err != nil {
		handleError(c, fmt.Errorf("failed to get body from request: %w: %w", err, model.ErrBadRequest))
		return
	}

	res, err := h.programService.UpdateProgram(c.Request.Context(), userID, mapper.ConvertUpdateProgramInputFromDTO(id, &body))
	if err != nil {
		handleError(c, err)
		return
	}

	c.JSON(http.StatusOK, mapper.ConvertProgramToDTO(res))
}

// ReplaceProgramStructure godoc
// @Id replaceProgramStructure
// @Tags programs
// @Summary Replace days of week, weeks and exercises of program
// @Description Replaces the whole structure. The same request fills the structure of a new draft and edits it later. The position in weeks is the week number, weeks_count becomes the number of weeks. Errors: program_read_only, exercise_not_available
// @Accept json
// @Produce json
// @Security BearerAuth
// @Param id path string true "program id" format(uuid)
// @Param request body dto.ReplaceProgramStructureRequestBody true "full structure"
// @Success 200 {object} dto.ProgramDTO
// @Failure 400 {object} ErrorResponse
// @Failure 401 {object} ErrorResponse
// @Failure 403 {object} ErrorResponse
// @Failure 404 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /programs/{id}/structure [put]
func (h *ProgramHandler) ReplaceProgramStructure(c *gin.Context) {
	userID, err := tryGetUserIDFromCtx(c.Request.Context())
	if err != nil {
		handleError(c, err)
		return
	}

	id, err := parseIDParam(c)
	if err != nil {
		handleError(c, err)
		return
	}

	var body dto.ReplaceProgramStructureRequestBody
	err = c.ShouldBindJSON(&body)
	if err != nil {
		handleError(c, fmt.Errorf("failed to get body from request: %w: %w", err, model.ErrBadRequest))
		return
	}

	res, err := h.programService.ReplaceStructure(c.Request.Context(), userID, id, mapper.ConvertProgramStructureFromDTO(&body))
	if err != nil {
		handleError(c, err)
		return
	}

	c.JSON(http.StatusOK, mapper.ConvertProgramToDTO(res))
}

// ReplaceProgramWeek godoc
// @Id replaceProgramWeek
// @Tags programs
// @Summary Replace exercises of one week of program
// @Description Days of the week must be among days_of_week of the program, week is 1..weeks_count. Errors: program_read_only, exercise_not_available
// @Accept json
// @Produce json
// @Security BearerAuth
// @Param id path string true "program id" format(uuid)
// @Param week path int true "week number" minimum(1)
// @Param request body dto.ReplaceProgramWeekRequestBody true "days of the week"
// @Success 200 {object} dto.ProgramDTO
// @Failure 400 {object} ErrorResponse
// @Failure 401 {object} ErrorResponse
// @Failure 403 {object} ErrorResponse
// @Failure 404 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /programs/{id}/weeks/{week} [put]
func (h *ProgramHandler) ReplaceProgramWeek(c *gin.Context) {
	userID, err := tryGetUserIDFromCtx(c.Request.Context())
	if err != nil {
		handleError(c, err)
		return
	}

	id, err := parseIDParam(c)
	if err != nil {
		handleError(c, err)
		return
	}

	weekNumber, err := strconv.Atoi(c.Param("week"))
	if err != nil {
		handleError(c, fmt.Errorf("failed to parse week param: %w: %w", err, model.ErrBadRequest))
		return
	}

	var body dto.ReplaceProgramWeekRequestBody
	err = c.ShouldBindJSON(&body)
	if err != nil {
		handleError(c, fmt.Errorf("failed to get body from request: %w: %w", err, model.ErrBadRequest))
		return
	}

	res, err := h.programService.ReplaceWeek(c.Request.Context(), userID, id, weekNumber, mapper.ConvertProgramWeekFromDTO(&body))
	if err != nil {
		handleError(c, err)
		return
	}

	c.JSON(http.StatusOK, mapper.ConvertProgramToDTO(res))
}

// StartProgram godoc
// @Id startProgram
// @Tags programs
// @Summary Start draft program
// @Description The program becomes in_progress, the previous program in progress becomes finished and its id is returned in finished_program_id. Errors: invalid_state_transition, measurement_required (personal records are missing, see required_measurements), program_incomplete
// @Produce json
// @Security BearerAuth
// @Param id path string true "program id" format(uuid)
// @Success 200 {object} dto.StartProgramResponse
// @Failure 400 {object} ErrorResponse
// @Failure 401 {object} ErrorResponse
// @Failure 403 {object} ErrorResponse
// @Failure 404 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /programs/{id}/start [post]
func (h *ProgramHandler) StartProgram(c *gin.Context) {
	userID, err := tryGetUserIDFromCtx(c.Request.Context())
	if err != nil {
		handleError(c, err)
		return
	}

	id, err := parseIDParam(c)
	if err != nil {
		handleError(c, err)
		return
	}

	res, err := h.programService.StartProgram(c.Request.Context(), userID, id)
	if err != nil {
		handleError(c, err)
		return
	}

	c.JSON(http.StatusOK, mapper.ConvertStartProgramResultToDTO(res))
}

// FinishProgram godoc
// @Id finishProgram
// @Tags programs
// @Summary Finish program in progress
// @Description Error invalid_state_transition if program is not in progress
// @Produce json
// @Security BearerAuth
// @Param id path string true "program id" format(uuid)
// @Success 200 {object} dto.ProgramDTO
// @Failure 400 {object} ErrorResponse
// @Failure 401 {object} ErrorResponse
// @Failure 403 {object} ErrorResponse
// @Failure 404 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /programs/{id}/finish [post]
func (h *ProgramHandler) FinishProgram(c *gin.Context) {
	userID, err := tryGetUserIDFromCtx(c.Request.Context())
	if err != nil {
		handleError(c, err)
		return
	}

	id, err := parseIDParam(c)
	if err != nil {
		handleError(c, err)
		return
	}

	res, err := h.programService.FinishProgram(c.Request.Context(), userID, id)
	if err != nil {
		handleError(c, err)
		return
	}

	c.JSON(http.StatusOK, mapper.ConvertProgramToDTO(res))
}

// DeleteProgram godoc
// @Id deleteProgram
// @Tags programs
// @Summary Delete draft program
// @Description Error program_not_draft if program is not a draft or needs_measurement
// @Security BearerAuth
// @Param id path string true "program id" format(uuid)
// @Success 200
// @Failure 400 {object} ErrorResponse
// @Failure 401 {object} ErrorResponse
// @Failure 403 {object} ErrorResponse
// @Failure 404 {object} ErrorResponse
// @Failure 500 {object} ErrorResponse
// @Router /programs/{id} [delete]
func (h *ProgramHandler) DeleteProgram(c *gin.Context) {
	userID, err := tryGetUserIDFromCtx(c.Request.Context())
	if err != nil {
		handleError(c, err)
		return
	}

	id, err := parseIDParam(c)
	if err != nil {
		handleError(c, err)
		return
	}

	err = h.programService.DeleteProgram(c.Request.Context(), userID, id)
	if err != nil {
		handleError(c, err)
		return
	}

	c.Status(http.StatusOK)
}

func parseIDParam(c *gin.Context) (uuid.UUID, error) {
	id, err := uuid.Parse(c.Param("id"))
	if err != nil {
		return uuid.Nil, fmt.Errorf("failed to parse id param: %w: %w", err, model.ErrBadRequest)
	}
	return id, nil
}
