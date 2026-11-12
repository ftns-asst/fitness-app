//go:build integration

package integration

import (
	"ftns-asst/internal/model"
	"ftns-asst/internal/rest/dto"
	"net/http"
	"testing"

	"github.com/google/uuid"
	"github.com/stretchr/testify/require"
)

func TestCreatePersonalRecordsBulk(t *testing.T) {
	t.Parallel()
	exercises := basicExerciseIDs(t, 3)
	c, userID := newUserWithID(t)
	bulkPath := "/users/" + userID.String() + "/prs/bulk"

	// the result has the order of the request, each record is built as in the single request
	var created []dto.PersonalRecordDTO
	c.requireOK(http.MethodPost, bulkPath, dto.CreatePersonalRecordsRequestBody{Records: []dto.CreatePersonalRecordRequestBody{
		{ExerciseID: exercises[0], WeightKg: 100},
		{ExerciseID: exercises[1], WeightKg: 60, Reps: ptr(10)},
		{ExerciseID: exercises[2], WeightKg: 80, Reps: ptr(1)},
	}}, &created)
	require.Len(t, created, 3)
	require.Equal(t, []uuid.UUID{exercises[0], exercises[1], exercises[2]}, []uuid.UUID{created[0].ExerciseID, created[1].ExerciseID, created[2].ExerciseID})
	require.Equal(t, []float64{100, 80, 80}, []float64{created[0].WeightKg, created[1].WeightKg, created[2].WeightKg})
	require.Equal(t, []bool{false, true, false}, []bool{created[0].IsCalculated, created[1].IsCalculated, created[2].IsCalculated})
	for _, record := range created {
		require.NotEqual(t, uuid.Nil, record.ID)
		require.NotEmpty(t, record.ExerciseName)
	}

	var latest []dto.PersonalRecordDTO
	c.requireOK(http.MethodGet, "/users/"+userID.String()+"/prs", nil, &latest)
	require.Len(t, latest, 3)

	// one record is allowed too
	c.requireOK(http.MethodPost, bulkPath, dto.CreatePersonalRecordsRequestBody{Records: []dto.CreatePersonalRecordRequestBody{
		{ExerciseID: exercises[0], WeightKg: 105},
	}}, &created)
	require.Len(t, created, 1)
}

func TestCreatePersonalRecordsBulkMakesProgramReady(t *testing.T) {
	t.Parallel()
	exercises := basicExerciseIDs(t, 2)
	c, userID := newUserWithID(t)

	var program dto.ProgramDTO
	c.requireOK(http.MethodPost, "/programs", dto.CreateProgramRequestBody{Name: "p", Goal: "strength", Level: "beginner", DaysOfWeek: []int{1}}, &program)
	path := "/programs/" + program.ID.String()
	c.requireOK(http.MethodPut, path+"/weeks/1", dto.ReplaceProgramWeekRequestBody{Days: []dto.ProgramDayInput{
		{DayOfWeek: 1, Exercises: []dto.ProgramDayExerciseInput{percentExercise(exercises[0], 80), percentExercise(exercises[1], 70)}},
	}}, &program)
	require.Equal(t, "needs_measurement", program.State)
	require.ElementsMatch(t, exercises, measurementIDs(program))

	// the states are refreshed once after all records of the request
	c.requireOK(http.MethodPost, "/users/"+userID.String()+"/prs/bulk", dto.CreatePersonalRecordsRequestBody{Records: []dto.CreatePersonalRecordRequestBody{
		{ExerciseID: exercises[0], WeightKg: 100},
		{ExerciseID: exercises[1], WeightKg: 60, Reps: ptr(8)},
	}}, nil)

	program = dto.ProgramDTO{}
	c.requireOK(http.MethodGet, path, nil, &program)
	require.Equal(t, "draft", program.State)
	require.Empty(t, program.RequiredMeasurements)
	c.requireOK(http.MethodPost, path+"/start", nil, nil)
}

func TestCreatePersonalRecordsBulkIsAtomic(t *testing.T) {
	t.Parallel()
	exercises := basicExerciseIDs(t, 2)
	c, userID := newUserWithID(t)
	bulkPath := "/users/" + userID.String() + "/prs/bulk"
	record := func(exerciseID uuid.UUID, weight float64) dto.CreatePersonalRecordRequestBody {
		return dto.CreatePersonalRecordRequestBody{ExerciseID: exerciseID, WeightKg: weight}
	}

	// nothing is saved if any record is not valid or the exercise is not available
	c.requireError(http.MethodPost, bulkPath, dto.CreatePersonalRecordsRequestBody{Records: []dto.CreatePersonalRecordRequestBody{
		record(exercises[0], 100), record(uuid.New(), 50),
	}}, http.StatusBadRequest, model.ProgramExerciseNotAvailableKey)
	c.requireError(http.MethodPost, bulkPath, dto.CreatePersonalRecordsRequestBody{Records: []dto.CreatePersonalRecordRequestBody{
		record(exercises[0], 100), record(exercises[1], 0),
	}}, http.StatusBadRequest, "undefined_error")
	c.requireError(http.MethodPost, bulkPath, dto.CreatePersonalRecordsRequestBody{Records: []dto.CreatePersonalRecordRequestBody{
		record(exercises[0], 100), {ExerciseID: exercises[1], WeightKg: 50, Reps: ptr(0)},
	}}, http.StatusBadRequest, "undefined_error")

	other, otherID := newUserWithID(t)
	private := createExercise(other, dto.CreateExerciseRequestBody{Name: newPrefix() + "private"})
	c.requireError(http.MethodPost, bulkPath, dto.CreatePersonalRecordsRequestBody{Records: []dto.CreatePersonalRecordRequestBody{
		record(exercises[0], 100), record(private.ID, 50),
	}}, http.StatusBadRequest, model.ProgramExerciseNotAvailableKey)
	other.requireOK(http.MethodPost, "/users/"+otherID.String()+"/prs/bulk", dto.CreatePersonalRecordsRequestBody{Records: []dto.CreatePersonalRecordRequestBody{
		record(private.ID, 50), record(exercises[0], 70),
	}}, nil)

	// one exercise can be only once in the request
	c.requireError(http.MethodPost, bulkPath, dto.CreatePersonalRecordsRequestBody{Records: []dto.CreatePersonalRecordRequestBody{
		record(exercises[0], 100), record(exercises[0], 110),
	}}, http.StatusBadRequest, "undefined_error")

	var latest []dto.PersonalRecordDTO
	c.requireOK(http.MethodGet, "/users/"+userID.String()+"/prs", nil, &latest)
	require.Empty(t, latest)

	// the number of records is limited
	tooMany := make([]dto.CreatePersonalRecordRequestBody, 0, model.MaxPersonalRecordsInBulk+1)
	for i := 0; i <= model.MaxPersonalRecordsInBulk; i++ {
		tooMany = append(tooMany, record(uuid.New(), 50))
	}
	c.requireError(http.MethodPost, bulkPath, dto.CreatePersonalRecordsRequestBody{Records: tooMany}, http.StatusBadRequest, "undefined_error")
	c.requireError(http.MethodPost, bulkPath, dto.CreatePersonalRecordsRequestBody{Records: []dto.CreatePersonalRecordRequestBody{}}, http.StatusBadRequest, "undefined_error")
	c.requireError(http.MethodPost, bulkPath, map[string]any{}, http.StatusBadRequest, "undefined_error")
	c.requireError(http.MethodPost, "/users/not-uuid/prs/bulk", dto.CreatePersonalRecordsRequestBody{Records: []dto.CreatePersonalRecordRequestBody{record(exercises[0], 1)}}, http.StatusBadRequest, "undefined_error")
}

func TestCreatePersonalRecordsBulkAccess(t *testing.T) {
	t.Parallel()
	exercise := basicExerciseIDs(t, 1)[0]
	owner, ownerID := newUserWithID(t)
	other, _ := newUserWithID(t)
	body := dto.CreatePersonalRecordsRequestBody{Records: []dto.CreatePersonalRecordRequestBody{{ExerciseID: exercise, WeightKg: 100}}}
	bulkPath := "/users/" + ownerID.String() + "/prs/bulk"

	other.requireError(http.MethodPost, bulkPath, body, http.StatusForbidden, "undefined_error")
	newClient(t).requireError(http.MethodPost, bulkPath, body, http.StatusUnauthorized, model.AuthErrorInvalidTokenKey)

	var latest []dto.PersonalRecordDTO
	owner.requireOK(http.MethodGet, "/users/"+ownerID.String()+"/prs", nil, &latest)
	require.Empty(t, latest)
}
