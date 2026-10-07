package model

import (
	"ftns-asst/internal/util"
	"strings"
	"testing"

	"github.com/go-openapi/testify/v2/require"
)

func TestAuthInputValidation(t *testing.T) {
	t.Parallel()
	validEmail := "test@example.com"
	validPassword := "testPassword"
	tests := []struct {
		Name          string
		Email         string
		Password      string
		ExpectedError error
	}{
		{
			Name:          "error blank email",
			Email:         "     ",
			Password:      validPassword,
			ExpectedError: ErrBadRequest,
		},
		{
			Name:          "error blank password",
			Email:         validEmail,
			Password:      "    ",
			ExpectedError: ErrBadRequest,
		},
		{
			Name:          "error password <8 chars",
			Email:         validEmail,
			Password:      "3494",
			ExpectedError: ErrBadRequest,
		},
		{
			Name:          "error password >64 chars",
			Email:         validEmail,
			Password:      strings.Repeat("t", 65),
			ExpectedError: ErrBadRequest,
		},
		{
			Name:          "error password with invalid chars",
			Email:         validEmail,
			Password:      "абв        ",
			ExpectedError: ErrBadRequest,
		},
		{
			Name:          "good email and password",
			Email:         validEmail,
			Password:      validPassword,
			ExpectedError: nil,
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			t.Parallel()
			err := util.Ptr(AuthInput{
				Email:    test.Email,
				Password: test.Password,
			}).Validate()

			require.ErrorIs(t, err, test.ExpectedError)
		})
	}
}

func TestSingUpInputValidation(t *testing.T) {
	t.Parallel()
	validAuthInput := AuthInput{
		Email:    "test@example.com",
		Password: "testPassword",
	}
	tests := []struct {
		Name          string
		Username      string
		ExpectedError error
	}{
		{
			Name:          "good username",
			Username:      "someUsername",
			ExpectedError: nil,
		},
		{
			Name:          "error blank username",
			Username:      "    ",
			ExpectedError: ErrBadRequest,
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			t.Parallel()
			err := util.Ptr(SignUpInput{
				AuthInput: validAuthInput,
				Name:      test.Username,
			}).Validate()

			require.ErrorIs(t, err, test.ExpectedError)
		})
	}
}
