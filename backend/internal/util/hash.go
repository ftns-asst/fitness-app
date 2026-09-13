package util

import (
	"fmt"

	"github.com/alexedwards/argon2id"
)

func Hash(value string) (hashString string, err error) {
	hash, err := argon2id.CreateHash(value, argon2id.DefaultParams)
	if err != nil {
		return "", fmt.Errorf("failed to hash: %w", err)
	}
	return hash, nil
}

func CompareHash(valueRaw, targetHash string) (bool, error) {
	matches, err := argon2id.ComparePasswordAndHash(valueRaw, targetHash)
	if err != nil {
		return false, fmt.Errorf("failed to compare hash: %w", err)
	}
	return matches, nil
}
