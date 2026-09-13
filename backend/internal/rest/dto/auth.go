package dto

type CheckEmailRequestBody struct {
	Email string `json:"email" binding:"required,email"`
} //@Name CheckEmailRequestBody

type CheckEmailResponse struct {
	Free bool `json:"free"`
} //@Name CheckEmailResponse
