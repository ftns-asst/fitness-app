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
		})

	tokenString, err := token.SignedString(secret)
	if err != nil {
		return nil, err
	}

	return &model.TokenInfo{Token: tokenString, UserID: userID, ExpiresAt: expiresAt}, nil
}

var (
	ErrTokenExpired = errors.New("jwt token expired")
)

func ParseJWTToken(tokenString string, secret string) (*model.TokenInfo, error) {
	token, err := jwt.ParseWithClaims(tokenString,
		&UserJWTClaims{},
		func(token *jwt.Token) (interface{}, error) {
			if _, ok := token.Method.(*jwt.SigningMethodHMAC); !ok {
				return nil, fmt.Errorf("unexpected signing method: %v", token.Header["alg"])
			}
			return []byte(secret), nil
		},
		jwt.WithExpirationRequired(),
		jwt.WithLeeway(30*time.Second),
	)

	if err != nil {
		if errors.Is(err, jwt.ErrTokenExpired) {
			return nil, ErrTokenExpired
		}
		return nil, fmt.Errorf("invalid JWT: %w", err)
	}

	if claims, ok := token.Claims.(jwt.MapClaims); ok && token.Valid {
		strId, ok := claims["userID"].(string)
		fmt.Println(strId)
		if !ok {
			return nil, fmt.Errorf("invalid token")
		}
		userId, err := uuid.Parse(strId)
		if err != nil {
			return nil, fmt.Errorf("invalid userId in token")
		}

		expiration, err := claims.GetExpirationTime()
		if err != nil {
			return nil, fmt.Errorf("failed get expiration time")
		}
		return &model.TokenInfo{
			Token:     tokenString,
			UserID:    userId,
			ExpiresAt: expiration.Time,
		}, nil
	}
	return nil, fmt.Errorf("invalid token")
}
