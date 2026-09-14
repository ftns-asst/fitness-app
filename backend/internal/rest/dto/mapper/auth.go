package mapper

import (
	"ftns-asst/internal/model"
	"ftns-asst/internal/rest/dto"
)

func ConvertSignUpRequestBodyFromDTO(d *dto.SignUpRequestBody) *model.SignUpInput {
	if d == nil {
		return nil
	}
	return &model.SignUpInput{
		AuthInput: model.AuthInput{
			Email:    d.Email,
			Password: d.Password,
		},
		Name: d.Name,
	}
}

func ConvertLogInRequestBodyFromDTO(d *dto.LogInRequestBody) *model.LogInInput {
	if d == nil {
		return nil
	}
	return &model.LogInInput{
		AuthInput: model.AuthInput{
			Email:    d.Email,
			Password: d.Password,
		},
	}
}

func ConvertTokensToDTO(m *model.Tokens) *dto.Tokens {
	if m == nil {
		return nil
	}
	return &dto.Tokens{
		AccessToken:  m.AccessToken,
		RefreshToken: m.RefreshToken,
	}
}

func ConvertTokensFromDTO(d *dto.Tokens) *model.Tokens {
	if d == nil {
		return nil
	}
	return &model.Tokens{
		AccessToken:  d.AccessToken,
		RefreshToken: d.RefreshToken,
	}
}

func ConvertSignUpResultToDTO(m *model.SuccessSignUpResult) *dto.SignUpResponse {
	if m == nil {
		return nil
	}
	return &dto.SignUpResponse{
		User:   ConvertUserToDTO(m.User),
		Tokens: ConvertTokensToDTO(m.Tokens),
	}
}

func ConvertLogInResultToDTO(m *model.SuccessLogInResult) *dto.LogInResponse {
	if m == nil {
		return nil
	}
	return &dto.LogInResponse{
		User:   ConvertUserToDTO(m.User),
		Tokens: ConvertTokensToDTO(m.Tokens),
	}
}
