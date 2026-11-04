CREATE TABLE user_programs (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    name VARCHAR(255) NOT NULL,
    source_kind TEXT NOT NULL,
    default_program_id UUID REFERENCES default_programs(id) ON DELETE SET NULL,
    goal TEXT NOT NULL,
    level TEXT NOT NULL,
    state TEXT NOT NULL DEFAULT 'draft',
    start_date DATE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE UNIQUE INDEX ux_user_programs_one_active ON user_programs (user_id) WHERE state = 'active';
CREATE INDEX idx_user_programs_user ON user_programs (user_id, updated_at DESC);

CREATE TABLE user_program_days (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    program_id UUID NOT NULL REFERENCES user_programs(id) ON DELETE CASCADE,
    name VARCHAR(255) NOT NULL,
    order_index INT NOT NULL,
    archived_at TIMESTAMPTZ
);

CREATE UNIQUE INDEX ux_user_program_days_order ON user_program_days (program_id, order_index) WHERE archived_at IS NULL;

CREATE TABLE user_program_day_exercises (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    program_day_id UUID NOT NULL REFERENCES user_program_days(id) ON DELETE CASCADE,
    exercise_id UUID NOT NULL REFERENCES exercises(id),
    order_index INT NOT NULL,
    sets INT NOT NULL CHECK (sets BETWEEN 1 AND 20),
    target_reps INT NOT NULL CHECK (target_reps >= 1),
    target_weight_kg NUMERIC(6, 2) CHECK (target_weight_kg >= 0),
    rest_seconds INT CHECK (rest_seconds >= 0),
    UNIQUE (program_day_id, order_index)
);

CREATE TABLE user_program_schedule (
    program_id UUID NOT NULL REFERENCES user_programs(id) ON DELETE CASCADE,
    day_of_week SMALLINT NOT NULL CHECK (day_of_week BETWEEN 1 AND 7),
    program_day_id UUID NOT NULL REFERENCES user_program_days(id),
    PRIMARY KEY (program_id, day_of_week)
);
