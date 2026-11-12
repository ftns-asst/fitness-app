package util

import (
	"errors"
	"math"
)

// EstimateOneRepMax estimates one-rep max by the Epley formula: weight * (1 + reps / 30).
// One rep is the max itself. The estimate is less accurate when reps grow.
func EstimateOneRepMax(weightKg float64, reps int) (float64, error) {
	if math.IsNaN(weightKg) || math.IsInf(weightKg, 0) || weightKg <= 0 {
		return 0, errors.New("weight must be a positive number")
	}
	if reps < 1 {
		return 0, errors.New("reps must be at least 1")
	}
	if reps == 1 {
		return weightKg, nil
	}

	return math.Round(weightKg*(1+float64(reps)/30)*100) / 100, nil
}
