
FROM golang:1.26-alpine AS build

WORKDIR /build

COPY go.mod ./go.mod
COPY go.sum ./go.sum

RUN go mod download -x

COPY . .

RUN go build -o /build/ftns-backend ./cmd/main.go

FROM golang:1.26-alpine AS dlv
RUN CGO_ENABLED=0 go install github.com/go-delve/delve/cmd/dlv@v1.27.0

# light weight image for running
FROM alpine as run
WORKDIR /app

COPY --from=build /build/ftns-backend .
COPY --from=data . .
ENTRYPOINT ["./ftns-backend"]


# run in debug mode
FROM alpine as run-debug
WORKDIR /app

COPY --from=build /build/ftns-backend .
COPY --from=data . .
COPY --from=dlv /go/bin/dlv .
ENTRYPOINT ["./dlv", "exec", "./ftns-backend", "--headless", "--listen=:40021", "--api-version=2", "--accept-multiclient"]
