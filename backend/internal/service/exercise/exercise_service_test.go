package exercise

import (
	"context"
	"ftns-asst/internal/config"
	"ftns-asst/internal/model"
	"ftns-asst/internal/repository"
	"ftns-asst/internal/util"
	"ftns-asst/internal/util/testutil"
	"maps"
	"slices"
	"testing"

	"github.com/go-openapi/testify/v2/require"
	"github.com/google/uuid"
	"github.com/stretchr/testify/mock"
)

type deps struct {
	repo *MockExerciseRepo
}

func TestGetMuscleGroups(t *testing.T) {
	t.Parallel()

	svc := NewService(&config.Config{}, nil, nil)
	res := svc.GetMuscleGroups()

	require.ElementsMatch(t, slices.Collect(maps.Keys(model.MuscleGroupList)), res)
}

func TestGetEquipment(t *testing.T) {
	t.Parallel()

	svc := NewService(&config.Config{}, nil, nil)
	res := svc.GetEquipment()

	require.ElementsMatch(t, slices.Collect(maps.Keys(model.EquipmentList)), res)
}

func TestCreateExercise(t *testing.T) {
	t.Parallel()
	userID := uuid.New()
	exercise := &model.Exercise{Name: "Push ups"}
	created := &model.Exercise{ID: uuid.New(), Name: "Push ups"}

	d := deps{repo: NewMockExerciseRepo(t)}
	d.repo.EXPECT().CreateExercise(mock.Anything, exercise).
		Return(created, nil)

	svc := NewService(&config.Config{}, d.repo, nil)
	res, err := svc.CreateExercise(context.Background(), userID, exercise)
	require.NoError(t, err)
	require.Equal(t, created, res)
}

func TestGetExerciseByID(t *testing.T) {
	t.Parallel()
	userID := uuid.New()
	exerciseID := uuid.New()
	otherUserID := uuid.New()

	tests := []struct {
		Name           string
		setupMock      func(d deps)
		ErrorExpected  error
		ResultExpected *model.Exercise
	}{
		{
			Name: "forbidden when not owner",
			setupMock: func(d deps) {
				d.repo.EXPECT().GetExerciseByID(mock.Anything, exerciseID).
					Return(&model.Exercise{ID: exerciseID, OwnerID: &otherUserID}, nil)
			},
			ErrorExpected: model.ErrForbidden,
		},
		{
			Name: "success basic exercise without owner",
			setupMock: func(d deps) {
				d.repo.EXPECT().GetExerciseByID(mock.Anything, exerciseID).
					Return(&model.Exercise{ID: exerciseID}, nil)
			},
			ResultExpected: &model.Exercise{ID: exerciseID},
		},
		{
			Name: "success own exercise",
			setupMock: func(d deps) {
				d.repo.EXPECT().GetExerciseByID(mock.Anything, exerciseID).
					Return(&model.Exercise{ID: exerciseID, OwnerID: &userID}, nil)
			},
			ResultExpected: &model.Exercise{ID: exerciseID, OwnerID: &userID},
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			t.Parallel()
			d := deps{repo: NewMockExerciseRepo(t)}
			test.setupMock(d)

			svc := NewService(&config.Config{}, d.repo, nil)
			res, err := svc.GetExerciseByID(context.Background(), userID, exerciseID)
			if test.ErrorExpected != nil {
				require.ErrorIs(t, err, test.ErrorExpected)
				return
			}
			require.NoError(t, err)
			require.Equal(t, test.ResultExpected, res)
		})
	}
}

func TestGetExercises(t *testing.T) {
	t.Parallel()
	list := []*model.Exercise{{ID: uuid.New()}}

	tests := []struct {
		Name             string
		Filters          *model.ExerciseFilters
		setupMock        func(d deps)
		ErrorExpected    error
		ErrorKeyExpected *string
	}{
		{
			Name:             "error user id required for own list type",
			Filters:          &model.ExerciseFilters{ListType: model.ExerciseListTypeOwn},
			ErrorExpected:    model.ErrBadRequest,
			ErrorKeyExpected: util.Ptr(model.ExercisesFilterUserIDRequiredKey),
		},
		{
			Name:    "success",
			Filters: &model.ExerciseFilters{ListType: model.ExerciseListTypeBasic},
			setupMock: func(d deps) {
				d.repo.EXPECT().GetExercises(mock.Anything, mock.Anything, mock.Anything).
					Return(list, nil)
			},
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			t.Parallel()
			d := deps{repo: NewMockExerciseRepo(t)}
			if test.setupMock != nil {
				test.setupMock(d)
			}

			svc := NewService(&config.Config{}, d.repo, nil)
			res, err := svc.GetExercises(context.Background(), test.Filters,
				&model.PageInfo{Limit: util.Ptr(10), Offset: util.Ptr(0)})
			if test.ErrorExpected != nil {
				require.ErrorIs(t, err, test.ErrorExpected)
				if test.ErrorKeyExpected != nil {
					testutil.EqualSvcErrKey(t, err, *test.ErrorKeyExpected)
				}
				return
			}
			require.NoError(t, err)
			require.Equal(t, list, res)
		})
	}
}

func TestUpdateExercise(t *testing.T) {
	t.Parallel()
	userID := uuid.New()
	exerciseID := uuid.New()
	input := &model.UpdateExerciseInput{ID: exerciseID}
	updated := &model.Exercise{ID: exerciseID}

	tests := []struct {
		Name          string
		setupMock     func(d deps)
		ErrorExpected error
	}{
		{
			Name: "forbidden when not owner",
			setupMock: func(d deps) {
				d.repo.EXPECT().IsUserOwnerCheck(mock.Anything, userID, exerciseID).
					Return(false, nil)
			},
			ErrorExpected: model.ErrForbidden,
		},
		{
			Name: "not found when update target missing",
			setupMock: func(d deps) {
				d.repo.EXPECT().IsUserOwnerCheck(mock.Anything, userID, exerciseID).
					Return(true, nil)
				d.repo.EXPECT().UpdateExercise(mock.Anything, input).
					Return(nil, repository.ErrNotFound)
			},
			ErrorExpected: model.ErrNotFound,
		},
		{
			Name: "success",
			setupMock: func(d deps) {
				d.repo.EXPECT().IsUserOwnerCheck(mock.Anything, userID, exerciseID).
					Return(true, nil)
				d.repo.EXPECT().UpdateExercise(mock.Anything, input).
					Return(updated, nil)
			},
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			t.Parallel()
			d := deps{repo: NewMockExerciseRepo(t)}
			test.setupMock(d)

			svc := NewService(&config.Config{}, d.repo, nil)
			res, err := svc.UpdateExercise(context.Background(), userID, input)
			if test.ErrorExpected != nil {
				require.ErrorIs(t, err, test.ErrorExpected)
				return
			}
			require.NoError(t, err)
			require.Equal(t, updated, res)
		})
	}
}

func TestDeleteExercise(t *testing.T) {
	t.Parallel()
	userID := uuid.New()
	exerciseID := uuid.New()

	tests := []struct {
		Name          string
		setupMock     func(d deps)
		ErrorExpected error
	}{
		{
			Name: "forbidden when not owner",
			setupMock: func(d deps) {
				d.repo.EXPECT().IsUserOwnerCheck(mock.Anything, userID, exerciseID).
					Return(false, nil)
			},
			ErrorExpected: model.ErrForbidden,
		},
		{
			Name: "not found when no rows affected",
			setupMock: func(d deps) {
				d.repo.EXPECT().IsUserOwnerCheck(mock.Anything, userID, exerciseID).
					Return(true, nil)
				d.repo.EXPECT().DeleteExercise(mock.Anything, exerciseID).
					Return(repository.ErrNoAffectedRows)
			},
			ErrorExpected: model.ErrNotFound,
		},
		{
			Name: "success",
			setupMock: func(d deps) {
				d.repo.EXPECT().IsUserOwnerCheck(mock.Anything, userID, exerciseID).
					Return(true, nil)
				d.repo.EXPECT().DeleteExercise(mock.Anything, exerciseID).
					Return(nil)
			},
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			t.Parallel()
			d := deps{repo: NewMockExerciseRepo(t)}
			test.setupMock(d)

			svc := NewService(&config.Config{}, d.repo, nil)
			err := svc.DeleteExercise(context.Background(), userID, exerciseID)
			if test.ErrorExpected != nil {
				require.ErrorIs(t, err, test.ErrorExpected)
				return
			}
			require.NoError(t, err)
		})
	}
}
