package rest

import (
	"encoding/json"
	"fmt"
	"ftns-asst/internal/model"
	"ftns-asst/internal/rest/dto"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/google/uuid"
	"github.com/stretchr/testify/mock"
	"github.com/stretchr/testify/require"
)

func newExercise(ownerID *uuid.UUID) *model.Exercise {
	return &model.Exercise{
		ID:           uuid.New(),
		OwnerID:      ownerID,
		IsPublic:     true,
		Name:         "Жим штанги лёжа",
		Description:  new("description"),
		MuscleGroups: []model.MuscleGroupKey{model.MuscleGroupChest, model.MuscleGroupTriceps},
		Equipments:   []model.EquipmentKey{model.EquipmentBarbell, model.EquipmentBench},
		CreatedAt:    time.Now().Add(-time.Hour).UTC().Truncate(time.Second),
	}
}

func exerciseResponse(e *model.Exercise) dto.ExerciseDTO {
	return dto.ExerciseDTO{
		ID:           e.ID,
		Name:         e.Name,
		Description:  e.Description,
		OwnerID:      e.OwnerID,
		IsPublic:     e.IsPublic,
		MuscleGroups: []string{"chest", "triceps"},
		Equipment:    []string{"barbell", "bench"},
		CreatedAt:    e.CreatedAt,
	}
}

func checkExerciseResponse(e *model.Exercise) func(*testing.T, *httptest.ResponseRecorder) {
	return func(t *testing.T, w *httptest.ResponseRecorder) {
		require.Equal(t, exerciseResponse(e), decodeBody[dto.ExerciseDTO](t, w))
	}
}

func TestGetMuscleGroupsHandler(t *testing.T) {
	t.Parallel()

	runUserHandlerTests(t, http.MethodGet, []userHandlerTest{
		{
			Name: "success without token",
			Path: "/api/v1/muscle-groups",
			Setup: func(d *deps) {
				d.exercise.EXPECT().GetMuscleGroups().Return([]model.MuscleGroup{
					{Key: model.MuscleGroupChest, Name: "Грудные"},
					{Key: model.MuscleGroupAbs, Name: "Пресс"},
				})
			},
			CodeExpected: http.StatusOK,
			Check: func(t *testing.T, w *httptest.ResponseRecorder) {
				require.JSONEq(t, `[{"key":"chest","name":"Грудные"},{"key":"abs","name":"Пресс"}]`, w.Body.String())
			},
		},
		{
			Name: "empty list",
			Path: "/api/v1/muscle-groups",
			Setup: func(d *deps) {
				d.exercise.EXPECT().GetMuscleGroups().Return(nil)
			},
			CodeExpected: http.StatusOK,
			Check: func(t *testing.T, w *httptest.ResponseRecorder) {
				require.JSONEq(t, `[]`, w.Body.String())
			},
		},
	})
}

func TestGetEquipmentsHandler(t *testing.T) {
	t.Parallel()

	runUserHandlerTests(t, http.MethodGet, []userHandlerTest{
		{
			Name: "success without token",
			Path: "/api/v1/equipments",
			Setup: func(d *deps) {
				d.exercise.EXPECT().GetEquipment().Return([]model.Equipment{
					{Key: model.EquipmentBarbell, Name: "Штанга"},
				})
			},
			CodeExpected: http.StatusOK,
			Check: func(t *testing.T, w *httptest.ResponseRecorder) {
				require.JSONEq(t, `[{"key":"barbell","name":"Штанга"}]`, w.Body.String())
			},
		},
	})
}

func TestListExercisesHandler(t *testing.T) {
	t.Parallel()
	userID := uuid.New()
	token := genAccessToken(t, userID, testSecret)
	exercise := newExercise(nil)

	runUserHandlerTests(t, http.MethodGet, []userHandlerTest{
		{
			Name:         "missing type",
			Path:         "/api/v1/exercises?limit=10&offset=0",
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:         "unknown type",
			Path:         "/api/v1/exercises?type=other&limit=10&offset=0",
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:         "limit out of range",
			Path:         "/api/v1/exercises?type=basic&limit=1001&offset=0",
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:         "negative offset",
			Path:         "/api/v1/exercises?type=basic&limit=10&offset=-1",
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:         "expired token",
			Path:         "/api/v1/exercises?type=basic&limit=10&offset=0",
			Token:        genExpiredToken(t, userID),
			CodeExpected: http.StatusUnauthorized,
			KeyExpected:  model.AuthErrorTokenExpiredKey,
		},
		{
			Name: "filters without token",
			Path: "/api/v1/exercises?type=basic&muscle_group=chest&muscle_group=abs&equipment=barbell&search=жим&limit=10&offset=5",
			Setup: func(d *deps) {
				d.exercise.EXPECT().GetExercises(mock.Anything,
					&model.ExerciseFilters{
						ListType:     model.ExerciseListTypeBasic,
						MuscleGroups: []model.MuscleGroupKey{model.MuscleGroupChest, model.MuscleGroupAbs},
						Equipments:   []model.EquipmentKey{model.EquipmentBarbell},
						Search:       new("жим"),
					},
					&model.PageInfo{Limit: new(10), Offset: new(5)}).
					Return([]*model.Exercise{exercise}, nil)
			},
			CodeExpected: http.StatusOK,
			Check: func(t *testing.T, w *httptest.ResponseRecorder) {
				require.Equal(t, []dto.ExerciseDTO{exerciseResponse(exercise)}, decodeBody[[]dto.ExerciseDTO](t, w))
			},
		},
		{
			Name:  "user id from token",
			Path:  "/api/v1/exercises?type=own",
			Token: token,
			Setup: func(d *deps) {
				d.exercise.EXPECT().GetExercises(mock.Anything,
					mock.MatchedBy(func(f *model.ExerciseFilters) bool {
						return f.ListType == model.ExerciseListTypeOwn && f.UserID != nil && *f.UserID == userID
					}),
					&model.PageInfo{}).
					Return(nil, nil)
			},
			CodeExpected: http.StatusOK,
			Check: func(t *testing.T, w *httptest.ResponseRecorder) {
				require.JSONEq(t, `[]`, w.Body.String())
			},
		},
		{
			Name: "service error with key",
			Path: "/api/v1/exercises?type=own",
			Setup: func(d *deps) {
				d.exercise.EXPECT().GetExercises(mock.Anything,
					mock.MatchedBy(func(f *model.ExerciseFilters) bool { return f.UserID == nil }),
					mock.Anything).
					Return(nil, model.ExercisesFilterUserIDRequired())
			},
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.ExercisesFilterUserIDRequiredKey,
		},
		{
			Name: "internal error",
			Path: "/api/v1/exercises?type=basic",
			Setup: func(d *deps) {
				d.exercise.EXPECT().GetExercises(mock.Anything, mock.Anything, mock.Anything).
					Return(nil, fmt.Errorf("db is down"))
			},
			CodeExpected: http.StatusInternalServerError,
			KeyExpected:  model.DefaultErrorKey,
		},
	})
}

func TestCreateExerciseHandler(t *testing.T) {
	t.Parallel()
	userID := uuid.New()
	token := genAccessToken(t, userID, testSecret)
	created := newExercise(&userID)
	path := "/api/v1/exercises"
	body := dto.CreateExerciseRequestBody{
		Name:         created.Name,
		IsPublic:     true,
		Description:  created.Description,
		MuscleGroups: []string{"chest", "triceps"},
		Equipment:    []string{"barbell", "bench"},
	}

	runUserHandlerTests(t, http.MethodPost, []userHandlerTest{
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
			Body:         `{"name":`,
			Token:        token,
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:  "success",
			Path:  path,
			Body:  body,
			Token: token,
			Setup: func(d *deps) {
				d.exercise.EXPECT().CreateExercise(mock.Anything, userID, &model.Exercise{
					OwnerID:      &userID,
					IsPublic:     true,
					Name:         created.Name,
					Description:  created.Description,
					MuscleGroups: created.MuscleGroups,
					Equipments:   created.Equipments,
				}).Return(created, nil)
			},
			CodeExpected: http.StatusOK,
			Check:        checkExerciseResponse(created),
		},
		{
			Name:  "validation error",
			Path:  path,
			Body:  dto.CreateExerciseRequestBody{Name: "Push ups", MuscleGroups: []string{"unknown"}},
			Token: token,
			Setup: func(d *deps) {
				d.exercise.EXPECT().CreateExercise(mock.Anything, userID, mock.Anything).
					Return(nil, fmt.Errorf("unknown muscle group: %w", model.ErrBadRequest))
			},
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
	})
}

func TestGetExerciseByIDHandler(t *testing.T) {
	t.Parallel()
	userID := uuid.New()
	token := genAccessToken(t, userID, testSecret)
	exercise := newExercise(&userID)
	path := "/api/v1/exercises/" + exercise.ID.String()

	runUserHandlerTests(t, http.MethodGet, []userHandlerTest{
		{
			Name:         "no token",
			Path:         path,
			CodeExpected: http.StatusUnauthorized,
			KeyExpected:  model.AuthErrorInvalidTokenKey,
		},
		{
			Name:         "invalid id",
			Path:         "/api/v1/exercises/123",
			Token:        token,
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:  "success",
			Path:  path,
			Token: token,
			Setup: func(d *deps) {
				d.exercise.EXPECT().GetExerciseByID(mock.Anything, userID, exercise.ID).Return(exercise, nil)
			},
			CodeExpected: http.StatusOK,
			Check:        checkExerciseResponse(exercise),
		},
		{
			Name:  "forbidden",
			Path:  path,
			Token: token,
			Setup: func(d *deps) {
				d.exercise.EXPECT().GetExerciseByID(mock.Anything, userID, exercise.ID).Return(nil, model.ErrForbidden)
			},
			CodeExpected: http.StatusForbidden,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:  "not found",
			Path:  path,
			Token: token,
			Setup: func(d *deps) {
				d.exercise.EXPECT().GetExerciseByID(mock.Anything, userID, exercise.ID).Return(nil, model.ErrNotFound)
			},
			CodeExpected: http.StatusNotFound,
			KeyExpected:  model.DefaultErrorKey,
		},
	})
}

func TestUpdateExerciseHandler(t *testing.T) {
	t.Parallel()
	userID := uuid.New()
	token := genAccessToken(t, userID, testSecret)
	updated := newExercise(&userID)
	path := "/api/v1/exercises/" + updated.ID.String()
	body := dto.UpdateExerciseRequestBody{
		ID: updated.ID,
		ExerciseInfoDTO: dto.ExerciseInfoDTO{
			Name:         new(updated.Name),
			MuscleGroups: []string{"chest", "triceps"},
		},
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
			Name:         "invalid id",
			Path:         "/api/v1/exercises/123",
			Body:         body,
			Token:        token,
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:         "invalid json",
			Path:         path,
			Body:         `{"id":`,
			Token:        token,
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:         "body id differs from path id",
			Path:         "/api/v1/exercises/" + uuid.NewString(),
			Body:         body,
			Token:        token,
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:  "success",
			Path:  path,
			Body:  body,
			Token: token,
			Setup: func(d *deps) {
				d.exercise.EXPECT().UpdateExercise(mock.Anything, userID, &model.UpdateExerciseInput{
					ID: updated.ID,
					ExerciseInput: model.ExerciseInput{
						Name:         new(updated.Name),
						MuscleGroups: []model.MuscleGroupKey{model.MuscleGroupChest, model.MuscleGroupTriceps},
					},
				}).Return(updated, nil)
			},
			CodeExpected: http.StatusOK,
			Check:        checkExerciseResponse(updated),
		},
		{
			Name:  "forbidden",
			Path:  path,
			Body:  body,
			Token: token,
			Setup: func(d *deps) {
				d.exercise.EXPECT().UpdateExercise(mock.Anything, userID, mock.Anything).Return(nil, model.ErrForbidden)
			},
			CodeExpected: http.StatusForbidden,
			KeyExpected:  model.DefaultErrorKey,
		},
	})
}

func TestDeleteExerciseHandler(t *testing.T) {
	t.Parallel()
	userID := uuid.New()
	token := genAccessToken(t, userID, testSecret)
	exerciseID := uuid.New()
	path := "/api/v1/exercises/" + exerciseID.String()

	runUserHandlerTests(t, http.MethodDelete, []userHandlerTest{
		{
			Name:         "no token",
			Path:         path,
			CodeExpected: http.StatusUnauthorized,
			KeyExpected:  model.AuthErrorInvalidTokenKey,
		},
		{
			Name:         "invalid id",
			Path:         "/api/v1/exercises/123",
			Token:        token,
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:  "success",
			Path:  path,
			Token: token,
			Setup: func(d *deps) {
				d.exercise.EXPECT().DeleteExercise(mock.Anything, userID, exerciseID).Return(nil)
			},
			CodeExpected: http.StatusOK,
			Check: func(t *testing.T, w *httptest.ResponseRecorder) {
				require.Empty(t, w.Body.String())
			},
		},
		{
			Name:  "not found",
			Path:  path,
			Token: token,
			Setup: func(d *deps) {
				d.exercise.EXPECT().DeleteExercise(mock.Anything, userID, exerciseID).Return(model.ErrNotFound)
			},
			CodeExpected: http.StatusNotFound,
			KeyExpected:  model.DefaultErrorKey,
		},
	})
}

func TestUpdateExerciseMediaHandler(t *testing.T) {
	t.Parallel()
	token := genAccessToken(t, uuid.New(), testAdminSecret)
	exerciseID := uuid.New()
	path := "/api/v1/admin/exercise/" + exerciseID.String() + "/media"
	body := dto.UpdateExerciseMediaRequestBody{MediaType: "image"}
	media := &model.Media{ID: uuid.New(), Type: model.MediaTypeImage, ObjectKey: "a"}

	runUserHandlerTests(t, http.MethodPut, []userHandlerTest{
		{
			Name:         "no token",
			Path:         path,
			Body:         body,
			CodeExpected: http.StatusUnauthorized,
			KeyExpected:  model.AuthErrorInvalidTokenKey,
		},
		{
			Name:         "invalid id",
			Path:         "/api/v1/admin/exercise/123/media",
			Body:         body,
			Token:        token,
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:         "invalid media type",
			Path:         path,
			Body:         dto.UpdateExerciseMediaRequestBody{MediaType: "audio"},
			Token:        token,
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:  "success",
			Path:  path,
			Body:  body,
			Token: token,
			Setup: func(d *deps) {
				d.exercise.EXPECT().UpdateMedia(mock.Anything, exerciseID, model.MediaTypeImage).
					Return(media, "http://put", nil)
			},
			CodeExpected: http.StatusOK,
			Check: func(t *testing.T, w *httptest.ResponseRecorder) {
				var res dto.UpdateExerciseMediaResponse
				require.NoError(t, json.Unmarshal(w.Body.Bytes(), &res))
				require.Equal(t, media.ID, res.Media.ID)
				require.Equal(t, "image", res.Media.Type)
				require.Equal(t, "http://put", res.UploadURL)
			},
		},
		{
			Name:  "not found",
			Path:  path,
			Body:  body,
			Token: token,
			Setup: func(d *deps) {
				d.exercise.EXPECT().UpdateMedia(mock.Anything, exerciseID, model.MediaTypeImage).
					Return(nil, "", model.ErrNotFound)
			},
			CodeExpected: http.StatusNotFound,
			KeyExpected:  model.DefaultErrorKey,
		},
	})
}

func TestDeleteExerciseMediaHandler(t *testing.T) {
	t.Parallel()
	token := genAccessToken(t, uuid.New(), testAdminSecret)
	exerciseID := uuid.New()
	path := "/api/v1/admin/exercise/" + exerciseID.String() + "/media"

	runUserHandlerTests(t, http.MethodDelete, []userHandlerTest{
		{
			Name:         "no token",
			Path:         path,
			CodeExpected: http.StatusUnauthorized,
			KeyExpected:  model.AuthErrorInvalidTokenKey,
		},
		{
			Name:         "invalid id",
			Path:         "/api/v1/admin/exercise/123/media",
			Token:        token,
			CodeExpected: http.StatusBadRequest,
			KeyExpected:  model.DefaultErrorKey,
		},
		{
			Name:  "success",
			Path:  path,
			Token: token,
			Setup: func(d *deps) {
				d.exercise.EXPECT().DeleteMedia(mock.Anything, exerciseID).Return(nil)
			},
			CodeExpected: http.StatusOK,
			Check: func(t *testing.T, w *httptest.ResponseRecorder) {
				require.Empty(t, w.Body.String())
			},
		},
		{
			Name:  "not found",
			Path:  path,
			Token: token,
			Setup: func(d *deps) {
				d.exercise.EXPECT().DeleteMedia(mock.Anything, exerciseID).Return(model.ErrNotFound)
			},
			CodeExpected: http.StatusNotFound,
			KeyExpected:  model.DefaultErrorKey,
		},
	})
}
