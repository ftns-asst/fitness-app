
FROM golang:1.26-alpine AS build

WORKDIR /build

COPY go.mod ./go.mod
COPY go.sum ./go.sum

RUN go mod download -x

COPY . .

RUN go build -o /build/ftns-backend ./cmd/main.go


# light weight image for running
FROM alpine
WORKDIR /app

COPY --from=build /build/ftns-backend . 
ENTRYPOINT ["./ftns-backend"]
