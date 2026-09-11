
server-run:
	go run cmd/main.go

server-up:
	docker compose --env-file ./.env -f ./docker/docker-compose.yaml -p ftns-backend up -d --build ftns-backend

server-down:
	docker compose --env-file ./.env -f ./docker/docker-compose.yaml -p ftns-backend down
