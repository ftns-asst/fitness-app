package config

import (
	"ftns-asst/internal/util"
	"time"
)

// app config
type Config struct {
	HTTPServerPort     string        `env:"HTTP_SERVER_PORT,required"`
	ShutdownTimeoutSec time.Duration `env:"SERVER_GRACEFUL_SHUTDOWN_TIMEOUT" envDefault:"10s"`
}

// reads config, panics on error
func MustRead() *Config {
	return util.Ptr(util.MustReadFromEnv[Config]())
}
