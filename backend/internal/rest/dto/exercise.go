package dto

import (
	"time"

	"github.com/google/uuid"
)

type (
	ExerciseDTO struct {
		ID           uuid.UUID  `json:"id"`
		Name         string     `json:"name"`
		Description  *string    `json:"description,omitempty"`
		OwnerID      *uuid.UUID `json:"owner_id,omitempty"`
		IsPublic     bool       `json:"is_public"`
		MuscleGroups []string   `json:"muscle_groups"`
		Equipment    []string   `json:"equipment"`
		Media        *MediaDTO  `json:"media,omitempty"`
		CreatedAt    time.Time  `json:"created_at"`
	} // @Name Exercise
	ExerciseInfoDTO struct {
		Name         *string  `json:"name,omitempty"`
		IsPublic     *bool    `json:"is_public"`
		Description  *string  `json:"description,omitempty"`
		MuscleGroups []string `json:"muscle_groups"`
		Equipment    []string `json:"equipment"`
	} // @Name ExerciseInfo
	CreateExerciseRequestBody struct {
		Name         string   `json:"name"`
		IsPublic     bool     `json:"is_public"`
		Description  *string  `json:"description,omitempty"`
		MuscleGroups []string `json:"muscle_groups"`
		Equipment    []string `json:"equipment"`
	} // @Name CreateExerciseRequestBody
	UpdateExerciseRequestBody struct {
		ID uuid.UUID `json:"id"`
		ExerciseInfoDTO
	} // @Name UpdateExerciseRequestBody
	MediaDTO struct {
		ID   uuid.UUID `json:"id"`
		Type string    `json:"type"`
		URL  string    `json:"url,omitempty"` // temporary url for downloading
	} // @Name Media
	UpdateExerciseMediaRequestBody struct {
		MediaType string `json:"media_type" binding:"required,oneof=image video"`
	} // @Name UpdateExerciseMediaRequestBody
	UpdateExerciseMediaResponse struct {
		Media     MediaDTO `json:"media"`
		UploadURL string   `json:"upload_url"` // temporary url for uploading the file with PUT
	} // @Name UpdateExerciseMediaResponse
	ListExerciseQueryParams struct {
		Type         string   `form:"type" binding:"required,oneof=own public basic all"`
		MuscleGroups []string `form:"muscle_group"`
		Equipment    []string `form:"equipment"`
		Search       *string  `form:"search,omitempty"`
		PageQueryParams
	} // @Name ListExerciseQueryParams
	PageQueryParams struct {
		Limit  *int `form:"limit" binding:"omitempty,min=1,max=1000"`
		Offset *int `form:"offset" binding:"omitempty,min=0,max=100000"`
	} // @Name PageQueryParams
	MuscleGroup struct {
		Key  string `json:"key"`  // unique key
		Name string `json:"name"` // readable name
	} // @Name MuscleGroup

	Equipment struct {
		Key  string `json:"key"`  // unique key
		Name string `json:"name"` // readable name
	} // @Name Equipment
)
