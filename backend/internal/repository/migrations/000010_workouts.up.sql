CREATE TABLE workouts (
    id UUID PRIMARY KEY,
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    program_id UUID REFERENCES user_programs(id) ON DELETE SET NULL,
    week_number INT CHECK (week_number BETWEEN 1 AND 52),
    day_of_week SMALLINT CHECK (day_of_week BETWEEN 1 AND 7),
    started_at TIMESTAMPTZ NOT NULL,
    completed_at TIMESTAMPTZ NOT NULL CHECK (completed_at >= started_at),
    request_hash BYTEA NOT NULL,
    CHECK ((week_number IS NULL) = (day_of_week IS NULL)),
    CHECK (program_id IS NULL OR week_number IS NOT NULL)
);

CREATE INDEX idx_workouts_history ON workouts (user_id, completed_at DESC, id DESC);

CREATE TABLE workout_exercises (
    id UUID PRIMARY KEY,
    workout_id UUID NOT NULL REFERENCES workouts(id) ON DELETE CASCADE,
    exercise_id UUID NOT NULL REFERENCES exercises(id),
    order_index INT NOT NULL CHECK (order_index >= 0),
    target_sets INT CHECK (target_sets BETWEEN 1 AND 20),
    target_reps INT CHECK (target_reps > 0),
    target_weight_kg NUMERIC(6,2) CHECK (target_weight_kg >= 0),
    UNIQUE (workout_id, order_index)
);

CREATE TABLE workout_sets (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    workout_exercise_id UUID NOT NULL REFERENCES workout_exercises(id) ON DELETE CASCADE,
    set_number INT NOT NULL CHECK (set_number > 0),
    reps INT NOT NULL CHECK (reps > 0),
    weight_kg NUMERIC(6,2) CHECK (weight_kg >= 0),
    UNIQUE (workout_exercise_id, set_number)
);

ALTER TABLE personal_records ADD COLUMN source_workout_set_id UUID UNIQUE
    REFERENCES workout_sets(id) ON DELETE SET NULL;
