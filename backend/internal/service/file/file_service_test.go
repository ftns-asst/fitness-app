package file_service

import (
	"context"
	"errors"
	"ftns-asst/internal/config"
	"ftns-asst/internal/model"
	"ftns-asst/internal/repository"
	"ftns-asst/internal/util/testutil"
	"testing"

	"github.com/google/uuid"
	"github.com/stretchr/testify/mock"
	"github.com/stretchr/testify/require"
)

type deps struct {
	repo    *MockFileRepo
	storage *MockFileStorage
}

func newTestService(t *testing.T) (*FileService, *deps) {
	d := &deps{
		repo:    NewMockFileRepo(t),
		storage: NewMockFileStorage(t),
	}
	return NewService(&config.Config{}, d.repo, d.storage, &testutil.MockTransactor{}), d
}

func TestCreateMedia(t *testing.T) {
	ctx := context.Background()

	t.Run("saves record and returns put url", func(t *testing.T) {
		svc, d := newTestService(t)
		id := uuid.New()
		d.repo.EXPECT().CreateMedia(ctx, mock.Anything).
			RunAndReturn(func(_ context.Context, m *model.Media) (*model.Media, error) {
				require.Equal(t, model.MediaTypeImage, m.Type)
				_, err := uuid.Parse(m.ObjectKey)
				require.NoError(t, err)
				m.ID = id
				return m, nil
			})
		d.storage.EXPECT().CreatePresignedPutUrl(ctx, mock.Anything, putURLExpiry).Return("http://put", nil)

		media, url, err := svc.CreateMedia(ctx, model.MediaTypeImage)
		require.NoError(t, err)
		require.Equal(t, id, media.ID)
		require.Equal(t, "http://put", url)
	})

	t.Run("invalid input is not saved", func(t *testing.T) {
		svc, _ := newTestService(t)

		_, _, err := svc.CreateMedia(ctx, "audio")
		require.ErrorIs(t, err, model.ErrBadRequest)
		_, _, err = svc.CreateMedia(ctx, "")
		require.ErrorIs(t, err, model.ErrBadRequest)
	})

	t.Run("record is removed if url was not created", func(t *testing.T) {
		svc, d := newTestService(t)
		id := uuid.New()
		d.repo.EXPECT().CreateMedia(ctx, mock.Anything).
			RunAndReturn(func(_ context.Context, m *model.Media) (*model.Media, error) {
				m.ID = id
				return m, nil
			})
		d.storage.EXPECT().CreatePresignedPutUrl(ctx, mock.Anything, putURLExpiry).Return("", errors.New("boom"))
		d.repo.EXPECT().DeleteMedia(ctx, id).Return(&model.Media{ID: id}, nil)

		_, _, err := svc.CreateMedia(ctx, model.MediaTypeVideo)
		require.Error(t, err)
	})
}

func TestGetMedia(t *testing.T) {
	ctx := context.Background()
	media := &model.Media{ID: uuid.New(), Type: model.MediaTypeImage, ObjectKey: "a.png"}

	t.Run("returns media with get url", func(t *testing.T) {
		svc, d := newTestService(t)
		d.repo.EXPECT().GetMediaByID(ctx, media.ID).Return(&model.Media{ID: media.ID, ObjectKey: media.ObjectKey}, nil)
		d.storage.EXPECT().CreatePresignedGetURL(ctx, "a.png", getURLExpiry).Return("http://get", nil)

		got, url, err := svc.GetMedia(ctx, media.ID)
		require.NoError(t, err)
		require.Equal(t, "http://get", url)
		require.Equal(t, "http://get", got.URL)
	})

	t.Run("no record", func(t *testing.T) {
		svc, d := newTestService(t)
		d.repo.EXPECT().GetMediaByID(ctx, media.ID).Return(nil, repository.ErrNotFound)

		_, _, err := svc.GetMedia(ctx, media.ID)
		require.ErrorIs(t, err, model.ErrNotFound)
	})

	t.Run("no file", func(t *testing.T) {
		svc, d := newTestService(t)
		d.repo.EXPECT().GetMediaByID(ctx, media.ID).Return(media, nil)
		d.storage.EXPECT().CreatePresignedGetURL(ctx, "a.png", getURLExpiry).Return("", model.ErrNotFound)

		_, _, err := svc.GetMedia(ctx, media.ID)
		require.ErrorIs(t, err, model.ErrNotFound)
	})
}

func TestGetMediaList(t *testing.T) {
	ctx := context.Background()
	withFile := &model.Media{ID: uuid.New(), ObjectKey: "a.png"}
	withoutFile := &model.Media{ID: uuid.New(), ObjectKey: "b.png"}
	ids := []uuid.UUID{withFile.ID, withoutFile.ID}

	t.Run("skips media without file", func(t *testing.T) {
		svc, d := newTestService(t)
		d.repo.EXPECT().GetMediaByIDs(ctx, ids).Return([]*model.Media{withFile, withoutFile}, nil)
		d.storage.EXPECT().CreatePresignedGetURL(ctx, "a.png", getURLExpiry).Return("http://a", nil)
		d.storage.EXPECT().CreatePresignedGetURL(ctx, "b.png", getURLExpiry).Return("", model.ErrNotFound)

		res, err := svc.GetMediaList(ctx, ids)
		require.NoError(t, err)
		require.Len(t, res, 1)
		require.Equal(t, "http://a", res[withFile.ID].URL)
	})

	t.Run("empty ids do not touch db", func(t *testing.T) {
		svc, _ := newTestService(t)

		res, err := svc.GetMediaList(ctx, nil)
		require.NoError(t, err)
		require.Empty(t, res)
	})
}

func TestDeleteMedia(t *testing.T) {
	ctx := context.Background()
	id := uuid.New()

	t.Run("deletes record and file", func(t *testing.T) {
		svc, d := newTestService(t)
		d.repo.EXPECT().DeleteMedia(mock.Anything, id).Return(&model.Media{ID: id, ObjectKey: "a.png"}, nil)
		d.storage.EXPECT().DeleteObject(mock.Anything, "a.png").Return(nil)

		require.NoError(t, svc.DeleteMedia(ctx, id))
	})

	t.Run("no record", func(t *testing.T) {
		svc, d := newTestService(t)
		d.repo.EXPECT().DeleteMedia(mock.Anything, id).Return(nil, repository.ErrNotFound)

		require.ErrorIs(t, svc.DeleteMedia(ctx, id), model.ErrNotFound)
	})

	t.Run("file delete error", func(t *testing.T) {
		svc, d := newTestService(t)
		d.repo.EXPECT().DeleteMedia(mock.Anything, id).Return(&model.Media{ID: id, ObjectKey: "a.png"}, nil)
		d.storage.EXPECT().DeleteObject(mock.Anything, "a.png").Return(errors.New("boom"))

		require.Error(t, svc.DeleteMedia(ctx, id))
	})
}

func TestUpdateMedia(t *testing.T) {
	ctx := context.Background()
	media := &model.Media{ID: uuid.New(), Type: model.MediaTypeImage, ObjectKey: "a.png"}

	t.Run("returns put url for the same object, record is not changed", func(t *testing.T) {
		svc, d := newTestService(t)
		d.repo.EXPECT().GetMediaByID(ctx, media.ID).Return(media, nil)
		d.storage.EXPECT().CreatePresignedPutUrl(ctx, "a.png", putURLExpiry).Return("http://put", nil)

		got, url, err := svc.UpdateMedia(ctx, media.ID, model.MediaTypeImage)
		require.NoError(t, err)
		require.Equal(t, media, got)
		require.Equal(t, "http://put", url)
	})

	t.Run("no record", func(t *testing.T) {
		svc, d := newTestService(t)
		d.repo.EXPECT().GetMediaByID(ctx, media.ID).Return(nil, repository.ErrNotFound)

		_, _, err := svc.UpdateMedia(ctx, media.ID, model.MediaTypeImage)
		require.ErrorIs(t, err, model.ErrNotFound)
	})

	t.Run("type mismatch", func(t *testing.T) {
		svc, d := newTestService(t)
		d.repo.EXPECT().GetMediaByID(ctx, media.ID).Return(media, nil)

		_, _, err := svc.UpdateMedia(ctx, media.ID, model.MediaTypeVideo)
		require.ErrorIs(t, err, model.ErrBadRequest)
	})

	t.Run("unknown type", func(t *testing.T) {
		svc, _ := newTestService(t)

		_, _, err := svc.UpdateMedia(ctx, media.ID, "audio")
		require.ErrorIs(t, err, model.ErrBadRequest)
	})
}

func TestUpsertMedia(t *testing.T) {
	ctx := context.Background()

	t.Run("nil id creates media", func(t *testing.T) {
		svc, d := newTestService(t)
		d.repo.EXPECT().CreateMedia(ctx, mock.Anything).
			RunAndReturn(func(_ context.Context, m *model.Media) (*model.Media, error) {
				m.ID = uuid.New()
				return m, nil
			})
		d.storage.EXPECT().CreatePresignedPutUrl(ctx, mock.Anything, putURLExpiry).Return("http://put", nil)

		media, url, err := svc.UpsertMedia(ctx, nil, model.MediaTypeImage)
		require.NoError(t, err)
		require.NotEqual(t, uuid.Nil, media.ID)
		require.Equal(t, "http://put", url)
	})

	t.Run("existing id returns url for the same object", func(t *testing.T) {
		svc, d := newTestService(t)
		media := &model.Media{ID: uuid.New(), Type: model.MediaTypeImage, ObjectKey: "a"}
		d.repo.EXPECT().GetMediaByID(ctx, media.ID).Return(media, nil)
		d.storage.EXPECT().CreatePresignedPutUrl(ctx, "a", putURLExpiry).Return("http://put", nil)

		got, url, err := svc.UpsertMedia(ctx, &media.ID, model.MediaTypeImage)
		require.NoError(t, err)
		require.Equal(t, media, got)
		require.Equal(t, "http://put", url)
	})

	t.Run("unknown id is not created", func(t *testing.T) {
		svc, d := newTestService(t)
		id := uuid.New()
		d.repo.EXPECT().GetMediaByID(ctx, id).Return(nil, repository.ErrNotFound)

		_, _, err := svc.UpsertMedia(ctx, &id, model.MediaTypeImage)
		require.ErrorIs(t, err, model.ErrNotFound)
	})
}
