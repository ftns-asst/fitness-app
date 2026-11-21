package service

import (
	"ftns-asst/internal/config"
	"ftns-asst/internal/model"
	"ftns-asst/internal/repository"
	"ftns-asst/internal/service/auth"
	"ftns-asst/internal/service/exercise"
	file_service "ftns-asst/internal/service/file"
	"ftns-asst/internal/service/personalrecord"
	"ftns-asst/internal/service/program"
	"ftns-asst/internal/service/users"
)

// all services
//
//nolint:unused
type Services struct {
	userService           *users.UserService
	authService           *auth.AuthService
	exerciseService       *exercise.ExerciseService
	fileService           *file_service.FileService
	programService        *program.ProgramService
	personalRecordService *personalrecord.PersonalRecordService
}

func NewServices(cfg *config.Config, repositories repository.Repositories, mailer model.Mailer, transactor model.Transactor, storage file_service.FileStorage) *Services {
	user := users.NewService(cfg, repositories.User(), transactor)
	auth := auth.NewService(cfg, user, repositories.Auth(), mailer, transactor)
	file := file_service.NewService(cfg, repositories.File(), storage, transactor)
	exercise := exercise.NewService(cfg, repositories.Exercise(), file, transactor)
	program := program.NewService(cfg, repositories.Program(), transactor)
	personalRecord := personalrecord.NewService(repositories.PersonalRecord(), program, transactor)
	return &Services{
		userService:           user,
		authService:           auth,
		exerciseService:       exercise,
		fileService:           file,
		programService:        program,
		personalRecordService: personalRecord,
	}
}

func (s *Services) User() *users.UserService {
	return s.userService
}

func (s *Services) Auth() *auth.AuthService {
	return s.authService
}

func (s *Services) Exercise() *exercise.ExerciseService {
	return s.exerciseService
}

func (s *Services) File() *file_service.FileService {
	return s.fileService
}

func (s *Services) Program() *program.ProgramService {
	return s.programService
}

func (s *Services) PersonalRecord() *personalrecord.PersonalRecordService {
	return s.personalRecordService
}
