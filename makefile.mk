# Makefile for Notification Player

APP_NAME = notification-player
VERSION = $(shell git describe --tags --always --dirty 2>/dev/null || echo "dev")
BUILD_TIME = $(shell date -u '+%Y-%m-%d_%H:%M:%S')
GIT_COMMIT = $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")
LDFLAGS = -ldflags "-X main.Version=${VERSION} -X main.BuildTime=${BUILD_TIME} -X main.GitCommit=${GIT_COMMIT}"

.PHONY: all build clean install deb test run

all: build

build:
	@echo "Building ${APP_NAME}..."
	@go build ${LDFLAGS} -o bin/${APP_NAME} cmd/notification-player/main.go

clean:
	@echo "Cleaning..."
	@rm -rf bin/ dist/ *.deb
	@go clean

test:
	@echo "Running tests..."
	@go test -v ./...

run: build
	@./bin/${APP_NAME} -config config.yaml

install: build
	@echo "Installing to /usr/local/bin..."
	@sudo cp bin/${APP_NAME} /usr/local/bin/
	@sudo mkdir -p /etc/notification-player
	@sudo cp config.yaml /etc/notification-player/

deb: build
	@echo "Building deb package..."
	@./scripts/package-deb.sh ${VERSION}

dist: clean build deb
	@echo "Distribution packages built in dist/"

help:
	@echo "Available targets:"
	@echo "  build   - Build the application"
	@echo "  clean   - Clean build artifacts"
	@echo "  test    - Run tests"
	@echo "  run     - Build and run"
	@echo "  install - Install to system"
	@echo "  deb     - Build deb package"
	@echo "  dist    - Build all distribution packages"