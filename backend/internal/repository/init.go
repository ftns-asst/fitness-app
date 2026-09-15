package repository

import (
	"context"
	"embed"
	"errors"
	"fmt"
	"ftns-asst/internal/config"
	"log"

	"github.com/golang-migrate/migrate/v4"
	migrate_pgx "github.com/golang-migrate/migrate/v4/database/pgx"
	"github.com/golang-migrate/migrate/v4/source/iofs"
	"github.com/jackc/pgx/v5/pgxpool"
	"github.com/jackc/pgx/v5/stdlib"
)

//go:embed migrations/*.sql
var migrationsFS embed.FS

func runMigrations(pool *pgxpool.Pool) error {
	sourceDriver, err := iofs.New(migrationsFS, "migrations")
	if err != nil {
		return fmt.Errorf("failed to create iofs driver: %w", err)
	}
	db := stdlib.OpenDBFromPool(pool)
	dbDriver, err := migrate_pgx.WithInstance(db, &migrate_pgx.Config{})
	if err != nil {
		return fmt.Errorf("failed to create pgx driver: %w", err)
	}
	m, err := migrate.NewWithInstance("iofs", sourceDriver, "postgres", dbDriver)
	if err != nil {
		return fmt.Errorf("failed to create migrate instance: %w", err)
	}

	if err := m.Up(); err != nil {
		if errors.Is(err, migrate.ErrNoChange) {
			log.Println("no changes to migrate")
			return nil
		}
		return fmt.Errorf("migrate up failed: %w", err)
	}

	return nil
}

func ConnectToDB(ctx context.Context, config *config.PostgresConfig) (*pgxpool.Pool, error) {
	pool, err := pgxpool.New(ctx, config.ConnString())
	if err != nil {
		return nil, fmt.Errorf("postgres connection failed: %w", err)
	}
	if err := pool.Ping(ctx); err != nil {
		return nil, fmt.Errorf("database ping failed: %w", err)
	}

	if err := runMigrations(pool); err != nil {
		return nil, fmt.Errorf("migrations failed: %w", err)
	}
	log.Println("successful migrations")
	return pool, nil
}
