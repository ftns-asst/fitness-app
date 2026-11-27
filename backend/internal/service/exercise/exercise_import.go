package exercise

import (
	"context"
	"fmt"
	"ftns-asst/internal/model"
	"ftns-asst/internal/util"
	"log"
	"strings"

	"github.com/google/uuid"
	excelize "github.com/xuri/excelize/v2"
)

type loadExercisesResult struct {
	exercises    []*model.Exercise
	muscleGroups model.MuscleGroupProvider
	equipment    model.EquipmentProvider
}

func (s *ExerciseService) loadExercises(ctx context.Context) (*loadExercisesResult, error) {
	xlsx, err := excelize.OpenFile(s.cfg.ExerciseCatalogPath)
	if err != nil {
		return nil, fmt.Errorf("failed to open file: %w", err)
	}
	defer func() {
		_ = xlsx.Close()
	}()

	muscleGroupList, err := parseMuscleGroupsFromExcel(xlsx)
	if err != nil {
		return nil, fmt.Errorf("failed to parse muscle groups: %w", err)
	}
	log.Printf("Loaded muscle groups: \n%q", muscleGroupList)

	equipmentList, err := parseEquipmentFromExcel(xlsx)
	if err != nil {
		return nil, fmt.Errorf("failed to parse equipments: %w", err)
	}
	log.Printf("Loaded equipment: \n%q", equipmentList)

	muscleGroups := util.NewDataReader(muscleGroupList, func(group model.MuscleGroup) string {
		return string(group.Key)
	})
	equipment := util.NewDataReader(equipmentList, func(equipment model.Equipment) string {
		return string(equipment.Key)
	})

	exercises, err := parseExercisesFromExcel(xlsx, muscleGroups, equipment)
	if err != nil {
		return nil, fmt.Errorf("failed to parse exercises: %w", err)
	}

	log.Printf("Loaded exercises:\n%+v", exercises)
	return &loadExercisesResult{
		exercises:    exercises,
		muscleGroups: muscleGroups,
		equipment:    equipment,
	}, nil
}

func parseMuscleGroupsFromExcel(file *excelize.File) ([]model.MuscleGroup, error) {
	rows, err := file.GetRows("muscle_groups")
	if err != nil {
		return nil, fmt.Errorf("failed to get muscle groups: %w", err)
	}
	if len(rows) == 0 {
		return nil, nil
	}

	// check for blank cells
	for i, row := range rows {
		if len(row) != 2 {
			return nil, fmt.Errorf("incomplete muscle groups row %d", i+1)
		}
	}

	if rows[0][0] != "key" || rows[0][1] != "name" {
		return nil, fmt.Errorf("incorrect muscle groups columns, must be: 'key', 'name'")
	}

	seen := make(model.Set[model.MuscleGroupKey], len(rows))

	result := make([]model.MuscleGroup, 0, len(rows)-1)
	for i, row := range rows[1:] {
		mg := model.MuscleGroup{
			Key:  model.MuscleGroupKey(strings.TrimSpace(row[0])),
			Name: strings.TrimSpace(row[1]),
		}
		if mg.Key == "" {
			return nil, fmt.Errorf("muscle group key is empty at row %d", i+2)
		}
		if _, ok := seen[mg.Key]; ok {
			return nil, fmt.Errorf("duplicate muscle group key at row %d", i+2)
		}
		seen[mg.Key] = struct{}{}

		if mg.Name == "" {
			return nil, fmt.Errorf("muscle group name is empty at row %d", i+2)
		}
		result = append(result, mg)
	}

	return result, nil
}

func parseEquipmentFromExcel(file *excelize.File) ([]model.Equipment, error) {
	rows, err := file.GetRows("equipment")
	if err != nil {
		return nil, fmt.Errorf("failed to get equipment: %w", err)
	}
	if len(rows) == 0 {
		return nil, nil
	}

	// check for blank cells
	for i, row := range rows {
		if len(row) != 2 {
			return nil, fmt.Errorf("incomplete equipment row %d", i+1)
		}
	}

	if rows[0][0] != "key" || rows[0][1] != "name" {
		return nil, fmt.Errorf("incorrect equipment columns, must be: 'key', 'name'")
	}

	seen := make(model.Set[model.EquipmentKey], len(rows))

	result := make([]model.Equipment, 0, len(rows)-1)
	for i, row := range rows[1:] {
		e := model.Equipment{
			Key:  model.EquipmentKey(strings.TrimSpace(row[0])),
			Name: strings.TrimSpace(row[1]),
		}
		if e.Key == "" {
			return nil, fmt.Errorf("equipment key is empty at row %d", i+2)
		}
		if _, ok := seen[e.Key]; ok {
			return nil, fmt.Errorf("duplicate equipment key at row %d", i+2)
		}
		seen[e.Key] = struct{}{}

		if e.Name == "" {
			return nil, fmt.Errorf("equipment name is empty at row %d", i+2)
		}
		result = append(result, e)
	}

	return result, nil
}

func parseExercisesFromExcel(file *excelize.File, mgp model.MuscleGroupProvider, eqp model.EquipmentProvider) ([]*model.Exercise, error) {
	rows, err := file.GetRows("exercises")
	if err != nil {
		return nil, fmt.Errorf("failed to get exercises: %w", err)
	}
	if len(rows) == 0 {
		return nil, nil
	}

	passed := make(model.Set[uuid.UUID], len(rows))

	// check for blank cells
	for i, row := range rows {
		if len(row) != 4 {
			return nil, fmt.Errorf("incomplete exercises row %d", i)
		}
	}

	if rows[0][0] != "uuid" ||
		rows[0][1] != "name" ||
		rows[0][2] != "equipment" ||
		rows[0][3] != "muscle_groups" {
		return nil, fmt.Errorf("incorrect exercises columns, must be: 'uuid', 'name', 'equipment', 'muscle_group'")
	}

	result := make([]*model.Exercise, 0, len(rows)-1)
	for i, row := range rows[1:] {
		e := model.Exercise{
			IsPublic: true,
		}
		e.ID, err = uuid.Parse(strings.TrimSpace(row[0]))
		if err != nil {
			return nil, fmt.Errorf("failed to parse exercise uuid: %w", err)
		}
		if _, ok := passed[e.ID]; ok {
			return nil, fmt.Errorf("duplicate exercise uuid: %s; at row %d", e.ID, i+2)
		}
		passed[e.ID] = struct{}{}

		e.Name = strings.TrimSpace(row[1])
		if e.Name == "" {
			return nil, fmt.Errorf("exercise name is empty at row %d", i+2)
		}

		eqs := strings.Split(strings.ReplaceAll(strings.TrimSpace(row[2]), " ", ""), ",")
		e.Equipments = make([]model.EquipmentKey, 0, len(eqs))
		for _, eq := range eqs {
			if _, ok := eqp.Get(eq); !ok {
				return nil, fmt.Errorf("unexpected exercise equipment '%s', at row %d", eq, i+2)
			}
			e.Equipments = append(e.Equipments, model.EquipmentKey(eq))
		}

		mgs := strings.Split(strings.ReplaceAll(strings.TrimSpace(row[3]), " ", ""), ",")
		e.MuscleGroups = make([]model.MuscleGroupKey, 0, len(mgs))
		for _, mg := range mgs {
			if _, ok := mgp.Get(mg); !ok {
				return nil, fmt.Errorf("unexpected exercise muscle group '%s', at row %d", mg, i+2)
			}
			e.MuscleGroups = append(e.MuscleGroups, model.MuscleGroupKey(mg))
		}

		result = append(result, &e)
	}

	return result, nil
}
