package model

import (
	"time"

	"github.com/google/uuid"
)

type Gender string

const (
	GenderMale   Gender = "male"
	GenderFemale Gender = "female"
)

type User struct {
	ID           uuid.UUID `db:"id"`
	Name         string    `db:"display_name"`
	Email        string    `db:"email"`
	PasswordHash string    `db:"pass_hash"`
	CreatedAt    time.Time `db:"created_at"`
	UpdatedAt    time.Time `db:"updated_at"`
}

type UserProfile struct {
	UserID    uuid.UUID  `db:"user_id"`
	Age       *uint      `db:"age"`
	Gender    *Gender    `db:"gender"`
	HeightCm  *uint      `db:"height_cm"`
	WeightKg  *uint      `db:"weight_kg"`
	UpdatedAt *time.Time `db:"updated_at"`
}

type UserWithProfile struct {
	User
	Profile *UserProfile
}

type UpdateUserProfileInput struct {
	Age      *uint
	Gender   *string
	HeightCm *uint
	WeightKg *uint
} // @Name UpdateUserProfileRequestBody
