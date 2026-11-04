--
CREATE TABLE workout_templates (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name VARCHAR(255) NOT NULL,
    focus TEXT NOT NULL,
    level TEXT NOT NULL,
    equipment TEXT[] NOT NULL DEFAULT '{}',
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE workout_template_exercises (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    workout_template_id UUID NOT NULL REFERENCES workout_templates(id) ON DELETE CASCADE,
    exercise_id UUID NOT NULL REFERENCES exercises(id),
    order_index INT NOT NULL,
    sets INT NOT NULL CHECK (sets BETWEEN 1 AND 20),
    target_reps INT NOT NULL CHECK (target_reps >= 1),
    rest_seconds INT CHECK (rest_seconds >= 0),
    UNIQUE (workout_template_id, order_index)
);

CREATE TABLE default_programs (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name VARCHAR(255) NOT NULL,
    description TEXT,
    goal TEXT NOT NULL,
    level TEXT NOT NULL,
    days_per_week INT NOT NULL CHECK (days_per_week BETWEEN 1 AND 7),
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TABLE default_program_days (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    default_program_id UUID NOT NULL REFERENCES default_programs(id) ON DELETE CASCADE,
    order_index INT NOT NULL,
    workout_template_id UUID NOT NULL REFERENCES workout_templates(id),
    UNIQUE (default_program_id, order_index)
);
