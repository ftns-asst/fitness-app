package rest

import (
	"errors"
	"fmt"
	"ftns-asst/internal/model"
	"ftns-asst/internal/rest/dto"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"github.com/stretchr/testify/mock"
	"github.com/stretchr/testify/require"
)

type userHandlerTest struct {
	Name         string
	Path         string
	Body         any
	Token        string
	Setup        func(*deps)
	CodeExpected int
	KeyExpected  string
	Check        func(*testing.T, *httptest.ResponseRecorder)
}

func runUserHandlerTests(t *testing.T, method string, tests []userHandlerTest) {
	t.Helper()
	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			t.Parallel()
			d := newDeps(t)
			if test.Setup != nil {
				test.Setup(d)
			}

			w := doRequest(t, newRouter(d), method, test.Path, test.Body, test.Token)

			if test.CodeExpected != http.StatusOK {
				requireError(t, w, test.CodeExpected, test.KeyExpected)
				return
			}
			require.Equal(t, http.StatusOK, w.Code, w.Body.String())
			if test.Check != nil {
				test.Check(t, w)
			}
		})
	}
}

func newUserWithProfile() *model.UserWithProfile {
	user := model.User{
		ID:           uuid.New(),
		Name:         "John",
		Email:        "test@example.com",
		PasswordHash: "hash",
		CreatedAt:    time.Now().Add(-time.Hour).UTC().Truncate(time.Second),
	}
	return &model.UserWithProfile{
		User: user,
		Profile: &model.UserProfile{
			UserID:   user.ID,
			Age:      new(uint(20)),
			Gender:   new(model.GenderMale),
			HeightCm: new(uint(170)),
			WeightKg: new(uint(65)),
		},
	}
}

func userResponse(u *model.UserWithProfile) dto.UserResponse {
	return dto.UserResponse{
		ID:        u.ID,
		Name:      u.Name,
		Email:     u.Email,
		CreatedAt: u.CreatedAt,
	}
}

func profileResponse(p *model.UserProfile) *dto.UserProfileResponse {
	return &dto.UserProfileResponse{
		Age:      *p.Age,
		Gender:   string(*p.Gender),
		HeightCm: *p.HeightCm,
		WeightKg: *p.WeightKg,
	}
}

func TestGetUserByIDHandler(t *testing.T) {
	t.Parallel()
	user := newUserWithProfile()
	withoutProfile := &model.UserWithProfile{User: user.User}
	token := genAccessToken(t, uuid.New(), testSecret)
	path := "/api/v1/users/" + user.ID.String()

	runUserHandlerTests(t, http.MethodGet, []userHandlerTest{
		{
			Name:         "no token",
			Path:         path,
			CodeExpected: http.StatusUnauthorized,
			KeyExpected:  model.AuthErrorInvalidTokenKey,
		},
		{
			Name:         "invalid id",
			Path:         "/api/v1/users/123",
			Token:        token,
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:         "invalid query param",
			Path:         path + "?withProfile=maybe",
			Token:        token,
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:  "not found",
			Path:  path,
			Token: token,
			Setup: func(d *deps) {
				d.user.EXPECT().GetUserByID(mock.Anything, user.ID, false).
					Return(nil, fmt.Errorf("user not found: %w", model.ErrNotFound))
			},
			CodeExpected: http.StatusNotFound,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:  "service error",
			Path:  path,
			Token: token,
			Setup: func(d *deps) {
				d.user.EXPECT().GetUserByID(mock.Anything, user.ID, false).
					Return(nil, errors.New("db is down"))
			},
			CodeExpected: http.StatusInternalServerError,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:  "success without profile",
			Path:  path,
			Token: token,
			Setup: func(d *deps) {
				d.user.EXPECT().GetUserByID(mock.Anything, user.ID, false).Return(withoutProfile, nil)
			},
			CodeExpected: http.StatusOK,
			Check: func(t *testing.T, w *httptest.ResponseRecorder) {
				require.Equal(t, dto.UserWithProfileResponse{
					UserResponse: userResponse(user),
				}, decodeBody[dto.UserWithProfileResponse](t, w))
				require.NotContains(t, w.Body.String(), "profile")
			},
		},
		{
			Name:  "success with profile",
			Path:  path + "?withProfile=true",
			Token: token,
			Setup: func(d *deps) {
				d.user.EXPECT().GetUserByID(mock.Anything, user.ID, true).Return(user, nil)
			},
			CodeExpected: http.StatusOK,
			Check: func(t *testing.T, w *httptest.ResponseRecorder) {
				require.Equal(t, dto.UserWithProfileResponse{
					UserResponse: userResponse(user),
					Profile:      profileResponse(user.Profile),
				}, decodeBody[dto.UserWithProfileResponse](t, w))
			},
		},
	})
}

func TestGetMeHandler(t *testing.T) {
	t.Parallel()
	user := newUserWithProfile()
	token := genAccessToken(t, user.ID, testSecret)

	runUserHandlerTests(t, http.MethodGet, []userHandlerTest{
		{
			Name:         "no token",
			Path:         "/api/v1/users/me",
			CodeExpected: http.StatusUnauthorized,
			KeyExpected:  model.AuthErrorInvalidTokenKey,
		},
		{
			Name:         "expired token",
			Path:         "/api/v1/users/me",
			Token:        genExpiredToken(t, user.ID),
			CodeExpected: http.StatusUnauthorized,
			KeyExpected:  model.AuthErrorTokenExpiredKey,
		},
		{
			Name:  "user deleted",
			Path:  "/api/v1/users/me",
			Token: token,
			Setup: func(d *deps) {
				d.user.EXPECT().GetUserByID(mock.Anything, user.ID, false).
					Return(nil, fmt.Errorf("user not found: %w", model.ErrNotFound))
			},
			CodeExpected: http.StatusNotFound,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:  "success with profile",
			Path:  "/api/v1/users/me?withProfile=true",
			Token: token,
			Setup: func(d *deps) {
				d.user.EXPECT().GetUserByID(mock.Anything, user.ID, true).Return(user, nil)
			},
			CodeExpected: http.StatusOK,
			Check: func(t *testing.T, w *httptest.ResponseRecorder) {
				require.Equal(t, dto.UserWithProfileResponse{
					UserResponse: userResponse(user),
					Profile:      profileResponse(user.Profile),
				}, decodeBody[dto.UserWithProfileResponse](t, w))
			},
		},
	})
}

func TestUpdateUserProfileHandler(t *testing.T) {
	t.Parallel()
	user := newUserWithProfile()
	token := genAccessToken(t, user.ID, testSecret)
	path := "/api/v1/users/me/profile"
	body := dto.UpdateUserProfileRequestBody{
		Age:    new(uint(21)),
		Gender: new("female"),
	}
	input := &model.UpdateUserProfileInput{
		Age:    body.Age,
		Gender: body.Gender,
	}
	updated := &model.UserProfile{
		UserID:   user.ID,
		Age:      new(uint(21)),
		Gender:   new(model.GenderFemale),
		HeightCm: user.Profile.HeightCm,
		WeightKg: user.Profile.WeightKg,
	}

	runUserHandlerTests(t, http.MethodPatch, []userHandlerTest{
		{
			Name:         "no token",
			Path:         path,
			Body:         body,
			CodeExpected: http.StatusUnauthorized,
			KeyExpected:  model.AuthErrorInvalidTokenKey,
		},
		{
			Name:         "invalid json",
			Path:         path,
			Body:         `{"age":"old"}`,
			Token:        token,
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:         "invalid gender",
			Path:         path,
			Body:         dto.UpdateUserProfileRequestBody{Gender: new("other")},
			Token:        token,
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:         "age out of range",
			Path:         path,
			Body:         dto.UpdateUserProfileRequestBody{Age: new(uint(151))},
			Token:        token,
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:         "zero weight",
			Path:         path,
			Body:         dto.UpdateUserProfileRequestBody{WeightKg: new(uint(0))},
			Token:        token,
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:  "service error",
			Path:  path,
			Body:  body,
			Token: token,
			Setup: func(d *deps) {
				d.user.EXPECT().UpdateUserProfile(mock.Anything, user.ID, input).
					Return(nil, errors.New("db is down"))
			},
			CodeExpected: http.StatusInternalServerError,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:  "success",
			Path:  path,
			Body:  body,
			Token: token,
			Setup: func(d *deps) {
				d.user.EXPECT().UpdateUserProfile(mock.Anything, user.ID, input).Return(updated, nil)
			},
			CodeExpected: http.StatusOK,
			Check: func(t *testing.T, w *httptest.ResponseRecorder) {
				require.Equal(t, *profileResponse(updated), decodeBody[dto.UserProfileResponse](t, w))
			},
		},
	})
}

// handlers behind auth middleware must not reach the service without user ID in context
func TestUserHandlersWithoutUserID(t *testing.T) {
	t.Parallel()
	tests := []struct {
		Name    string
		Method  string
		Path    string
		Handler func(*UserHandler) gin.HandlerFunc
	}{
		{
			Name:    "get me",
			Method:  http.MethodGet,
			Path:    "/api/v1/users/me",
			Handler: func(h *UserHandler) gin.HandlerFunc { return h.GetMe },
		},
		{
			Name:    "update profile",
			Method:  http.MethodPatch,
			Path:    "/api/v1/users/me/profile",
			Handler: func(h *UserHandler) gin.HandlerFunc { return h.UpdateUserProfile },
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			t.Parallel()
			d := newDeps(t)
			r := gin.New()
			r.Handle(test.Method, test.Path, test.Handler(NewUserHandler(d.user)))

			w := doRequest(t, r, test.Method, test.Path, `{}`, "")

			requireError(t, w, http.StatusInternalServerError, model.DefaultErrorKey)
		})
	}
}
