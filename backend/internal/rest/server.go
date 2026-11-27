package rest

import (
	"fmt"
	"ftns-asst/internal/config"
	"ftns-asst/internal/service"
	"ftns-asst/static"
	"io/fs"
	"log"
	"net/http"

	_ "ftns-asst/api"

	"github.com/gin-contrib/cors"
	"github.com/gin-gonic/gin"
	swaggerFiles "github.com/swaggo/files"
	ginSwagger "github.com/swaggo/gin-swagger"
)

func NewServer(cfg *config.Config, services service.Services) *http.Server {
	r := gin.New()
	r.Use(gin.Logger())
	r.Use(gin.Recovery())

	r.Use(cors.New(cors.Config{
		AllowOrigins:     []string{"*"},
		AllowMethods:     []string{"GET", "POST", "PUT", "PATCH", "DELETE", "OPTIONS"},
		AllowHeaders:     []string{"Origin", "Content-Type", "Authorization"},
		AllowCredentials: false,
	}))
	handlers := newHandlers(cfg, &services)

	staticUI, err := fs.Sub(static.StaticFiles, "files")
	if err != nil {
		log.Fatal(err)
	}
	fileServer := gin.WrapH(http.StripPrefix("/admin", http.FileServer(http.FS(staticUI))))

	r.GET("/admin/*filepath", fileServer)

	api := r.Group("/api/v1")
	handlers.RegisterRoutes(api)

	api.GET("/swagger/*any", func(c *gin.Context) {
		if p := c.Param("any"); p == "" || p == "/" {
			c.Redirect(http.StatusFound, c.Request.URL.Path+"/index.html")
			return
		}
		ginSwagger.WrapHandler(swaggerFiles.Handler, ginSwagger.URL("doc.json"))(c)
	})
	api.GET("/", func(c *gin.Context) {
		c.Redirect(http.StatusFound, c.Request.URL.Path+"swagger/index.html")
	})

	server := http.Server{
		Addr:    fmt.Sprintf(":%s", cfg.HTTPServerPort),
		Handler: r.Handler(),
	}
	log.Println(server.Addr)
	return &server
}
