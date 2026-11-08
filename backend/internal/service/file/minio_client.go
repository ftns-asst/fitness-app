package file_service

import (
	"context"
	"fmt"
	"ftns-asst/internal/config"
	"ftns-asst/internal/model"
	"log"
	"net/url"
	"time"

	"github.com/minio/minio-go/v7"
	"github.com/minio/minio-go/v7/pkg/credentials"
)

// обертка для minio.Client
type MinioClient struct {
	Client     *minio.Client
	BucketName string
}

func NewMinioClient(config *config.MinioConfig) (*MinioClient, error) {
	client, err := minio.New(config.Endpoint, &minio.Options{
		Creds:  credentials.NewStaticV4(config.RootUser, config.RootPassword, ""),
		Secure: config.UseSSL,
	})
	if err != nil {
		return nil, err
	}

	// проверяем, есть ли бакет, если нет — создаём
	ctx := context.Background()
	exists, errBucketExists := client.BucketExists(ctx, config.BucketName)
	if errBucketExists != nil {
		return nil, errBucketExists
	}
	if !exists {
		if err = client.MakeBucket(ctx, config.BucketName, minio.MakeBucketOptions{}); err != nil {
			return nil, err
		}
	}

	return &MinioClient{Client: client, BucketName: config.BucketName}, nil
}

// creates new url for uploading
func (mc *MinioClient) CreatePresignedPutUrl(ctx context.Context, objectName string, expiry time.Duration) (string, error) {
	presignedURL, err := mc.Client.PresignedPutObject(ctx, mc.BucketName, objectName, expiry)
	if err != nil {
		return "", fmt.Errorf("failed to generate presigned url: %w", err)
	}

	return presignedURL.String(), nil
}

// creates new get url for downloading
func (mc *MinioClient) CreatePresignedGetURL(ctx context.Context, objectName string, expiry time.Duration) (string, error) {
	if _, err := mc.Client.StatObject(ctx, mc.BucketName, objectName, minio.StatObjectOptions{}); err != nil {
		if minio.ToErrorResponse(err).Code == minio.NoSuchKey {
			return "", fmt.Errorf("object %s: %w", objectName, model.ErrNotFound)
		}
		return "", fmt.Errorf("failed to stat object: %w", err)
	}
	reqParams := make(url.Values)
	url, err := mc.Client.PresignedGetObject(ctx, mc.BucketName, objectName, expiry, reqParams)
	if err != nil {
		return "", fmt.Errorf("failed to generate presigned url: %w", err)
	}
	log.Printf("generated URL: %s", url.String())
	log.Printf("client endpoint: %s", mc.Client.EndpointURL())

	return url.String(), nil
}

// deletes object, missing object is not an error
func (mc *MinioClient) DeleteObject(ctx context.Context, objectName string) error {
	return mc.Client.RemoveObject(ctx, mc.BucketName, objectName, minio.RemoveObjectOptions{})
}
