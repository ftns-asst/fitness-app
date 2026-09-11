.PHONY: server-run server-up server-down migrate-create migrate-force lint-run

include ./.env

server-run:
	go run cmd/main.go

server-up:
	docker compose --env-file ./.env -f ./docker/docker-compose.yaml -p ftns-backend up -d --build ftns-backend

server-down:
	docker compose --env-file ./.env -f ./docker/docker-compose.yaml -p ftns-backend down

migrate-create: 
ifndef MIGRATE_NAME
	echo $(error migrate name is required, use `make migrate-create MIGRATE_NAME=your_migration_name`)
else
	migrate create -ext sql -dir internal/repository/migrations -seq ${MIGRATE_NAME}
endif

migrate-force:
ifndef MIGRATE_VERSION
	echo $(error migrate version is required, use `make migrate-force MIGRATE_VERSION=your_migration_version`)
else
	migrate -database postgres://${POSTGRES_USER}:${POSTGRES_PASSWORD}@${POSTGRES_HOST}:${POSTGRES_PORT}/${POSTGRES_DB}?sslmode=disable -path internal/repository/migrations force ${MIGRATE_VERSION}
endif


lint-run:
	docker run -t --rm -v .:/app -w /app/ golangci/golangci-lint:v2.12.2 \
		golangci-lint run --no-config -E govet,staticcheck