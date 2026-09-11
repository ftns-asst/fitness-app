package app

import (
	"context"
	"errors"
	"fmt"
	"ftns-asst/internal/config"
	"ftns-asst/internal/repository"
	"ftns-asst/internal/rest"
	"ftns-asst/internal/service"
	"log"
	"net/http"
)

type App struct {
	config     *config.Config
	httpServer *http.Server
}

// initialize app components
func New(ctx context.Context, cfg *config.Config) *App {
	_, err := repository.ConnectToDB(ctx, cfg.Postgres())
	if err != nil {
		log.Panicf("failed connect to DB: %s", err.Error())
	}

	services := service.NewServices(cfg)

	httpServer := rest.NewServer(cfg, *services)

	return &App{
		config:     cfg,
		httpServer: httpServer,
	}
}

// run http server and services
func (a *App) Run(rootCtx context.Context) error {
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
	err := a.httpServer.ListenAndServe()
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
