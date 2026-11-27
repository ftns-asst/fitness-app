package static

import "embed"

// static files
//
//go:embed files/*
var StaticFiles embed.FS
