package file_service

import (
	"context"
	"ftns-asst/internal/model"
	"time"

	"github.com/google/uuid"
)

type FileRepo interface {
	CreateMedia(ctx context.Context, media *model.Media) (*model.Media, error)
	GetMediaByID(ctx context.Context, id uuid.UUID) (*model.Media, error)
	GetMediaByIDs(ctx context.Context, ids []uuid.UUID) ([]*model.Media, error)
	DeleteMedia(ctx context.Context, id uuid.UUID) (*model.Media, error)
}

// object storage, implemented by MinioClient
type FileStorage interface {
	CreatePresignedPutUrl(ctx context.Context, objectName string, expiry time.Duration) (string, error)
	CreatePresignedGetURL(ctx context.Context, objectName string, expiry time.Duration) (string, error)
	DeleteObject(ctx context.Context, objectName string) error
}
