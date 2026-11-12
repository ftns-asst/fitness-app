package program

import (
	"context"
	"ftns-asst/internal/model"

	"github.com/google/uuid"
)

type ProgramRepo interface {
	GetDefaultPrograms(ctx context.Context, filters *model.DefaultProgramFilters) ([]*model.DefaultProgram, error)
	GetDefaultProgramByID(ctx context.Context, id uuid.UUID) (*model.DefaultProgram, error)
	CreateUserProgram(ctx context.Context, program *model.UserProgram, weeks []*model.UserProgramWeek) (*model.UserProgram, error)
	GetUserPrograms(ctx context.Context, userID uuid.UUID, state *model.ProgramState) ([]*model.UserProgram, error)
	GetUserProgramByID(ctx context.Context, id uuid.UUID) (*model.UserProgram, error)
	UpdateUserProgram(ctx context.Context, input *model.UpdateUserProgramInput) (*model.UserProgram, error)
	ReplaceUserProgramStructure(ctx context.Context, programID uuid.UUID, structure *model.UserProgramStructure) error
	ReplaceUserProgramWeek(ctx context.Context, programID uuid.UUID, weekNumber int, week *model.UserProgramWeek) error
	StartUserProgram(ctx context.Context, userID uuid.UUID, programID uuid.UUID) (*uuid.UUID, error)
	FinishUserProgram(ctx context.Context, id uuid.UUID) error
	DeleteDraftUserProgram(ctx context.Context, id uuid.UUID) error
	GetMissingMeasurements(ctx context.Context, programID uuid.UUID, userID uuid.UUID) ([]*model.MeasurementExercise, error)
	SetUserProgramState(ctx context.Context, id uuid.UUID, state model.ProgramState) error
	AreExercisesAvailable(ctx context.Context, userID uuid.UUID, exerciseIDs []uuid.UUID) (bool, error)
}
