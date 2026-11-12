package mapper

import (
	"ftns-asst/internal/model"
	"ftns-asst/internal/rest/dto"
)

func ConvertPersonalRecordToDTO(m *model.PersonalRecord) *dto.PersonalRecordDTO {
	if m == nil {
		return nil
	}
	return &dto.PersonalRecordDTO{
		ID:           m.ID,
		ExerciseID:   m.ExerciseID,
		ExerciseName: m.ExerciseName,
		WeightKg:     m.WeightKg,
		IsCalculated: m.IsCalculated,
		CreatedAt:    m.CreatedAt,
	}
}

func ConvertCreatePersonalRecordInputFromDTO(d *dto.CreatePersonalRecordRequestBody) *model.CreatePersonalRecordInput {
	if d == nil {
		return nil
	}
	return &model.CreatePersonalRecordInput{
		ExerciseID: d.ExerciseID,
		WeightKg:   d.WeightKg,
		Reps:       d.Reps,
	}
}

func ConvertCreatePersonalRecordsInputFromDTO(d *dto.CreatePersonalRecordsRequestBody) []*model.CreatePersonalRecordInput {
	if d == nil {
		return nil
	}

	res := make([]*model.CreatePersonalRecordInput, 0, len(d.Records))
	for i := range d.Records {
		res = append(res, ConvertCreatePersonalRecordInputFromDTO(&d.Records[i]))
	}
	return res
}
