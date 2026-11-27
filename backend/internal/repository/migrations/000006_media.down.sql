DROP INDEX IF EXISTS idx_exercises_media_id;
ALTER TABLE exercises DROP COLUMN IF EXISTS media_id;
DROP TABLE IF EXISTS media;
