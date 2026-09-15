package util

import (
	"errors"
	"fmt"
	"ftns-asst/internal/model"
	"time"

	"github.com/golang-jwt/jwt/v5"
	"github.com/google/uuid"
)

type UserJWTClaims struct {
	jwt.RegisteredClaims
	UserID uuid.UUID `json:"userID"`
}

func GenAccessToken(userID uuid.UUID, secret string) (*model.TokenInfo, error) {
	return genToken(userID, secret, time.Now().Add(model.AccessTokenExpireTime))
}

func GenRefreshToken(userID uuid.UUID, secret string) (*model.TokenInfo, error) {
	return genToken(userID, secret, time.Now().Add(model.RefreshTokenExpireTime))
}

func genToken(userID uuid.UUID, secret string, expiresAt time.Time) (*model.TokenInfo, error) {
	if userID == uuid.Nil {
		return nil, fmt.Errorf("failed to gen token, user ID is nil")
	}
	token := jwt.NewWithClaims(jwt.SigningMethodHS256,
		UserJWTClaims{
			UserID: userID,
			RegisteredClaims: jwt.RegisteredClaims{
				ExpiresAt: jwt.NewNumericDate(expiresAt),
			},
		},
	)

	tokenString, err := token.SignedString([]byte(secret))
	if err != nil {
		return nil, err
	}

	return &model.TokenInfo{Token: tokenString, UserID: userID, ExpiresAt: expiresAt}, nil
}

var (
	ErrTokenExpired = errors.New("jwt token expired")
)

func ParseJWTToken(tokenString string, secret string) (*model.TokenInfo, error) {
	claims := &UserJWTClaims{}

	token, err := jwt.ParseWithClaims(tokenString, claims,
		func(token *jwt.Token) (any, error) {
			return []byte(secret), nil
		},
		jwt.WithValidMethods([]string{jwt.SigningMethodHS256.Alg()}),
		jwt.WithExpirationRequired(),
		jwt.WithLeeway(30*time.Second),
	)

	if err != nil {
		if errors.Is(err, jwt.ErrTokenExpired) {
			return nil, ErrTokenExpired
		}
		return nil, fmt.Errorf("invalid JWT: %w", err)
	}
	if !token.Valid || claims.UserID == uuid.Nil {
		return nil, errors.New("invalid token")
	}

	return &model.TokenInfo{
		Token:     tokenString,
		UserID:    claims.UserID,
		ExpiresAt: claims.ExpiresAt.Time,
	}, nil
}
