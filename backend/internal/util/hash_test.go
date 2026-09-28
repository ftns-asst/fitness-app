package util

import (
	"testing"

	"github.com/go-openapi/testify/v2/require"
)

type testData struct {
	Name               string
	InputValue         string
	TargetHash         string
	IsMatchingExpected bool
	IsErrorExpected    bool
	Secret             *string
}

func TestHashArgon2(t *testing.T) {
	target := "test12345"
	targetHash, err := Hash(target)
	require.NoError(t, err)
	tests := []testData{
		{
			Name:               "correct value",
			InputValue:         target,
			TargetHash:         targetHash,
			IsMatchingExpected: true,
		},
		{
			Name:       "incorrect value",
			InputValue: "incorrectValue",
			TargetHash: targetHash,
		},
		{
			Name:            "incorrect target hash",
			InputValue:      "someValue",
			TargetHash:      "invalidHash",
			IsErrorExpected: true,
		},
	}
	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			ok, err := CompareHash(test.InputValue, test.TargetHash)
			if test.IsErrorExpected {
				require.Error(t, err)
			} else {
				require.NoError(t, err)
			}
			require.Equal(t, test.IsMatchingExpected, ok)
		})
	}
}

func TestHashSHA256(t *testing.T) {
	target := "test98765"
	secret := "someSecret3843985"
	targetHash, err := HashSHA256(target, secret)
	require.NoError(t, err)
	tests := []testData{
		{
			Name:               "success: correct value, correct secret",
			InputValue:         target,
			TargetHash:         targetHash,
			Secret:             Ptr(secret),
			IsMatchingExpected: true,
		},
		{
			Name:       "incorrect value, correct secret",
			InputValue: "incorrectValue",
			TargetHash: targetHash,
			Secret:     Ptr(secret),
		},
		{
			Name:       "correct value, incorrect secret",
			InputValue: "incorrectValue",
			TargetHash: targetHash,
			Secret:     Ptr(secret),
		},
		{
			Name:       "incorrect value, incorrect secret",
			InputValue: "incorrectValue",
			TargetHash: targetHash,
			Secret:     Ptr("incorrectSecret"),
		},
		{
			Name:            "invalid target hash",
			InputValue:      target,
			TargetHash:      "invalidHash*/- ",
			Secret:          Ptr(secret),
			IsErrorExpected: true,
		},
	}
	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			ok, err := CompareSHA256(test.InputValue, test.TargetHash, *test.Secret)
			if test.IsErrorExpected {
				require.Error(t, err)
			} else {
				require.NoError(t, err)
			}
			require.Equal(t, test.IsMatchingExpected, ok)
		})
	}
}
