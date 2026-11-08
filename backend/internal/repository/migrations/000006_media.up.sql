CREATE TABLE IF NOT EXISTS media (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    type VARCHAR(16) NOT NULL CHECK (type IN ('image', 'video')),
    object_key TEXT NOT NULL UNIQUE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

ALTER TABLE exercises
    ADD COLUMN media_id UUID REFERENCES media(id) ON DELETE SET NULL;

CREATE INDEX idx_exercises_media_id ON exercises (media_id) WHERE media_id IS NOT NULL;
