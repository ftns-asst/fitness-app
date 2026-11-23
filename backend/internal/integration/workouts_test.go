//go:build integration

package integration

import (
	"context"
	"net/http"
	"sync"
	"testing"
	"time"

	"ftns-asst/internal/model"
	"ftns-asst/internal/rest/dto"

	"github.com/google/uuid"
	"github.com/stretchr/testify/require"
)

func TestWorkoutHTTP(t *testing.T) {
	owner, ownerID := newUserWithID(t)
	other := newUserClient(t)
	exerciseID := basicExerciseIDs(t, 1)[0]
	id := uuid.New()
	path := "/workouts/" + id.String()
	anon := newClient(t)
	anon.requireError(http.MethodPut, path, nil, http.StatusUnauthorized, model.AuthErrorInvalidTokenKey)
	anon.requireError(http.MethodGet, path, nil, http.StatusUnauthorized, model.AuthErrorInvalidTokenKey)
	anon.requireError(http.MethodGet, "/workouts", nil, http.StatusUnauthorized, model.AuthErrorInvalidTokenKey)
	owner.requireError(http.MethodPut, path, map[string]any{}, http.StatusBadRequest, model.DefaultErrorKey)
	owner.requireError(http.MethodGet, "/workouts/not-a-uuid", nil, http.StatusBadRequest, model.DefaultErrorKey)
	owner.requireError(http.MethodGet, "/workouts?program_id=invalid", nil, http.StatusBadRequest, model.DefaultErrorKey)
	owner.requireError(http.MethodGet, "/workouts?limit=-1", nil, http.StatusBadRequest, model.DefaultErrorKey)

	var plan uuid.UUID
	err := db.QueryRow(context.Background(), `INSERT INTO user_programs (user_id,name,source_kind,goal,level)
		VALUES ($1,'test','custom','strength','beginner') RETURNING id`, ownerID).Scan(&plan)
	require.NoError(t, err)
	start := time.Now().UTC().Truncate(time.Second)
	body := dto.SaveWorkoutRequestBody{
		ProgramID: &plan, WeekNumber: ptr(2), DayOfWeek: ptr(3),
		StartedAt: start, CompletedAt: start.Add(time.Hour),
		Exercises: []dto.WorkoutExerciseInput{{
			WorkoutExerciseTargets: dto.WorkoutExerciseTargets{ExerciseID: exerciseID, TargetSets: ptr(2), TargetReps: ptr(8), TargetWeightKg: ptr(50.0)},
			Sets:                   []dto.WorkoutSetInput{{Reps: 8}, {Reps: 5, WeightKg: ptr(60.0)}},
		}},
	}
	var saved, replay, detail dto.WorkoutDTO
	status, failure := owner.do(http.MethodPut, path, body, &saved)
	require.Equal(t, http.StatusCreated, status, failure)
	require.Equal(t, id, saved.ID)
	require.Equal(t, &plan, saved.ProgramID)
	require.True(t, start.Equal(saved.StartedAt))
	require.Len(t, saved.Exercises, 1)
	ex := saved.Exercises[0]
	require.Equal(t, body.Exercises[0].WorkoutExerciseTargets, ex.WorkoutExerciseTargets)
	require.NotEmpty(t, ex.ExerciseName)
	require.Len(t, ex.Sets, 2)
	require.Nil(t, ex.Sets[0].WeightKg)
	require.Equal(t, body.Exercises[0].Sets[1], ex.Sets[1].WorkoutSetInput)
	require.Equal(t, 2, ex.Sets[1].SetNumber)
	owner.requireOK(http.MethodPut, path, body, &replay)
	owner.requireOK(http.MethodGet, path, nil, &detail)
	require.Equal(t, saved, replay)
	require.Equal(t, saved, detail)
	body.Exercises[0].Sets[0].Reps++
	owner.requireError(http.MethodPut, path, body, http.StatusConflict, model.DefaultErrorKey)
	other.requireError(http.MethodGet, path, nil, http.StatusNotFound, model.DefaultErrorKey)

	var summaries []map[string]any
	owner.requireOK(http.MethodGet, "/workouts?limit=1&offset=0&program_id="+plan.String(), nil, &summaries)
	require.Len(t, summaries, 1)
	require.Equal(t, id.String(), summaries[0]["id"])
	require.NotContains(t, summaries[0], "exercises")
	for _, query := range []string{"?offset=1", "?program_id=" + uuid.NewString()} {
		var empty []dto.WorkoutSummaryDTO
		owner.requireOK(http.MethodGet, "/workouts"+query, nil, &empty)
		require.Equal(t, []dto.WorkoutSummaryDTO{}, empty)
	}
	var records []dto.PersonalRecordDTO
	owner.requireOK(http.MethodGet, "/users/"+ownerID.String()+"/prs", nil, &records)
	require.Len(t, records, 1)
	require.Equal(t, &ex.Sets[1].ID, records[0].SourceWorkoutSetID)
}

func TestWorkoutHistory(t *testing.T) {
	ctx := context.Background()
	_, owner := newUserWithID(t)
	_, other := newUserWithID(t)
	exercise := basicExerciseIDs(t, 1)[0]
	service := services.Workout()
	plan := uuid.New()
	_, err := db.Exec(ctx, `INSERT INTO user_programs (id,user_id,name,source_kind,goal,level,days_of_week)
		VALUES ($1,$2,'test','custom','strength','beginner','{3}')`, plan, owner)
	require.NoError(t, err)
	start := time.Now().Add(-time.Hour)
	input := model.SaveWorkoutInput{
		ProgramID: &plan, WeekNumber: ptr(1), DayOfWeek: ptr(3), StartedAt: start, CompletedAt: start.Add(time.Hour),
		Exercises: []model.WorkoutExerciseInput{
			{WorkoutExerciseTargets: model.WorkoutExerciseTargets{ExerciseID: exercise, TargetSets: ptr(3), TargetReps: ptr(6), TargetWeightKg: ptr(55.25)},
				Sets: []model.WorkoutSetInput{{Reps: 6, WeightKg: ptr(50.25)}, {Reps: 5, WeightKg: ptr(60.0)}}},
			{WorkoutExerciseTargets: model.WorkoutExerciseTargets{ExerciseID: exercise}, Sets: []model.WorkoutSetInput{{Reps: 8}}},
		},
	}
	id := uuid.New()
	type response struct {
		workout *model.Workout
		created bool
		err     error
	}
	var results [2]response
	var wg sync.WaitGroup
	for i := range results {
		wg.Go(func() {
			results[i].workout, results[i].created, results[i].err = service.SaveWorkout(ctx, owner, id, &input)
		})
	}
	wg.Wait()
	for _, r := range results {
		require.NoError(t, r.err)
	}
	require.NotEqual(t, results[0].created, results[1].created)
	require.Equal(t, results[0].workout, results[1].workout)
	workout, err := service.GetWorkout(ctx, owner, id)
	require.NoError(t, err)
	require.Equal(t, results[0].workout, workout)
	require.Len(t, workout.Exercises, 2)
	require.Len(t, workout.Exercises[0].Sets, 2)
	require.Equal(t, 2, workout.Exercises[0].Sets[1].SetNumber)
	require.Equal(t, 55.25, *workout.Exercises[0].TargetWeightKg)
	require.Equal(t, 50.25, *workout.Exercises[0].Sets[0].WeightKg)
	require.Nil(t, workout.Exercises[1].Sets[0].WeightKg)

	_, err = service.GetWorkout(ctx, other, id)
	require.ErrorIs(t, err, model.ErrNotFound)
	_, _, err = service.SaveWorkout(ctx, other, uuid.New(), &input)
	require.ErrorIs(t, err, model.ErrNotFound)
	input.Exercises[0].Sets[0].Reps++
	_, _, err = service.SaveWorkout(ctx, owner, id, &input)
	require.ErrorIs(t, err, model.ErrConflict)
	input.Exercises[0].Sets[0].Reps--

	prs := services.PersonalRecord()
	history, err := prs.GetRecordHistory(ctx, owner, owner, exercise)
	require.NoError(t, err)
	require.Len(t, history, 1)
	setID := workout.Exercises[0].Sets[1].ID
	record := history[0]
	require.Equal(t, exercise, record.ExerciseID)
	require.Equal(t, &setID, record.SourceWorkoutSetID)
	require.True(t, record.IsCalculated)
	require.Equal(t, 70.0, record.WeightKg)

	free := model.SaveWorkoutInput{StartedAt: start.Add(-time.Hour), CompletedAt: start, Exercises: input.Exercises[1:]}
	freeID := uuid.New()
	_, created, err := service.SaveWorkout(ctx, owner, freeID, &free)
	require.NoError(t, err)
	require.True(t, created)
	list, err := service.ListWorkouts(ctx, owner, nil, model.PageInfo{Limit: ptr(1), Offset: ptr(1)})
	require.NoError(t, err)
	require.Len(t, list, 1)
	require.Equal(t, freeID, list[0].ID)
	list, err = service.ListWorkouts(ctx, owner, &plan, model.PageInfo{})
	require.NoError(t, err)
	require.Len(t, list, 1)
	require.Equal(t, id, list[0].ID)
	list, err = service.ListWorkouts(ctx, other, nil, model.PageInfo{})
	require.NoError(t, err)
	require.Empty(t, list)

	badID := uuid.New()
	free.Exercises = append(free.Exercises, model.WorkoutExerciseInput{
		WorkoutExerciseTargets: model.WorkoutExerciseTargets{ExerciseID: uuid.New()}, Sets: []model.WorkoutSetInput{{Reps: 1}},
	})
	_, _, err = service.SaveWorkout(ctx, owner, badID, &free)
	require.ErrorIs(t, err, model.ErrBadRequest)
	_, err = service.GetWorkout(ctx, owner, badID)
	require.ErrorIs(t, err, model.ErrNotFound)

	_, err = db.Exec(ctx, "DELETE FROM user_programs WHERE id=$1", plan)
	require.NoError(t, err)
	replay, created, err := service.SaveWorkout(ctx, owner, id, &input)
	require.NoError(t, err)
	require.False(t, created)
	require.Nil(t, replay.ProgramID)
	require.Equal(t, workout.Exercises, replay.Exercises)
}

func TestConcurrentWorkoutRecords(t *testing.T) {
	ctx := context.Background()
	_, userID := newUserWithID(t)
	ids := basicExerciseIDs(t, 3)
	prs := services.PersonalRecord()
	_, err := prs.CreateRecord(ctx, userID, userID, &model.CreatePersonalRecordInput{ExerciseID: ids[0], WeightKg: 90})
	require.NoError(t, err)
	exercise := func(id uuid.UUID, weight float64, reps int) model.WorkoutExerciseInput {
		return model.WorkoutExerciseInput{WorkoutExerciseTargets: model.WorkoutExerciseTargets{ExerciseID: id},
			Sets: []model.WorkoutSetInput{{WeightKg: &weight, Reps: reps}}}
	}
	now := time.Now()
	input := model.SaveWorkoutInput{StartedAt: now.Add(-time.Hour), CompletedAt: now, Exercises: []model.WorkoutExerciseInput{
		exercise(ids[0], 80, 1), exercise(ids[1], 60, 5), exercise(ids[1], 70, 1), exercise(ids[2], 0, 10),
	}}
	var wg sync.WaitGroup
	var errs [2]error
	for i := range errs {
		wg.Go(func() {
			_, _, errs[i] = services.Workout().SaveWorkout(ctx, userID, uuid.New(), &input)
		})
	}
	wg.Wait()
	for _, err := range errs {
		require.NoError(t, err)
	}
	latest, err := prs.GetLatestRecords(ctx, userID, userID)
	require.NoError(t, err)
	require.Len(t, latest, 2)
	history, err := prs.GetRecordHistory(ctx, userID, userID, ids[0])
	require.NoError(t, err)
	require.Len(t, history, 1)
	require.Equal(t, 90.0, history[0].WeightKg)
	require.Nil(t, history[0].SourceWorkoutSetID)
	history, err = prs.GetRecordHistory(ctx, userID, userID, ids[1])
	require.NoError(t, err)
	require.Len(t, history, 1)
	require.Equal(t, 70.0, history[0].WeightKg)
	require.False(t, history[0].IsCalculated)
	require.NotNil(t, history[0].SourceWorkoutSetID)

	input.Exercises = []model.WorkoutExerciseInput{exercise(ids[0], 95, 1)}
	w, _, err := services.Workout().SaveWorkout(ctx, userID, uuid.New(), &input)
	require.NoError(t, err)
	history, err = prs.GetRecordHistory(ctx, userID, userID, ids[0])
	require.NoError(t, err)
	require.Len(t, history, 2)
	require.Equal(t, 95.0, history[0].WeightKg)
	require.Equal(t, &w.Exercises[0].Sets[0].ID, history[0].SourceWorkoutSetID)
}
