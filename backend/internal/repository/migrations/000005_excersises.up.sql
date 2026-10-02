CREATE TABLE IF NOT EXISTS exercises (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    owner_id UUID REFERENCES users(id) ON DELETE CASCADE,
    is_public BOOLEAN NOT NULL DEFAULT FALSE,
    name VARCHAR(255) NOT NULL,
    description TEXT NOT NULL,
    muscle_group TEXT[] NOT NULL DEFAULT '{}',
    equipment TEXT[] NOT NULL DEFAULT '{}',
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    deleted_at TIMESTAMPTZ
)

CREATE INDEX idx_exercises_muscle_group ON exercises USING GIN (muscle_group);
CREATE INDEX idx_exercises_equipment ON exercises USING GIN (equipment);
