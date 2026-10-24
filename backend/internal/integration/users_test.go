//go:build integration

package integration

import (
	"context"
	"ftns-asst/internal/model"
	"ftns-asst/internal/rest/dto"
	"net/http"
	"testing"

	"github.com/google/uuid"
	"github.com/stretchr/testify/require"
)

func TestUserProfile(t *testing.T) {
	t.Parallel()
	c := newClient(t)
	signed := signUp(t, c, newEmail())
	c.token = signed.Tokens.AccessToken

	var me dto.UserWithProfileResponse
	c.requireOK(http.MethodGet, "/users/me?withProfile=true", nil, &me)
	require.Equal(t, *signed.User, me.UserResponse)
	require.Nil(t, me.Profile)

	var profile dto.UserProfileResponse
	c.requireOK(http.MethodPatch, "/users/me/profile", dto.UpdateUserProfileRequestBody{
		Age:      new(uint(25)),
		Gender:   new("male"),
		HeightCm: new(uint(180)),
		WeightKg: new(uint(75)),
	}, &profile)
	require.Equal(t, dto.UserProfileResponse{Age: 25, Gender: "male", HeightCm: 180, WeightKg: 75}, profile)

	// partial update keeps other fields
	c.requireOK(http.MethodPatch, "/users/me/profile", dto.UpdateUserProfileRequestBody{
		WeightKg: new(uint(72)),
	}, &profile)
	require.Equal(t, dto.UserProfileResponse{Age: 25, Gender: "male", HeightCm: 180, WeightKg: 72}, profile)

	c.requireOK(http.MethodGet, "/users/me?withProfile=true", nil, &me)
	require.Equal(t, *signed.User, me.UserResponse)
	require.Equal(t, &profile, me.Profile)

	var byID dto.UserWithProfileResponse
	c.requireOK(http.MethodGet, "/users/"+signed.User.ID.String()+"?withProfile=true", nil, &byID)
	require.Equal(t, me, byID)

	byID.Profile = nil
	c.requireOK(http.MethodGet, "/users/"+signed.User.ID.String(), nil, &byID)
	require.Equal(t, *signed.User, byID.UserResponse)
	require.Nil(t, byID.Profile)
}

func TestGetOtherUser(t *testing.T) {
	t.Parallel()
	c := newClient(t)
	other := signUp(t, newClient(t), newEmail())
	c.token = signUp(t, c, newEmail()).Tokens.AccessToken

	var resp dto.UserWithProfileResponse
	c.requireOK(http.MethodGet, "/users/"+other.User.ID.String(), nil, &resp)
	require.Equal(t, *other.User, resp.UserResponse)
}

func TestGetUnknownUser(t *testing.T) {
	t.Parallel()
	c := newClient(t)
	c.token = signUp(t, c, newEmail()).Tokens.AccessToken

	c.requireError(http.MethodGet, "/users/"+uuid.NewString(), nil, http.StatusNotFound, model.DefaultErrorKey)
	c.requireError(http.MethodGet, "/users/"+uuid.NewString()+"?withProfile=true", nil,
		http.StatusNotFound, model.DefaultErrorKey)
}

func TestUsersRequireAuth(t *testing.T) {
	t.Parallel()
	c := newClient(t)

	c.requireError(http.MethodGet, "/users/me", nil, http.StatusUnauthorized, model.AuthErrorInvalidTokenKey)
	c.requireError(http.MethodPatch, "/users/me/profile", dto.UpdateUserProfileRequestBody{Age: new(uint(30))},
		http.StatusUnauthorized, model.AuthErrorInvalidTokenKey)
}

// repositories must use tx from context, so rollback in services discards writes
func TestRepoUsesTxFromContext(t *testing.T) {
	t.Parallel()
	ctx := context.Background()
	email := newEmail()

	tx, err := db.Begin(ctx)
	require.NoError(t, err)
	txCtx := context.WithValue(ctx, model.ContextKeyTx, tx)
	_, err = repos.User().CreateUser(txCtx, &model.User{Email: email, Name: "John", PasswordHash: "hash"})
	require.NoError(t, err)
	require.NoError(t, tx.Rollback(ctx))

	exists, err := repos.User().CheckUserWithEmailExists(ctx, email)
	require.NoError(t, err)
	require.False(t, exists)
}
