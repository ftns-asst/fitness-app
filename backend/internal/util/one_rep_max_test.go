package util

import (
	"math"
	"testing"

	"github.com/stretchr/testify/require"
)

func TestEstimateOneRepMax(t *testing.T) {
	tests := []struct {
		name     string
		weightKg float64
		reps     int
		expected float64
	}{
		{name: "one rep is the max itself", weightKg: 100, reps: 1, expected: 100},
		{name: "five reps rounded to two digits", weightKg: 100, reps: 5, expected: 116.67},
		{name: "ten reps", weightKg: 60, reps: 10, expected: 80},
		{name: "fractional weight", weightKg: 52.5, reps: 3, expected: 57.75},
		{name: "thirty reps doubles the weight", weightKg: 100, reps: 30, expected: 200},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			res, err := EstimateOneRepMax(tt.weightKg, tt.reps)
			require.NoError(t, err)
			require.InDelta(t, tt.expected, res, 1e-9)
		})
	}
}

func TestEstimateOneRepMaxInvalidInput(t *testing.T) {
	tests := []struct {
		name     string
		weightKg float64
		reps     int
	}{
		{name: "zero weight", weightKg: 0, reps: 5},
		{name: "negative weight", weightKg: -10, reps: 5},
		{name: "NaN weight", weightKg: math.NaN(), reps: 5},
		{name: "infinite weight", weightKg: math.Inf(1), reps: 5},
		{name: "zero reps", weightKg: 100, reps: 0},
		{name: "negative reps", weightKg: 100, reps: -1},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			_, err := EstimateOneRepMax(tt.weightKg, tt.reps)
			require.Error(t, err)
		})
	}
}
