package file_service

import (
	"context"
	"errors"
	"fmt"
	"ftns-asst/internal/config"
	"ftns-asst/internal/model"
	"ftns-asst/internal/repository"
	"time"

	"github.com/google/uuid"
)

const (
	putURLExpiry = 15 * time.Minute
	getURLExpiry = time.Hour
)

// thin service over media table and object storage, knows nothing about entities that use media
type FileService struct {
	cfg     *config.Config
	repo    FileRepo
	storage FileStorage
	tx      model.Transactor
}

func NewService(cfg *config.Config,
	repo FileRepo,
	storage FileStorage,
	transactor model.Transactor,
) *FileService {
	return &FileService{
		cfg:     cfg,
		repo:    repo,
		storage: storage,
		tx:      transactor,
	}
}

func validateMediaType(mediaType model.MediaType) error {
	if mediaType != model.MediaTypeImage && mediaType != model.MediaTypeVideo {
		return fmt.Errorf("unknown media type %q: %w", mediaType, model.ErrBadRequest)
	}
	return nil
}

// saves media record and returns it with url for uploading the file
func (s *FileService) CreateMedia(ctx context.Context, mediaType model.MediaType) (*model.Media, string, error) {
	if err := validateMediaType(mediaType); err != nil {
		return nil, "", err
	}

	media, err := s.repo.CreateMedia(ctx, &model.Media{
		Type:      mediaType,
		ObjectKey: uuid.NewString(),
	})
	if err != nil {
		return nil, "", fmt.Errorf("failed to create media: %w", err)
	}

	putURL, err := s.storage.CreatePresignedPutUrl(ctx, media.ObjectKey, putURLExpiry)
	if err != nil {
		// no one got the url, do not leave orphan record
		if _, delErr := s.repo.DeleteMedia(ctx, media.ID); delErr != nil {
			return nil, "", fmt.Errorf("failed to create put url: %w (rollback failed: %w)", err, delErr)
		}
		return nil, "", fmt.Errorf("failed to create put url: %w", err)
	}
	return media, putURL, nil
}

// returns url for replacing the file of existing media, the record is not changed,
// so media type must match the stored one
func (s *FileService) UpdateMedia(ctx context.Context, id uuid.UUID, mediaType model.MediaType) (*model.Media, string, error) {
	if err := validateMediaType(mediaType); err != nil {
		return nil, "", err
	}

	media, err := s.repo.GetMediaByID(ctx, id)
	if errors.Is(err, repository.ErrNotFound) {
		return nil, "", fmt.Errorf("media not found: %w", model.ErrNotFound)
	}
	if err != nil {
		return nil, "", fmt.Errorf("failed to get media: %w", err)
	}

	if media.Type != mediaType {
		return nil, "", fmt.Errorf("media type %q does not match stored type %q: %w", mediaType, media.Type, model.ErrBadRequest)
	}

	putURL, err := s.storage.CreatePresignedPutUrl(ctx, media.ObjectKey, putURLExpiry)
	if err != nil {
		return nil, "", fmt.Errorf("failed to create put url: %w", err)
	}
	return media, putURL, nil
}

// creates new media if id is nil, otherwise returns url for replacing the file of existing one
func (s *FileService) UpsertMedia(ctx context.Context, id *uuid.UUID, mediaType model.MediaType) (*model.Media, string, error) {
	if id == nil {
		return s.CreateMedia(ctx, mediaType)
	}
	return s.UpdateMedia(ctx, *id, mediaType)
}

// checks that media exists and returns it with url for downloading
func (s *FileService) GetMedia(ctx context.Context, id uuid.UUID) (*model.Media, string, error) {
	media, err := s.repo.GetMediaByID(ctx, id)
	if errors.Is(err, repository.ErrNotFound) {
		return nil, "", fmt.Errorf("media not found: %w", model.ErrNotFound)
	}
	if err != nil {
		return nil, "", fmt.Errorf("failed to get media: %w", err)
	}

	getURL, err := s.storage.CreatePresignedGetURL(ctx, media.ObjectKey, getURLExpiry)
	if err != nil {
		return nil, "", fmt.Errorf("failed to create get url: %w", err)
	}
	media.URL = getURL
	return media, getURL, nil
}

// returns media with get urls by id, ids that are not found or have no file are skipped
func (s *FileService) GetMediaList(ctx context.Context, ids []uuid.UUID) (map[uuid.UUID]*model.Media, error) {
	res := make(map[uuid.UUID]*model.Media, len(ids))
	if len(ids) == 0 {
		return res, nil
	}

	list, err := s.repo.GetMediaByIDs(ctx, ids)
	if err != nil {
		return nil, fmt.Errorf("failed to get media: %w", err)
	}

	for _, media := range list {
		getURL, err := s.storage.CreatePresignedGetURL(ctx, media.ObjectKey, getURLExpiry)
		if errors.Is(err, model.ErrNotFound) {
			continue // record created but file was not uploaded
		}
		if err != nil {
			return nil, fmt.Errorf("failed to create get url: %w", err)
		}
		media.URL = getURL
		res[media.ID] = media
	}
	return res, nil
}

// deletes media record and file, record is kept if file was not deleted
func (s *FileService) DeleteMedia(ctx context.Context, id uuid.UUID) error {
	tx, err := s.tx.Begin(ctx)
	if err != nil {
		return fmt.Errorf("failed begin transaction: %w", err)
	}
	defer model.CheckRollback(ctx, tx)
	ctx = context.WithValue(ctx, model.ContextKeyTx, tx)

	media, err := s.repo.DeleteMedia(ctx, id)
	if errors.Is(err, repository.ErrNotFound) {
		return fmt.Errorf("media not found: %w", model.ErrNotFound)
	}
	if err != nil {
		return fmt.Errorf("failed to delete media: %w", err)
	}

	if err = s.storage.DeleteObject(ctx, media.ObjectKey); err != nil {
		return fmt.Errorf("failed to delete object: %w", err)
	}

	if err = tx.Commit(ctx); err != nil {
		return fmt.Errorf("failed to commit transaction: %w", err)
	}
	return nil
}
