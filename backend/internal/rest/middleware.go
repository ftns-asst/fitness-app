package rest

import (
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
		if err != nil {
			handleError(c, err)
		}

		c.Set(model.ContextKeyUserID, tokenInfo.UserID)
	}
}
