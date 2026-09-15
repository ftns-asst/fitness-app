package dto

import (
	"time"

	"github.com/google/uuid"
)

type GetUserQueryParams struct {
	WithProfile bool `form:"withProfile"`
} // @Name GetUserParams

type UserResponse struct {
	ID        uuid.UUID `json:"id"`
	Name      string    `json:"name"`
	Email     string    `json:"email"`
	CreatedAt time.Time `json:"created_at"`
} // @Name User

type UserProfileResponse struct {
	Age      uint   `json:"age,omitempty"`
	Gender   string `json:"gender,omitempty" enums:"male,female"`
	HeightCm uint   `json:"height,omitempty"`
	WeightKg uint   `json:"weight,omitempty"`
} // @Name UserProfile

type UserWithProfileResponse struct {
	UserResponse
	Profile *UserProfileResponse `json:"profile,omitempty"`
} // @Name UserWithProfile
