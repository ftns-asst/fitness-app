# fitness-app

Мобильное приложение - фитнес помощник для удобного ведения записей тренировок и отслеживания прогресса.

## Технологический стек

__Клиент__ - мобильное приложение на Qt/QML(?)  
__Бэкенд__ - на Golang, Gin фреймворк для REST API, база данных PostgreSQL, развертывание в Docker.

## Backend

### Backend Quick Start

#### Run server in docker

```bash
# clone the repo
git clone https://github.com/ftns-asst/fitness-app

# go to backend folder
cd fitness-app/backend

# run server (including db)
make server-run
```

#### Lint

```bash
# run linter
make lint-run
```

#### Tests

```bash
# run tests
make test-run
```

#### Generate swagger API documentation

```bash
# generate swagger docs
make swagger-gen
```

### Swagger API UI

Go to <http://127.0.0.1:8181/api/v1/swagger>
