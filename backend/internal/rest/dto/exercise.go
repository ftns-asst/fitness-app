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
		MuscleGroups []string   `json:"muscle_groups"`
		Equipment    []string   `json:"equipment"`
		CreatedAt    time.Time  `json:"created_at"`
	}
	ExerciseInfoDTO struct {
		Name         *string  `json:"name,omitempty"`
		IsPublic     *bool    `json:"is_public"`
		Description  *string  `json:"description,omitempty"`
		MuscleGroups []string `json:"muscle_groups"`
		Equipment    []string `json:"equipment"`
	}
	CreateExerciseRequestBody struct {
		ExerciseDTO
	}
	UpdateExerciseRequestBody struct {
		ID uuid.UUID `json:"id"`
		ExerciseInfoDTO
	}
	ListExerciseQueryParams struct {
		Type         string   `form:"type" binding:"required,oneof=own public basic all"`
		MuscleGroups []string `form:"muscle_group"`
		Equipment    []string `form:"equipment"`
		Search       *string  `form:"search,omitempty"`
		PageQueryParams
	}
	PageQueryParams struct {
		Limit  int `form:"limit" binding:"required,gte=1,lte=1000"`
		Offset int `form:"offset" binding:"required,gte=1,lte=100000"`
	}
)
