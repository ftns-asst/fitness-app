package config

import (
	"fmt"
	"ftns-asst/internal/util"
	"time"
)

// app config
type Config struct {
	HTTPServerPort  string        `env:"HTTP_SERVER_PORT,required"`
	ShutdownTimeout time.Duration `env:"SERVER_GRACEFUL_SHUTDOWN_TIMEOUT" envDefault:"10s"`
	postgres        *PostgresConfig
}

func (c *Config) Postgres() *PostgresConfig {
	return c.postgres
}

type PostgresConfig struct {
	Host     string `env:"POSTGRES_HOST,required"`
	Port     string `env:"POSTGRES_PORT,required"`
	User     string `env:"POSTGRES_USER,required"`
	Password string `env:"POSTGRES_PASSWORD,required"`
	DBName   string `env:"POSTGRES_DB,required"`
	SSLMode  string `env:"POSTGRES_SSL_MODE,required"`
}

func (c *PostgresConfig) ConnString() string {
	return fmt.Sprintf("host=%s port=%s user=%s password=%s dbname=%s sslmode=%s",
		c.Host, c.Port, c.User, c.Password, c.DBName, c.SSLMode)
}

// reads config, panics on error
func MustRead() *Config {
	cfg := util.Ptr(util.MustReadFromEnv[Config]())
	cfg.postgres = util.Ptr(util.MustReadFromEnv[PostgresConfig]())
	return cfg
}
