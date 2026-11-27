//go:build integration

package integration

import (
	"bytes"
	"context"
	"fmt"
	"ftns-asst/internal/config"
	file_service "ftns-asst/internal/service/file"
	"io"
	"log"
	"net/http"
	"testing"
	"time"

	"github.com/stretchr/testify/require"
)

var (
	cfg = &config.MinioConfig{
		Endpoint:     "127.0.0.1:9000",
		RootUser:     "minio",
		RootPassword: "miniotest",
		BucketName:   "test",
		UseSSL:       false,
	}
	testFileData = []byte("some file data 123")
)

func TestMinioCRUD(t *testing.T) {
	//file := bytes.NewReader(testFileData)
	filename := "test.txt"
	contentType := "text/plain"
	ctx := context.Background()
	client, err := file_service.NewMinioClient(cfg)
	require.NoError(t, err)

	url, err := client.CreatePresignedPutUrl(ctx, filename, time.Hour)
	require.NoError(t, err)
	log.Println(url)

	err = uploadFile(ctx, url, bytes.NewReader(testFileData), contentType)
	require.NoError(t, err)

	url, err = client.CreatePresignedGetURL(ctx, filename, time.Hour)
	require.NoError(t, err)

	res, err := downloadFile(ctx, url)
	require.NoError(t, err)

	require.Equal(t, testFileData, res)

	err = client.DeleteObject(ctx, filename)
	require.NoError(t, err)

	url, err = client.CreatePresignedGetURL(ctx, filename, time.Hour)
	require.Error(t, err)
}

func uploadFile(ctx context.Context, putUrl string, file io.Reader, contentType string) error {
	req, _ := http.NewRequest("PUT", putUrl, file)
	req.Header.Set("Content-Type", contentType)

	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return err
	}

	if resp.StatusCode != 200 {
		return fmt.Errorf("status code %d", resp.StatusCode)
	}
	return nil
}

func downloadFile(ctx context.Context, getUrl string) ([]byte, error) {
	req, _ := http.NewRequest("GET", getUrl, nil)
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return nil, err
	}
	if resp.StatusCode != 200 {
		return nil, fmt.Errorf("status code %d", resp.StatusCode)
	}

	return io.ReadAll(resp.Body)
}
