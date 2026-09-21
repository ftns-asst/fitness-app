package mapper

import (
	"ftns-asst/internal/model"
	"ftns-asst/internal/rest/dto"
	"ftns-asst/internal/util"
)

func ConvertUserToDTO(m *model.User) *dto.UserResponse {
	if m == nil {
		return nil
	}
	return &dto.UserResponse{
		ID:        m.ID,
		Name:      m.Name,
		Email:     m.Email,
		CreatedAt: m.CreatedAt,
	}
}

func ConvertProfileToDTO(m *model.UserProfile) *dto.UserProfileResponse {
	if m == nil {
		return nil
	}
	return &dto.UserProfileResponse{
		Age:      *m.Age,
		Gender:   string(*m.Gender),
		HeightCm: *m.HeightCm,
		WeightKg: *m.WeightKg,
	}
}

func ConvertUserWithProfileToDTO(m *model.UserWithProfile) *dto.UserWithProfileResponse {
	if m == nil {
		return nil
	}
	return &dto.UserWithProfileResponse{
		UserResponse: util.UnPtr(ConvertUserToDTO(util.Ptr(m.User))),
		Profile:      ConvertProfileToDTO(m.Profile),
	}
}

func ConvertUpdateUserProfileInputFromDTO(d *dto.UpdateUserProfileRequestBody) *model.UpdateUserProfileInput {
	if d == nil {
		return nil
	}
	return &model.UpdateUserProfileInput{
		Age:      d.Age,
		HeightCm: d.HeightCm,
		WeightKg: d.WeightKg,
		Gender:   d.Gender,
	}
}
