package model

import (
	"ftns-asst/internal/util"
	"strings"
	"testing"

	"github.com/go-openapi/testify/v2/require"
)

func TestExerciseValidation(t *testing.T) {
	t.Parallel()
	tests := []struct {
		Name          string
		Exercise      Exercise
		ExpectedError error
	}{
		{
			Name: "good exercise",
			Exercise: Exercise{
				Name:         "Отжимания",
				MuscleGroups: []MuscleGroupKey{MuscleGroupChest},
				Equipments:   []EquipmentKey{EquipmentBodyweight},
			},
		},
		{
			Name:     "good name with max length in cyrillic",
			Exercise: Exercise{Name: strings.Repeat("я", ExerciseNameMaxLength)},
		},
		{
			Name:          "error blank name",
			Exercise:      Exercise{Name: "   "},
			ExpectedError: ErrBadRequest,
		},
		{
			Name:          "error too long name",
			Exercise:      Exercise{Name: strings.Repeat("я", ExerciseNameMaxLength+1)},
			ExpectedError: ErrBadRequest,
		},
		{
			Name:          "error unknown muscle group",
			Exercise:      Exercise{Name: "Push ups", MuscleGroups: []MuscleGroupKey{"unknown"}},
			ExpectedError: ErrBadRequest,
		},
		{
			Name:          "error unknown equipment",
			Exercise:      Exercise{Name: "Push ups", Equipments: []EquipmentKey{"unknown"}},
			ExpectedError: ErrBadRequest,
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			t.Parallel()
			err := test.Exercise.Validate(NewTestMuscleGroupProvider(), NewTestEquipmentProvider())

			require.ErrorIs(t, err, test.ExpectedError)
		})
	}
}

func TestExerciseInputValidation(t *testing.T) {
	t.Parallel()
	tests := []struct {
		Name          string
		Input         ExerciseInput
		ExpectedError error
	}{
		{
			Name:  "good empty input",
			Input: ExerciseInput{},
		},
		{
			Name: "good input",
			Input: ExerciseInput{
				Name:         util.Ptr("Push ups"),
				MuscleGroups: []MuscleGroupKey{MuscleGroupChest},
				Equipments:   []EquipmentKey{EquipmentBodyweight},
			},
		},
		{
			Name:          "error blank name",
			Input:         ExerciseInput{Name: util.Ptr("  ")},
			ExpectedError: ErrBadRequest,
		},
		{
			Name:          "error too long name",
			Input:         ExerciseInput{Name: util.Ptr(strings.Repeat("x", ExerciseNameMaxLength+1))},
			ExpectedError: ErrBadRequest,
		},
		{
			Name:          "error unknown muscle group",
			Input:         ExerciseInput{MuscleGroups: []MuscleGroupKey{"unknown"}},
			ExpectedError: ErrBadRequest,
		},
		{
			Name:          "error unknown equipment",
			Input:         ExerciseInput{Equipments: []EquipmentKey{"unknown"}},
			ExpectedError: ErrBadRequest,
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			t.Parallel()
			err := test.Input.Validate(NewTestMuscleGroupProvider(), NewTestEquipmentProvider())

			require.ErrorIs(t, err, test.ExpectedError)
		})
	}
}

func TestExerciseFiltersValidation(t *testing.T) {
	t.Parallel()
	tests := []struct {
		Name          string
		Filters       ExerciseFilters
		ExpectedError error
	}{
		{
			Name: "good filters with duplicates",
			Filters: ExerciseFilters{
				ListType:     ExerciseListTypeBasic,
				MuscleGroups: []MuscleGroupKey{MuscleGroupChest, MuscleGroupChest},
				Equipments:   []EquipmentKey{EquipmentBarbell},
			},
		},
		{
			Name:          "error user id required",
			Filters:       ExerciseFilters{ListType: ExerciseListTypeOwn},
			ExpectedError: ErrUserIDRequired,
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			t.Parallel()
			err := test.Filters.ValidateAndDedup()

			require.ErrorIs(t, err, test.ExpectedError)
			if err == nil {
				require.Len(t, test.Filters.MuscleGroups, 1)
			}
		})
	}
}
