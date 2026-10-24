//go:build integration

package integration

import (
	"bytes"
	"context"
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"ftns-asst/internal/config"
	"ftns-asst/internal/repository"
	"ftns-asst/internal/rest"
	"ftns-asst/internal/service"
	"io"
	"log"
	"net/http"
	"net/http/httptest"
	"os"
	"regexp"
	"sync"
	"testing"
	"time"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"github.com/jackc/pgx/v5/pgxpool"
	"github.com/stretchr/testify/require"
)

var (
	db     *pgxpool.Pool
	repos  *repository.Repositories
	mailer = newMailer()
	server *httptest.Server
)

func TestMain(m *testing.M) {
	os.Exit(run(m))
}

// creates a separate database for the run, drops it after
func run(m *testing.M) int {
	ctx := context.Background()
	gin.SetMode(gin.TestMode)

	pgCfg := &config.PostgresConfig{
		Host:     getEnv("TEST_POSTGRES_HOST", "localhost"),
		Port:     getEnv("TEST_POSTGRES_PORT", "5435"),
		User:     getEnv("TEST_POSTGRES_USER", "postgres"),
		Password: getEnv("TEST_POSTGRES_PASSWORD", "postgres"),
		DBName:   getEnv("TEST_POSTGRES_DB", "postgres"),
		SSLMode:  "disable",
	}

	admin, err := pgxpool.New(ctx, pgCfg.ConnString())
	if err == nil {
		err = admin.Ping(ctx)
	}
	if err != nil {
		log.Printf("postgres is not available (start it with `make test-integration-db`): %s", err)
		return 1
	}
	defer admin.Close()

	suffix := make([]byte, 4)
	_, _ = rand.Read(suffix)
	dbName := "ftns_it_" + hex.EncodeToString(suffix)
	if _, err := admin.Exec(ctx, "CREATE DATABASE "+dbName); err != nil {
		log.Printf("failed to create test database: %s", err)
		return 1
	}
	defer func() {
		if _, err := admin.Exec(ctx, "DROP DATABASE "+dbName+" WITH (FORCE)"); err != nil {
			log.Printf("failed to drop test database %s: %s", dbName, err)
		}
	}()

	pgCfg.DBName = dbName
	db, err = repository.ConnectToDB(ctx, pgCfg)
	if err != nil {
		log.Printf("failed to connect to test database: %s", err)
		return 1
	}
	// db.Close is not called: sql.DB from runMigrations keeps pool connections acquired
	// and Close would wait for them forever, DROP DATABASE WITH (FORCE) terminates them

	cfg := &config.Config{
		AccessTokenJWTSecretKey:       "access-secret",
		RefreshTokenJWTSecretKey:      "refresh-secret",
		RefreshTokenHashSecretKey:     "refresh-hash-secret",
		VerificationCodeHashSecretKey: "code-hash-secret",
		ResetTokenHashSecretKey:       "reset-hash-secret",
	}
	repos = repository.NewRepositories(db)
	services := service.NewServices(cfg, *repos, mailer, db)
	server = httptest.NewServer(rest.NewServer(cfg, *services).Handler)
	defer server.Close()

	return m.Run()
}

func getEnv(key, def string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return def
}

// collects sent emails by recipient, emails are sent asynchronously
type testMailer struct {
	mu    sync.Mutex
	boxes map[string]chan string
}

func newMailer() *testMailer {
	return &testMailer{boxes: map[string]chan string{}}
}

func (m *testMailer) box(email string) chan string {
	m.mu.Lock()
	defer m.mu.Unlock()
	if _, ok := m.boxes[email]; !ok {
		m.boxes[email] = make(chan string, 10)
	}
	return m.boxes[email]
}

func (m *testMailer) SendEmail(toEmail string, subjectRaw string, body string) error {
	m.box(toEmail) <- body
	return nil
}

var codeRe = regexp.MustCompile(`\d{6}`)

func (m *testMailer) waitCode(t *testing.T, email string) string {
	t.Helper()
	select {
	case body := <-m.box(email):
		code := codeRe.FindString(body)
		require.NotEmpty(t, code, body)
		return code
	case <-time.After(5 * time.Second):
		t.Fatalf("no email for %s", email)
		return ""
	}
}

func newEmail() string {
	return fmt.Sprintf("user-%s@example.com", uuid.NewString()[:8])
}

type client struct {
	t     *testing.T
	token string
}

func newClient(t *testing.T) *client {
	return &client{t: t}
}

// sends json request, decodes response into out on 2xx or returns error response
func (c *client) do(method, path string, body any, out any) (int, rest.ErrorResponse) {
	c.t.Helper()
	var reader io.Reader
	if body != nil {
		data, err := json.Marshal(body)
		require.NoError(c.t, err)
		reader = bytes.NewReader(data)
	}

	req, err := http.NewRequest(method, server.URL+"/api/v1"+path, reader)
	require.NoError(c.t, err)
	req.Header.Set("Content-Type", "application/json")
	if c.token != "" {
		req.Header.Set("Authorization", "Bearer "+c.token)
	}

	resp, err := server.Client().Do(req)
	require.NoError(c.t, err)
	defer resp.Body.Close()
	data, err := io.ReadAll(resp.Body)
	require.NoError(c.t, err)

	var errResp rest.ErrorResponse
	if resp.StatusCode >= 300 {
		require.NoError(c.t, json.Unmarshal(data, &errResp), string(data))
		return resp.StatusCode, errResp
	}
	if out != nil {
		require.NoError(c.t, json.Unmarshal(data, out), string(data))
	}
	return resp.StatusCode, errResp
}

func (c *client) requireOK(method, path string, body any, out any) {
	c.t.Helper()
	code, errResp := c.do(method, path, body, out)
	require.Equal(c.t, http.StatusOK, code, "%s %s: %+v", method, path, errResp)
}

func (c *client) requireError(method, path string, body any, code int, key string) {
	c.t.Helper()
	gotCode, errResp := c.do(method, path, body, nil)
	require.Equal(c.t, code, gotCode, "%s %s: %+v", method, path, errResp)
	require.Equal(c.t, key, errResp.Key, "%s %s: %+v", method, path, errResp)
}
