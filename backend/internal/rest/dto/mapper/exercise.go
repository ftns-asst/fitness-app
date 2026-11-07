package mapper

import (
	"ftns-asst/internal/model"
	"ftns-asst/internal/rest/dto"
	"ftns-asst/internal/util"

	"github.com/google/uuid"
)

func ConvertExerciseToDTO(m *model.Exercise) *dto.ExerciseDTO {
	if m == nil {
		return nil
	}
	return &dto.ExerciseDTO{
		ID:          m.ID,
		Name:        m.Name,
		Description: m.Description,
		IsPublic:    m.IsPublic,
		OwnerID:     m.OwnerID,
		MuscleGroups: util.Map(m.MuscleGroups, func(m model.MuscleGroupKey) string {
			return string(m)
		}),
		Equipment: util.Map(m.Equipments, func(m model.EquipmentKey) string {
			return string(m)
		}),
		CreatedAt: m.CreatedAt,
	}
}

func ConvertExerciseFromDTO(dto *dto.ExerciseDTO) *model.Exercise {
	if dto == nil {
		return nil
	}
	return &model.Exercise{
		ID:          dto.ID,
		Name:        dto.Name,
		Description: dto.Description,
		OwnerID:     dto.OwnerID,
		MuscleGroups: util.Map(dto.MuscleGroups, func(s string) model.MuscleGroupKey {
			return model.MuscleGroupKey(s)
		}),
		Equipments: util.Map(dto.Equipment, func(s string) model.EquipmentKey {
			return model.EquipmentKey(s)
		}),
		CreatedAt: dto.CreatedAt,
	}
}

func ConvertExerciseFromCreateDTO(dto *dto.CreateExerciseRequestBody, userID *uuid.UUID) *model.Exercise {
	if dto == nil {
		return nil
	}
	return &model.Exercise{
		Name:        dto.Name,
		Description: dto.Description,
		OwnerID:     userID,
		IsPublic:    dto.IsPublic,
		MuscleGroups: util.Map(dto.MuscleGroups, func(s string) model.MuscleGroupKey {
			return model.MuscleGroupKey(s)
		}),
		Equipments: util.Map(dto.Equipment, func(s string) model.EquipmentKey {
			return model.EquipmentKey(s)
		}),
	}
}
func ConvertExerciseInputFromDTO(d *dto.ExerciseInfoDTO) *model.ExerciseInput {
	if d == nil {
		return nil
	}
	return &model.ExerciseInput{
		Name:        d.Name,
		Description: d.Description,
		IsPublic:    d.IsPublic,
		MuscleGroups: util.Map(d.MuscleGroups, func(s string) model.MuscleGroupKey {
			return model.MuscleGroupKey(s)
		}),
		Equipments: util.Map(d.Equipment, func(s string) model.EquipmentKey {
			return model.EquipmentKey(s)
		}),
	}
}

func ConvertUpdateExerciseInputFromDTO(d *dto.UpdateExerciseRequestBody) *model.UpdateExerciseInput {
	if d == nil {
		return nil
	}
	return &model.UpdateExerciseInput{
		ID:            d.ID,
		ExerciseInput: util.UnPtr(ConvertExerciseInputFromDTO(&d.ExerciseInfoDTO)),
	}
}

func ConvertListExerciseQueryParams(p *dto.ListExerciseQueryParams, userID *uuid.UUID) (*model.ExerciseFilters, *model.PageInfo) {
	if p == nil {
		return nil, nil
	}
	if userID != nil && *userID == uuid.Nil {
		userID = nil
	}
	return &model.ExerciseFilters{
			UserID:       userID,
			MuscleGroups: util.Map(p.MuscleGroups, func(s string) model.MuscleGroupKey { return model.MuscleGroupKey(s) }),
			Equipments:   util.Map(p.Equipment, func(s string) model.EquipmentKey { return model.EquipmentKey(s) }),
			ListType:     model.ExerciseListType(p.Type),
			Search:       p.Search,
		},
		&model.PageInfo{
			Limit:  p.Limit,
			Offset: p.Offset,
		}
}

func ConvertMuscleGroupListToDTO(d []model.MuscleGroup) []dto.MuscleGroup {
	return util.Map(d, func(m model.MuscleGroup) dto.MuscleGroup {
		return dto.MuscleGroup{
			Key:  string(m.Key),
			Name: m.Name,
		}
	})
}

func ConvertEquipmentListToDTO(d []model.Equipment) []dto.Equipment {
	return util.Map(d, func(m model.Equipment) dto.Equipment {
		return dto.Equipment{
			Key:  string(m.Key),
			Name: m.Name,
		}
	})
}
