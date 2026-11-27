//go:build integration

package integration

import (
	"context"
	"ftns-asst/internal/model"
	"ftns-asst/internal/rest/dto"
	"net/http"
	"testing"
	"time"

	"github.com/google/uuid"
	"github.com/stretchr/testify/require"
)

// there is no request to add records yet, so tests insert them with explicit time
func insertPersonalRecord(t *testing.T, userID, exerciseID uuid.UUID, weightKg float64, isCalculated bool, createdAt time.Time) {
	t.Helper()
	_, err := db.Exec(context.Background(), `
		INSERT INTO personal_records (user_id, exercise_id, weight_kg, is_calculated, created_at)
		VALUES ($1, $2, $3, $4, $5)
	`, userID, exerciseID, weightKg, isCalculated, createdAt)
	require.NoError(t, err)
}

func newUserWithID(t *testing.T) (*client, uuid.UUID) {
	t.Helper()
	c := newClient(t)
	signed := signUp(t, c, newEmail())
	c.token = signed.Tokens.AccessToken
	return c, signed.User.ID
}

func TestPersonalRecords(t *testing.T) {
	t.Parallel()
	exercises := basicExerciseIDs(t, 3)
	bench, squat, unused := exercises[0], exercises[1], exercises[2]
	c, userID := newUserWithID(t)
	prsPath := "/users/" + userID.String() + "/prs"

	// a user without records gets an empty list, not null
	var list []dto.PersonalRecordDTO
	c.requireOK(http.MethodGet, prsPath, nil, &list)
	require.NotNil(t, list)
	require.Empty(t, list)

	now := time.Now().UTC().Truncate(time.Millisecond)
	insertPersonalRecord(t, userID, bench, 100, false, now.Add(-48*time.Hour))
	insertPersonalRecord(t, userID, bench, 116.67, true, now.Add(-24*time.Hour))
	insertPersonalRecord(t, userID, bench, 110, false, now.Add(-72*time.Hour))
	insertPersonalRecord(t, userID, squat, 140, false, now)

	// the latest record in each exercise is returned, by time and not by weight
	c.requireOK(http.MethodGet, prsPath, nil, &list)
	require.Len(t, list, 2)
	byExercise := map[uuid.UUID]dto.PersonalRecordDTO{}
	for _, record := range list {
		byExercise[record.ExerciseID] = record
	}
	require.Equal(t, 116.67, byExercise[bench].WeightKg)
	require.True(t, byExercise[bench].IsCalculated)
	require.NotEmpty(t, byExercise[bench].ExerciseName)
	require.NotEqual(t, uuid.Nil, byExercise[bench].ID)
	require.Equal(t, 140.0, byExercise[squat].WeightKg)
	require.False(t, byExercise[squat].IsCalculated)

	// the history of the exercise, the newest first
	var history []dto.PersonalRecordDTO
	c.requireOK(http.MethodGet, prsPath+"/"+bench.String(), nil, &history)
	require.Len(t, history, 3)
	require.Equal(t, []float64{116.67, 100, 110}, []float64{history[0].WeightKg, history[1].WeightKg, history[2].WeightKg})
	require.Equal(t, []bool{true, false, false}, []bool{history[0].IsCalculated, history[1].IsCalculated, history[2].IsCalculated})
	require.True(t, history[0].CreatedAt.After(history[1].CreatedAt))
	require.True(t, history[1].CreatedAt.After(history[2].CreatedAt))

	history = nil
	c.requireOK(http.MethodGet, prsPath+"/"+squat.String(), nil, &history)
	require.Len(t, history, 1)

	// an exercise without records and an unknown exercise give an empty list
	for _, exerciseID := range []uuid.UUID{unused, uuid.New()} {
		history = nil
		c.requireOK(http.MethodGet, prsPath+"/"+exerciseID.String(), nil, &history)
		require.NotNil(t, history)
		require.Empty(t, history)
	}

	c.requireError(http.MethodGet, "/users/not-uuid/prs", nil, http.StatusBadRequest, "undefined_error")
	c.requireError(http.MethodGet, prsPath+"/not-uuid", nil, http.StatusBadRequest, "undefined_error")
}

func TestPersonalRecordsOfOtherUser(t *testing.T) {
	t.Parallel()
	exercise := basicExerciseIDs(t, 1)[0]
	owner, ownerID := newUserWithID(t)
	other, _ := newUserWithID(t)
	insertPersonalRecord(t, ownerID, exercise, 100, false, time.Now())

	prsPath := "/users/" + ownerID.String() + "/prs"
	other.requireError(http.MethodGet, prsPath, nil, http.StatusForbidden, "undefined_error")
	other.requireError(http.MethodGet, prsPath+"/"+exercise.String(), nil, http.StatusForbidden, "undefined_error")

	// even a missing user is not distinguished from another one
	other.requireError(http.MethodGet, "/users/"+uuid.NewString()+"/prs", nil, http.StatusForbidden, "undefined_error")

	owner.requireOK(http.MethodGet, prsPath, nil, nil)
}

func TestPersonalRecordsRequireAuth(t *testing.T) {
	t.Parallel()
	c := newClient(t)

	id := uuid.NewString()
	for _, path := range []string{
		"/users/" + id + "/prs",
		"/users/" + id + "/prs/" + id,
	} {
		c.requireError(http.MethodGet, path, nil, http.StatusUnauthorized, model.AuthErrorInvalidTokenKey)
	}
}
