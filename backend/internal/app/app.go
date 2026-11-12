package app

import (
	"context"
	"errors"
	"fmt"
	"ftns-asst/internal/config"
	"ftns-asst/internal/repository"
	"ftns-asst/internal/rest"
	"ftns-asst/internal/service"
	file_service "ftns-asst/internal/service/file"
	"ftns-asst/internal/util"
	"log"
	"net/http"
)

type App struct {
	config     *config.Config
	httpServer *http.Server
	services   *service.Services
}

// initialize app components
func New(ctx context.Context, cfg *config.Config) *App {
	db, err := repository.ConnectToDB(ctx, cfg.Postgres())
	if err != nil {
		log.Panicf("failed connect to DB: %s", err.Error())
	}

	mailer := util.NewSMTPClient(cfg.SMTP())
	repositories := repository.NewRepositories(db)
	storage, err := file_service.NewMinioClient(cfg.Minio())
	if err != nil {
		log.Panicf("failed connect to minio: %s", err.Error())
	}
	services := service.NewServices(cfg, *repositories, mailer, db, storage)

	httpServer := rest.NewServer(cfg, *services)

	return &App{
		config:     cfg,
		httpServer: httpServer,
		services:   services,
	}
}

// run http server and services
func (a *App) Run(rootCtx context.Context) error {
	err := a.services.Exercise().Start(rootCtx)
	if err != nil {
		return fmt.Errorf("exercise service start failed: %w", err)
	}
	err = a.services.Program().Start(rootCtx)
	if err != nil {
		return fmt.Errorf("program service start failed: %w", err)
	}
	shutdownChan := make(chan error)
	go func() {
		<-rootCtx.Done()
		shutdownCtx, cancel := context.WithTimeout(rootCtx, a.config.ShutdownTimeout)
		defer cancel()
		err := a.httpServer.Shutdown(shutdownCtx)
		if err != nil {
			shutdownChan <- fmt.Errorf("server shutdown error: %w", err)
			return
		}
		shutdownChan <- nil
	}()

	// http server starting
	err = a.httpServer.ListenAndServe()
	if err != nil && !errors.Is(err, http.ErrServerClosed) {
		return fmt.Errorf("http listen and server error: %w", err)
	}

	// waiting for graceful shutdown
	err = <-shutdownChan
	if err != nil {
		return err
	}
	return nil
}
