package users

import (
	"context"
	"errors"
	"ftns-asst/internal/config"
	"ftns-asst/internal/model"
	"ftns-asst/internal/repository"
	"ftns-asst/internal/util"
	"ftns-asst/internal/util/testutil"
	"testing"
	"time"

	"github.com/google/uuid"
	"github.com/stretchr/testify/mock"
	"github.com/stretchr/testify/require"
)

type (
	deps struct {
		repo *MockUserRepo
		tx   *testutil.MockTransactor
	}

	testData struct {
		cfg  *config.Config
		user *model.UserWithProfile
	}
)

func newDeps(t *testing.T) *deps {
	return &deps{
		repo: NewMockUserRepo(t),
		tx:   &testutil.MockTransactor{},
	}
}

func newTestData(t *testing.T) testData {
	passwordHash, err := util.Hash("somePass84")
	require.NoError(t, err)
	user := &model.User{
		ID:           uuid.New(),
		Name:         "John",
		Email:        "test@example.com",
		PasswordHash: passwordHash,
		CreatedAt:    time.Now().Add(-time.Hour),
	}
	profile := &model.UserProfile{
		UserID:   user.ID,
		Age:      new(uint(20)),
		Gender:   new(model.GenderMale),
		HeightCm: new(uint(170)),
		WeightKg: new(uint(65)),
	}

	return testData{
		cfg: &config.Config{},
		user: &model.UserWithProfile{
			User:    *user,
			Profile: profile,
		},
	}
}

func TestCheckUserWithEmailExists(t *testing.T) {
	t.Parallel()
	td := newTestData(t)
	someError := errors.New("some error")
	tests := []struct {
		Name           string
		Email          string
		Setup          func(*deps)
		ExistsExpected bool
		ErrorExpected  error
	}{
		{
			Name:  "email exists",
			Email: td.user.Email,
			Setup: func(d *deps) {
				d.repo.EXPECT().CheckUserWithEmailExists(mock.Anything, td.user.Email).
					Return(true, nil)
			},
			ExistsExpected: true,
			ErrorExpected:  nil,
		},
		{
			Name:  "free email",
			Email: td.user.Email,
			Setup: func(d *deps) {
				d.repo.EXPECT().CheckUserWithEmailExists(mock.Anything, td.user.Email).
					Return(false, nil)
			},
			ExistsExpected: false,
			ErrorExpected:  nil,
		},
		{
			Name:  "error occurred",
			Email: td.user.Email,
			Setup: func(d *deps) {
				d.repo.EXPECT().CheckUserWithEmailExists(mock.Anything, td.user.Email).
					Return(false, someError)
			},
			ExistsExpected: false,
			ErrorExpected:  someError,
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			deps := newDeps(t)
			if test.Setup != nil {
				test.Setup(deps)
			}
			svc := NewService(td.cfg, deps.repo, deps.tx)

			res, err := svc.CheckUserWithEmailExists(context.Background(), test.Email)
			if test.ErrorExpected != nil {
				require.ErrorIs(t, err, test.ErrorExpected)
				return
			}
			require.NoError(t, err)
			require.Equal(t, test.ExistsExpected, res)
		})
	}
}

func TestGetUserByID(t *testing.T) {
	t.Parallel()
	td := newTestData(t)
	someUuid := uuid.New()
	tests := []struct {
		Name           string
		UserID         uuid.UUID
		WithProfile    bool
		Setup          func(*deps)
		ResultExpected *model.UserWithProfile
		ErrorExpected  error
	}{
		{
			Name:        "user without profile not found",
			UserID:      someUuid,
			WithProfile: false,
			Setup: func(d *deps) {
				d.repo.EXPECT().GetUserByID(mock.Anything, someUuid).
					Return(nil, repository.ErrNotFound)
			},
			ErrorExpected: model.ErrNotFound,
		},
		{
			Name:        "success user without profile",
			UserID:      td.user.ID,
			WithProfile: false,
			Setup: func(d *deps) {
				d.repo.EXPECT().GetUserByID(mock.Anything, td.user.ID).
					Return(&td.user.User, nil)
			},
			ResultExpected: &model.UserWithProfile{
				User: td.user.User,
			},
			ErrorExpected: nil,
		},
		{
			Name:        "user with profile not found",
			UserID:      someUuid,
			WithProfile: true,
			Setup: func(d *deps) {
				d.repo.EXPECT().GetUserWithProfileByID(mock.Anything, someUuid).
					Return(nil, repository.ErrNotFound)
			},
			ErrorExpected: model.ErrNotFound,
		},
		{
			Name:        "success user with profile",
			UserID:      td.user.ID,
			WithProfile: true,
			Setup: func(d *deps) {
				d.repo.EXPECT().GetUserWithProfileByID(mock.Anything, td.user.ID).
					Return(td.user, nil)
			},
			ResultExpected: td.user,
			ErrorExpected:  nil,
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			deps := newDeps(t)
			if test.Setup != nil {
				test.Setup(deps)
			}
			svc := NewService(td.cfg, deps.repo, deps.tx)

			res, err := svc.GetUserByID(context.Background(), test.UserID, test.WithProfile)
			if test.ErrorExpected != nil {
				require.ErrorIs(t, err, test.ErrorExpected)
				require.Nil(t, res)
				return
			}
			require.NoError(t, err)
			require.Equal(t, test.ResultExpected, res)
		})
	}
}

func TestGetUserByEmail(t *testing.T) {
	t.Parallel()
	td := newTestData(t)

	tests := []struct {
		Name           string
		Email          string
		Setup          func(*deps)
		ResultExpected *model.User
		ErrorExpected  error
	}{
		{
			Name:  "not found user by email",
			Email: td.user.Email,
			Setup: func(d *deps) {
				d.repo.EXPECT().GetUserByEmail(mock.Anything, td.user.Email).
					Return(nil, repository.ErrNotFound)
			},
			ErrorExpected: model.ErrNotFound,
		},
		{
			Name:  "success get user by email",
			Email: td.user.Email,
			Setup: func(d *deps) {
				d.repo.EXPECT().GetUserByEmail(mock.Anything, td.user.Email).
					Return(&td.user.User, nil)
			},
			ResultExpected: &td.user.User,
			ErrorExpected:  nil,
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			deps := newDeps(t)
			if test.Setup != nil {
				test.Setup(deps)
			}
			svc := NewService(td.cfg, deps.repo, deps.tx)

			res, err := svc.GetUserByEmail(context.Background(), test.Email)
			if test.ErrorExpected != nil {
				require.ErrorIs(t, err, test.ErrorExpected)
				require.Nil(t, res)
				return
			}
			require.NoError(t, err)
			require.Equal(t, test.ResultExpected, res)
		})
	}
}

func TestCreateUser(t *testing.T) {
	t.Parallel()
	td := newTestData(t)
	userInput := &model.User{
		Email:        td.user.Email,
		Name:         td.user.Name,
		PasswordHash: td.user.PasswordHash,
		CreatedAt:    td.user.CreatedAt,
	}
	//someError := errors.New("some error")
	tests := []struct {
		Name           string
		User           *model.User
		Setup          func(*deps)
		ResultExpected *model.User
		ErrorExpected  error
	}{
		{
			Name: "success create user",
			User: userInput,
			Setup: func(d *deps) {
				d.repo.EXPECT().CreateUser(mock.Anything, userInput).
					RunAndReturn(func(ctx context.Context, user *model.User) (*model.User, error) {
						user.ID = td.user.ID
						user.CreatedAt = td.user.CreatedAt
						return user, nil
					})
			},
			ResultExpected: &td.user.User,
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			deps := newDeps(t)
			if test.Setup != nil {
				test.Setup(deps)
			}
			svc := NewService(td.cfg, deps.repo, deps.tx)

			res, err := svc.CreateUser(context.Background(), test.User)
			if test.ErrorExpected != nil {
				require.ErrorIs(t, err, test.ErrorExpected)
				require.Nil(t, res)
				return
			}
			require.NoError(t, err)
			require.Equal(t, test.ResultExpected, res)
		})
	}
}

func TestUpdateUserProfile(t *testing.T) {
	t.Parallel()
	td := newTestData(t)
	input := &model.UpdateUserProfileInput{
		Age:      td.user.Profile.Age,
		Gender:   (*string)(td.user.Profile.Gender),
		HeightCm: td.user.Profile.HeightCm,
		WeightKg: td.user.Profile.WeightKg,
	}

	tests := []struct {
		Name           string
		UserID         uuid.UUID
		Input          *model.UpdateUserProfileInput
		Setup          func(*deps)
		ResultExpected *model.UserProfile
		ErrorExpected  error
	}{
		{
			Name:   "user not found",
			Input:  input,
			UserID: td.user.ID,
			Setup: func(d *deps) {
				d.repo.EXPECT().CreateOrUpdateUserProfile(mock.Anything, td.user.ID, input).
					Return(nil, repository.ErrNotFound)
			},
			ErrorExpected: model.ErrNotFound,
		},
		{
			Name:   "user not found",
			Input:  input,
			UserID: td.user.ID,
			Setup: func(d *deps) {
				d.repo.EXPECT().CreateOrUpdateUserProfile(mock.Anything, td.user.ID, input).
					Return(&model.UserProfile{
						UserID:   td.user.ID,
						Age:      input.Age,
						Gender:   (*model.Gender)(input.Gender),
						HeightCm: input.HeightCm,
						WeightKg: input.WeightKg,
					}, nil)
			},
			ResultExpected: td.user.Profile,
			ErrorExpected:  nil,
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			deps := newDeps(t)
			if test.Setup != nil {
				test.Setup(deps)
			}
			svc := NewService(td.cfg, deps.repo, deps.tx)

			res, err := svc.UpdateUserProfile(context.Background(), test.UserID, test.Input)
			if test.ErrorExpected != nil {
				require.ErrorIs(t, err, test.ErrorExpected)
				require.Nil(t, res)
				return
			}
			require.NoError(t, err)
			require.Equal(t, test.ResultExpected, res)
		})
	}
}

func TestUpdateUserPassword(t *testing.T) {
	t.Parallel()
	td := newTestData(t)

	newPassword := "newPassword"
	newPasswordHash, err := util.Hash(newPassword)
	require.NoError(t, err)

	tests := []struct {
		Name            string
		UserID          uuid.UUID
		NewPasswordHash string
		Setup           func(*deps)
		ErrorExpected   error
	}{
		{
			Name:            "invalid new password hash",
			UserID:          td.user.ID,
			NewPasswordHash: "invalid_hash",
			ErrorExpected:   model.ErrNotFound,
		},
		{
			Name:            "user not found",
			UserID:          td.user.ID,
			NewPasswordHash: newPasswordHash,
			Setup: func(d *deps) {
				d.repo.EXPECT().UpdateUserPassword(mock.Anything, td.user.ID, newPasswordHash).
					Return(repository.ErrNoAffectedRows)
			},
			ErrorExpected: model.ErrNotFound,
		},
		{
			Name:            "success update password",
			UserID:          td.user.ID,
			NewPasswordHash: newPasswordHash,
			Setup: func(d *deps) {
				d.repo.EXPECT().UpdateUserPassword(mock.Anything, td.user.ID, newPasswordHash).
					Return(nil)
			},
			ErrorExpected: nil,
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			deps := newDeps(t)
			if test.Setup != nil {
				test.Setup(deps)
			}
			svc := NewService(td.cfg, deps.repo, deps.tx)

			err := svc.UpdateUserPasswordHash(context.Background(), test.UserID, test.NewPasswordHash)
			if test.ErrorExpected != nil {
				require.Error(t, err)
				return
			}
			require.NoError(t, err)
		})
	}
}
