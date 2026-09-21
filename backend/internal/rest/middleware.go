package rest

import (
	"context"
	"errors"
	"ftns-asst/internal/model"
	"ftns-asst/internal/util"
	"strings"

	"github.com/gin-gonic/gin"
)

func AuthMiddleware(secretKey string) gin.HandlerFunc {
	return func(c *gin.Context) {
		authHeader := c.GetHeader("Authorization")
		token := strings.TrimPrefix(authHeader, "Bearer ")

		tokenInfo, err := util.ParseJWTToken(token, secretKey)
		if errors.Is(err, util.ErrTokenExpired) {
			c.Abort()
			handleError(c, model.AuthErrorTokenExpired())
			return
		}
		if err != nil {
			c.Abort()
			handleError(c, model.AuthErrorInvalidToken())
			return
		}

		c.Request = c.Request.WithContext(context.WithValue(c.Request.Context(), model.ContextKeyUserID, tokenInfo.UserID))

		c.Next()
	}
}
