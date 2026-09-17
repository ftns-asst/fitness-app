package dto

// check if email is free for registration
type CheckEmailRequestBody struct {
	Email string `json:"email" binding:"required,email"`
} //@Name CheckEmailRequestBody

type CheckEmailResponse struct {
	Free bool `json:"free"`
} //@Name CheckEmailResponse

// --- Registration ---
type SignUpRequestBody struct {
	Name     string `json:"name" binding:"required"`
	Email    string `json:"email" binding:"required,email"`
	Password string `json:"password" binding:"required,min=8,max=64"`
} //@Name SignUpRequestBody

type SignUpResponse struct {
	User   *UserResponse `json:"user"`
	Tokens *Tokens       `json:"tokens"`
} //@Name SignUpResponse

// --- Login ---

type LogInRequestBody struct {
	Email    string `json:"email" binding:"required,email"`
	Password string `json:"password" binding:"required,min=8,max=64"`
} //@Name LogInRequestBody

type LogInResponse struct {
	User   *UserResponse `json:"user"`
	Tokens *Tokens       `json:"tokens"`
} //@Name LogInResponse

// --- Tokens ---

// jwt tokens for authentication
type Tokens struct {
	AccessToken  string `json:"access_token"`
	RefreshToken string `json:"refresh_token"`
} //@Name Tokens

// refresh expired access token with refresh token
type RefreshTokensRequestBody struct {
	RefreshToken string `json:"refresh_token" binding:"required"`
} //@Name RefreshTokensRequestBody

// refreshed access and refresh tokens
type RefreshTokensResponse struct {
	Tokens *Tokens `json:"tokens"`
} //@Name RefreshTokensResponse

// --- Password recovery ---

// password recovery with email to send recovery code
type PasswordRecoveryRequestBody struct {
	Email string `json:"email" binding:"required,email"`
} //@Name PasswordRecoveryRequestBody

// verify recovery code, entered by user
type VerifyRecoveryCodeRequestBody struct {
	Email string `json:"email" binding:"required,email"`
	Code  string `json:"code" binding:"required"`
} //@Name VerifyRecoveryCodeRequestBody
