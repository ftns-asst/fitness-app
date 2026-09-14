package util

import (
	"crypto/hmac"
	"crypto/sha256"
	"encoding/base64"
	"encoding/hex"
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

func HashSHA256(value, secret string) (string, error) {
	hash := hmac.New(sha256.New, []byte(secret))
	_, err := hash.Write([]byte(value))
	if err != nil {
		return "", fmt.Errorf("failed to hash: %w", err)
	}
	sha := hash.Sum(nil)

	// Кодируем результат в hex-строку для удобства передачи
	return base64.RawStdEncoding.EncodeToString(sha), nil
}

func CompareSHA256(value, targetHash, secret string) (bool, error) {
	// Вычисляем правильную подпись для текущего сообщения
	valueHash, err := HashSHA256(value, secret)
	if err != nil {
		return false, err
	}

	expectedBytes, err1 := hex.DecodeString(targetHash)
	receivedBytes, err2 := hex.DecodeString(valueHash)
	if err1 != nil || err2 != nil {
		return false, fmt.Errorf("failed to decode hash string")
	}

	return hmac.Equal(expectedBytes, receivedBytes), nil
}
