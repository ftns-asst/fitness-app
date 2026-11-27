package main

import (
	"context"
	"ftns-asst/internal/app"
	"ftns-asst/internal/config"
	"log"
	"os/signal"
	"syscall"
)

// @title           Fitness Assistant API
// @version         1.0
// @host            fitness.nought.ru
// @BasePath        /api/v1
func main() {
	cfg := config.MustRead()

	rootCtx, cancel := signal.NotifyContext(
		context.Background(), syscall.SIGINT, syscall.SIGTERM,
	)
	defer cancel()

	app := app.New(rootCtx, cfg)

	if err := app.Run(rootCtx); err != nil {
		log.Fatalf("failed to run: %v", err)
	}
}
