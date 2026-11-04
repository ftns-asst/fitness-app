CREATE TABLE user_programs (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    name VARCHAR(255) NOT NULL,
    source_kind TEXT NOT NULL,
    default_program_id UUID REFERENCES default_programs(id) ON DELETE SET NULL,
    goal TEXT NOT NULL,
    level TEXT NOT NULL,
    state TEXT NOT NULL DEFAULT 'draft',
    days_of_week SMALLINT[] NOT NULL DEFAULT '{}'
        CHECK (days_of_week <@ ARRAY[1, 2, 3, 4, 5, 6, 7]::SMALLINT[]),
    weeks_count INT NOT NULL DEFAULT 1 CHECK (weeks_count BETWEEN 1 AND 52),
    start_date DATE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE UNIQUE INDEX ux_user_programs_one_in_progress ON user_programs (user_id) WHERE state = 'in_progress';
CREATE INDEX idx_user_programs_user ON user_programs (user_id, updated_at DESC);

CREATE TABLE user_program_exercises (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    program_id UUID NOT NULL REFERENCES user_programs(id) ON DELETE CASCADE,
    week_number INT NOT NULL CHECK (week_number >= 1),
    day_of_week SMALLINT NOT NULL CHECK (day_of_week BETWEEN 1 AND 7),
    exercise_id UUID NOT NULL REFERENCES exercises(id),
    order_index INT NOT NULL,
    sets INT NOT NULL CHECK (sets BETWEEN 1 AND 20),
    target_reps INT NOT NULL CHECK (target_reps >= 1),
    target_weight_kg NUMERIC(6, 2) CHECK (target_weight_kg >= 0),
    target_percent_1rm NUMERIC(5, 2) CHECK (target_percent_1rm > 0 AND target_percent_1rm <= 100),
    rest_seconds INT CHECK (rest_seconds >= 0),
    UNIQUE (program_id, week_number, day_of_week, order_index),
    CHECK (target_weight_kg IS NULL OR target_percent_1rm IS NULL)
);
