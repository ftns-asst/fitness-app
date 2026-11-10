//go:build integration

package integration

import (
	"context"
	"crypto/rand"
	"encoding/hex"
	"ftns-asst/internal/model"
	"ftns-asst/internal/rest/dto"
	"net/http"
	"net/url"
	"slices"
	"strings"
	"testing"

	"github.com/google/uuid"
	"github.com/stretchr/testify/require"
	excelize "github.com/xuri/excelize/v2"
)

// relative to the package directory, where go test runs
var exerciseCatalogPath = getEnv("TEST_EXERCISE_CATALOG_PATH", "../../../data/exercise_catalog.xlsx")

type catalogItem struct {
	Key  string
	Name string
}

type catalogExercise struct {
	ID           uuid.UUID
	Name         string
	Equipment    []string
	MuscleGroups []string
}

type catalog struct {
	muscleGroups []catalogItem
	equipment    []catalogItem
	exercises    []catalogExercise
}

// reads the catalog file independently of the service loader
func readCatalog(t *testing.T) *catalog {
	t.Helper()
	f, err := excelize.OpenFile(exerciseCatalogPath)
	require.NoError(t, err)
	defer func() { _ = f.Close() }()

	rows := func(sheet string) [][]string {
		res, err := f.GetRows(sheet)
		require.NoError(t, err)
		require.NotEmpty(t, res, sheet)
		return res[1:]
	}
	split := func(s string) []string {
		parts := strings.Split(s, ",")
		for i := range parts {
			parts[i] = strings.TrimSpace(parts[i])
		}
		return parts
	}

	res := &catalog{}
	for _, row := range rows("muscle_groups") {
		res.muscleGroups = append(res.muscleGroups, catalogItem{Key: row[0], Name: row[1]})
	}
	for _, row := range rows("equipment") {
		res.equipment = append(res.equipment, catalogItem{Key: row[0], Name: row[1]})
	}
	for _, row := range rows("exercises") {
		res.exercises = append(res.exercises, catalogExercise{
			ID:           uuid.MustParse(row[0]),
			Name:         row[1],
			Equipment:    split(row[2]),
			MuscleGroups: split(row[3]),
		})
	}
	return res
}

func listExercises(c *client, query url.Values) []dto.ExerciseDTO {
	c.t.Helper()
	if !query.Has("limit") {
		query.Set("limit", "1000")
	}
	if !query.Has("offset") {
		query.Set("offset", "0")
	}
	var res []dto.ExerciseDTO
	c.requireOK(http.MethodGet, "/exercises?"+query.Encode(), nil, &res)
	return res
}

func exerciseNames(list []dto.ExerciseDTO) []string {
	res := make([]string, 0, len(list))
	for _, e := range list {
		res = append(res, e.Name)
	}
	return res
}

// unique name prefix, used with search to isolate exercises of one test
func newPrefix() string {
	b := make([]byte, 4)
	_, _ = rand.Read(b)
	return "it-" + hex.EncodeToString(b) + " "
}

func createExercise(c *client, body dto.CreateExerciseRequestBody) dto.ExerciseDTO {
	c.t.Helper()
	var res dto.ExerciseDTO
	c.requireOK(http.MethodPost, "/exercises", body, &res)
	return res
}

func TestCatalogDictionaries(t *testing.T) {
	t.Parallel()
	expected := readCatalog(t)
	c := newClient(t)

	var muscleGroups []dto.MuscleGroup
	c.requireOK(http.MethodGet, "/muscle-groups", nil, &muscleGroups)
	require.Len(t, muscleGroups, len(expected.muscleGroups))
	for _, mg := range expected.muscleGroups {
		require.Contains(t, muscleGroups, dto.MuscleGroup{Key: mg.Key, Name: mg.Name})
	}

	var equipment []dto.Equipment
	c.requireOK(http.MethodGet, "/equipments", nil, &equipment)
	require.Len(t, equipment, len(expected.equipment))
	for _, eq := range expected.equipment {
		require.Contains(t, equipment, dto.Equipment{Key: eq.Key, Name: eq.Name})
	}
}

func TestBasicExercisesFromCatalog(t *testing.T) {
	t.Parallel()
	expected := readCatalog(t)

	list := listExercises(newClient(t), url.Values{"type": {"basic"}})
	require.Len(t, list, len(expected.exercises))
	require.True(t, slices.IsSortedFunc(list, func(a, b dto.ExerciseDTO) int { return strings.Compare(a.Name, b.Name) }),
		"list must be sorted by name: %q", exerciseNames(list))

	byID := map[uuid.UUID]dto.ExerciseDTO{}
	for _, e := range list {
		byID[e.ID] = e
	}
	for _, e := range expected.exercises {
		got, ok := byID[e.ID]
		require.True(t, ok, "basic exercise %s %q not found", e.ID, e.Name)
		require.Equal(t, e.Name, got.Name)
		require.Equal(t, e.MuscleGroups, got.MuscleGroups)
		require.Equal(t, e.Equipment, got.Equipment)
		require.True(t, got.IsPublic)
		require.Nil(t, got.OwnerID)
	}
}

func TestBasicExercisesFilters(t *testing.T) {
	t.Parallel()
	expected := readCatalog(t)
	c := newClient(t)

	// any of muscle groups
	var withCalvesOrTriceps []string
	for _, e := range expected.exercises {
		if slices.Contains(e.MuscleGroups, "calves") || slices.Contains(e.MuscleGroups, "triceps") {
			withCalvesOrTriceps = append(withCalvesOrTriceps, e.Name)
		}
	}
	require.NotEmpty(t, withCalvesOrTriceps)
	list := listExercises(c, url.Values{"type": {"basic"}, "muscle_group": {"calves", "triceps"}})
	require.ElementsMatch(t, withCalvesOrTriceps, exerciseNames(list))

	// all equipment of exercise must be available
	available := []string{"bodyweight", "mat", "dumbbell"}
	var doable []string
	for _, e := range expected.exercises {
		if !slices.ContainsFunc(e.Equipment, func(eq string) bool { return !slices.Contains(available, eq) }) {
			doable = append(doable, e.Name)
		}
	}
	require.NotEmpty(t, doable)
	list = listExercises(c, url.Values{"type": {"basic"}, "equipment": available})
	require.ElementsMatch(t, doable, exerciseNames(list))

	// search is case-insensitive, like wildcards are escaped
	first := expected.exercises[0]
	list = listExercises(c, url.Values{"type": {"basic"}, "search": {strings.ToUpper(first.Name)}})
	require.Contains(t, exerciseNames(list), first.Name)
	require.Empty(t, listExercises(c, url.Values{"type": {"basic"}, "search": {"%"}}))
	require.Empty(t, listExercises(c, url.Values{"type": {"basic"}, "search": {"_"}}))

	// pagination keeps order by name
	all := listExercises(c, url.Values{"type": {"basic"}})
	page := listExercises(c, url.Values{"type": {"basic"}, "limit": {"2"}, "offset": {"3"}})
	require.Equal(t, all[3:5], page)
}

// not parallel: changes basic exercises, runs before parallel tests continue
func TestFillExercisesUpsert(t *testing.T) {
	ctx := context.Background()
	repo := repos.Exercise()
	expected := readCatalog(t)
	basicCount := func() int {
		var n int
		require.NoError(t, db.QueryRow(ctx,
			`SELECT count(*) FROM exercises WHERE owner_id IS NULL AND deleted_at IS NULL`).Scan(&n))
		return n
	}
	require.Equal(t, len(expected.exercises), basicCount())

	// restart keeps the same rows
	var createdBefore string
	require.NoError(t, db.QueryRow(ctx, `SELECT created_at::text FROM exercises WHERE id = $1`,
		expected.exercises[0].ID).Scan(&createdBefore))
	require.NoError(t, services.Exercise().Start(ctx))
	require.Equal(t, len(expected.exercises), basicCount())
	var createdAfter string
	require.NoError(t, db.QueryRow(ctx, `SELECT created_at::text FROM exercises WHERE id = $1`,
		expected.exercises[0].ID).Scan(&createdAfter))
	require.Equal(t, createdBefore, createdAfter)

	// user exercise with an id from the seed is not overwritten
	c := newClient(t)
	signed := signUp(t, c, newEmail())
	userID := signed.User.ID
	userExerciseID := uuid.New()
	_, err := db.Exec(ctx, `INSERT INTO exercises (id, owner_id, name) VALUES ($1, $2, 'user exercise')`,
		userExerciseID, userID)
	require.NoError(t, err)

	newID := uuid.New()
	seed := []*model.Exercise{
		{
			ID:           newID,
			IsPublic:     true,
			Name:         "it upsert",
			MuscleGroups: []model.MuscleGroupKey{model.MuscleGroupAbs},
			Equipments:   []model.EquipmentKey{model.EquipmentMat},
		},
		{
			ID:           userExerciseID,
			IsPublic:     true,
			Name:         "seed overwrite",
			MuscleGroups: []model.MuscleGroupKey{model.MuscleGroupChest},
			Equipments:   []model.EquipmentKey{model.EquipmentBarbell},
		},
	}
	t.Cleanup(func() {
		_, err := db.Exec(ctx, `DELETE FROM exercises WHERE id = ANY($1)`, []uuid.UUID{newID, userExerciseID})
		require.NoError(t, err)
	})
	require.NoError(t, repo.FillExercises(ctx, seed))

	type row struct {
		Name         string
		OwnerID      *uuid.UUID
		IsPublic     bool
		MuscleGroups []string
		Deleted      bool
	}
	getRow := func(id uuid.UUID) row {
		var r row
		require.NoError(t, db.QueryRow(ctx,
			`SELECT name, owner_id, is_public, muscle_group, deleted_at IS NOT NULL FROM exercises WHERE id = $1`, id).
			Scan(&r.Name, &r.OwnerID, &r.IsPublic, &r.MuscleGroups, &r.Deleted))
		return r
	}
	require.Equal(t, row{Name: "it upsert", IsPublic: true, MuscleGroups: []string{"abs"}}, getRow(newID))
	require.Equal(t, row{Name: "user exercise", OwnerID: &userID, MuscleGroups: []string{}}, getRow(userExerciseID))

	// second fill updates fields and restores deleted basic exercise
	_, err = db.Exec(ctx, `UPDATE exercises SET deleted_at = NOW() WHERE id = $1`, newID)
	require.NoError(t, err)
	seed[0].Name = "it upsert renamed"
	seed[0].MuscleGroups = []model.MuscleGroupKey{model.MuscleGroupAbs, model.MuscleGroupOblique}
	require.NoError(t, repo.FillExercises(ctx, seed[:1]))
	require.Equal(t, row{Name: "it upsert renamed", IsPublic: true, MuscleGroups: []string{"abs", "obliques"}}, getRow(newID))
	require.Equal(t, len(expected.exercises)+1, basicCount())

	// duplicate ids in one fill are rejected, nothing is changed
	dup := []*model.Exercise{
		{ID: newID, IsPublic: true, Name: "dup 1", MuscleGroups: []model.MuscleGroupKey{}, Equipments: []model.EquipmentKey{}},
		{ID: newID, IsPublic: true, Name: "dup 2", MuscleGroups: []model.MuscleGroupKey{}, Equipments: []model.EquipmentKey{}},
	}
	require.Error(t, repo.FillExercises(ctx, dup))
	require.Equal(t, "it upsert renamed", getRow(newID).Name)
}

func TestExerciseCRUD(t *testing.T) {
	t.Parallel()
	owner := newClient(t)
	signed := signUp(t, owner, newEmail())
	owner.token = signed.Tokens.AccessToken
	ownerID := signed.User.ID
	other := newClient(t)
	other.token = signUp(t, other, newEmail()).Tokens.AccessToken
	prefix := newPrefix()

	created := createExercise(owner, dto.CreateExerciseRequestBody{
		Name:         prefix + "private",
		Description:  new("desc"),
		MuscleGroups: []string{"chest"},
		Equipment:    []string{"barbell", "bench"},
	})
	require.NotEqual(t, uuid.Nil, created.ID)
	require.Equal(t, &ownerID, created.OwnerID)
	require.False(t, created.IsPublic)
	require.Equal(t, []string{"chest"}, created.MuscleGroups)
	require.Equal(t, []string{"barbell", "bench"}, created.Equipment)
	require.False(t, created.CreatedAt.IsZero())
	path := "/exercises/" + created.ID.String()

	var got dto.ExerciseDTO
	owner.requireOK(http.MethodGet, path, nil, &got)
	require.Equal(t, created, got)

	// private exercise is hidden from other users
	other.requireError(http.MethodGet, path, nil, http.StatusForbidden, model.DefaultErrorKey)
	other.requireError(http.MethodPatch, path, dto.UpdateExerciseRequestBody{ID: created.ID}, http.StatusForbidden, model.DefaultErrorKey)
	other.requireError(http.MethodDelete, path, nil, http.StatusForbidden, model.DefaultErrorKey)

	// partial update keeps other fields
	var updated dto.ExerciseDTO
	owner.requireOK(http.MethodPatch, path, dto.UpdateExerciseRequestBody{
		ID: created.ID,
		ExerciseInfoDTO: dto.ExerciseInfoDTO{
			Name:     new(prefix + "public"),
			IsPublic: new(true),
		},
	}, &updated)
	expected := created
	expected.Name = prefix + "public"
	expected.IsPublic = true
	require.Equal(t, expected, updated)

	// public exercise is visible to other users
	other.requireOK(http.MethodGet, path, nil, &got)
	require.Equal(t, updated, got)

	// arrays are replaced
	owner.requireOK(http.MethodPatch, path, dto.UpdateExerciseRequestBody{
		ID:              created.ID,
		ExerciseInfoDTO: dto.ExerciseInfoDTO{MuscleGroups: []string{"abs", "obliques"}},
	}, &updated)
	require.Equal(t, []string{"abs", "obliques"}, updated.MuscleGroups)
	require.Equal(t, []string{"barbell", "bench"}, updated.Equipment)

	owner.requireOK(http.MethodDelete, path, nil, nil)
	owner.requireError(http.MethodDelete, path, nil, http.StatusNotFound, model.DefaultErrorKey)
	require.Empty(t, listExercises(owner, url.Values{"type": {"own"}, "search": {prefix}}))
}

//func TestDeletedExerciseIsNotAvailable(t *testing.T) {
//	t.Parallel()
//	c := newClient(t)
//	c.token = signUp(t, c, newEmail()).Tokens.AccessToken
//	created := createExercise(c, dto.CreateExerciseRequestBody{Name: newPrefix() + "deleted"})
//	path := "/exercises/" + created.ID.String()
//	c.requireOK(http.MethodDelete, path, nil, nil)
//
//	c.requireError(http.MethodGet, path, nil, http.StatusNotFound, model.DefaultErrorKey)
//	c.requireError(http.MethodPatch, path, dto.UpdateExerciseRequestBody{
//		ID:              created.ID,
//		ExerciseInfoDTO: dto.ExerciseInfoDTO{Name: new("restored")},
//	}, http.StatusNotFound, model.DefaultErrorKey)
//}

func TestExerciseValidation(t *testing.T) {
	t.Parallel()
	c := newClient(t)
	c.token = signUp(t, c, newEmail()).Tokens.AccessToken

	c.requireError(http.MethodPost, "/exercises", dto.CreateExerciseRequestBody{Name: "  "},
		http.StatusBadRequest, model.DefaultErrorKey)
	c.requireError(http.MethodPost, "/exercises", dto.CreateExerciseRequestBody{Name: "x", MuscleGroups: []string{"unknown"}},
		http.StatusBadRequest, model.DefaultErrorKey)
	// removed from the catalog
	c.requireError(http.MethodPost, "/exercises", dto.CreateExerciseRequestBody{Name: "x", Equipment: []string{"treadmill"}},
		http.StatusBadRequest, model.DefaultErrorKey)

	created := createExercise(c, dto.CreateExerciseRequestBody{Name: newPrefix() + "valid"})
	c.requireError(http.MethodPatch, "/exercises/"+created.ID.String(), dto.UpdateExerciseRequestBody{
		ID:              created.ID,
		ExerciseInfoDTO: dto.ExerciseInfoDTO{Equipment: []string{"unknown"}},
	}, http.StatusBadRequest, model.DefaultErrorKey)

	c.requireError(http.MethodGet, "/exercises/"+uuid.NewString(), nil, http.StatusNotFound, model.DefaultErrorKey)
	c.requireError(http.MethodPatch, "/exercises/"+uuid.NewString(), dto.UpdateExerciseRequestBody{},
		http.StatusBadRequest, model.DefaultErrorKey)
	id := uuid.NewString()
	c.requireError(http.MethodPatch, "/exercises/"+id, dto.UpdateExerciseRequestBody{ID: uuid.MustParse(id)},
		http.StatusNotFound, model.DefaultErrorKey)
	c.requireError(http.MethodDelete, "/exercises/"+id, nil, http.StatusNotFound, model.DefaultErrorKey)
}

// empty arrays must be returned as [], not null
func TestCreateExerciseWithoutArrays(t *testing.T) {
	t.Parallel()
	c := newClient(t)
	c.token = signUp(t, c, newEmail()).Tokens.AccessToken

	created := createExercise(c, dto.CreateExerciseRequestBody{Name: newPrefix() + "no arrays"})
	require.Equal(t, []string{}, created.MuscleGroups)
	require.Equal(t, []string{}, created.Equipment)

	var got dto.ExerciseDTO
	c.requireOK(http.MethodGet, "/exercises/"+created.ID.String(), nil, &got)
	require.Equal(t, []string{}, got.MuscleGroups)
	require.Equal(t, []string{}, got.Equipment)
}

func TestListExerciseTypes(t *testing.T) {
	t.Parallel()
	prefix := newPrefix()
	author := newClient(t)
	author.token = signUp(t, author, newEmail()).Tokens.AccessToken
	viewer := newClient(t)
	viewer.token = signUp(t, viewer, newEmail()).Tokens.AccessToken
	anonymous := newClient(t)

	createExercise(author, dto.CreateExerciseRequestBody{
		Name: prefix + "a public chest", IsPublic: true,
		MuscleGroups: []string{"chest"}, Equipment: []string{"barbell", "bench"},
	})
	createExercise(author, dto.CreateExerciseRequestBody{
		Name: prefix + "b public abs", IsPublic: true,
		MuscleGroups: []string{"abs"}, Equipment: []string{"mat"},
	})
	createExercise(author, dto.CreateExerciseRequestBody{
		Name:         prefix + "c private chest",
		MuscleGroups: []string{"chest"}, Equipment: []string{"dumbbell"},
	})
	public := []string{prefix + "a public chest", prefix + "b public abs"}
	all := append(slices.Clone(public), prefix+"c private chest")

	search := func(listType string, extra url.Values) url.Values {
		q := url.Values{"type": {listType}, "search": {prefix}}
		for k, v := range extra {
			q[k] = v
		}
		return q
	}

	tests := []struct {
		Name     string
		Client   *client
		Query    url.Values
		Expected []string
	}{
		{Name: "own", Client: author, Query: search("own", nil), Expected: all},
		{Name: "own of other user", Client: viewer, Query: search("own", nil), Expected: nil},
		{Name: "all for author includes private", Client: author, Query: search("all", nil), Expected: all},
		{Name: "all for viewer", Client: viewer, Query: search("all", nil), Expected: public},
		{Name: "public for viewer", Client: viewer, Query: search("public", nil), Expected: public},
		{Name: "public without token", Client: anonymous, Query: search("public", nil), Expected: public},
		{Name: "public excludes own", Client: author, Query: search("public", nil), Expected: nil},
		{Name: "basic excludes user exercises", Client: author, Query: search("basic", nil), Expected: nil},
		{
			Name: "muscle group filter", Client: author,
			Query:    search("all", url.Values{"muscle_group": {"chest"}}),
			Expected: []string{prefix + "a public chest", prefix + "c private chest"},
		},
		{
			Name: "equipment filter needs all equipment", Client: author,
			Query:    search("all", url.Values{"equipment": {"barbell", "mat"}}),
			Expected: []string{prefix + "b public abs"},
		},
		{
			Name: "pagination", Client: author,
			Query:    search("own", url.Values{"limit": {"1"}, "offset": {"1"}}),
			Expected: []string{prefix + "b public abs"},
		},
	}
	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			t.Parallel()
			c := &client{t: t, token: test.Client.token}
			require.Equal(t, test.Expected, nilIfEmpty(exerciseNames(listExercises(c, test.Query))))
		})
	}

	t.Run("own without token", func(t *testing.T) {
		t.Parallel()
		c := newClient(t)
		c.requireError(http.MethodGet, "/exercises?"+search("own", nil).Encode(), nil,
			http.StatusBadRequest, model.ExercisesFilterUserIDRequiredKey)
	})
}

func nilIfEmpty(s []string) []string {
	if len(s) == 0 {
		return nil
	}
	return s
}
