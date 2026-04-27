# Wrapper de conveniencia sobre CMake.
# Para build directo con CMake usar:
#   cmake -B build && cmake --build build

BUILD_DIR ?= build
BUILD_TYPE ?= Release
JOBS ?= $(shell nproc 2>/dev/null || echo 4)

.PHONY: all configure build run repl test test-unit test-integracion clean install \
        debug release stress format help

all: build

help:
	@echo "Cornamusa — targets disponibles:"
	@echo "  make build          — compila (Release por defecto)"
	@echo "  make debug          — compila en modo Debug con símbolos"
	@echo "  make release        — compila en modo Release optimizado"
	@echo "  make stress         — compila con GC_STRESS activado"
	@echo "  make run ARGS=...   — ejecuta cornamusa"
	@echo "  make repl           — abre el REPL interactivo"
	@echo "  make test           — corre la suite completa de tests"
	@echo "  make test-unit      — solo tests unitarios C"
	@echo "  make test-integracion — solo tests de programas .cor"
	@echo "  make install        — instala en CMAKE_INSTALL_PREFIX"
	@echo "  make clean          — borra el directorio de build"

configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

build: configure
	cmake --build $(BUILD_DIR) -j $(JOBS)

debug:
	$(MAKE) build BUILD_TYPE=Debug

release:
	$(MAKE) build BUILD_TYPE=Release

stress:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug -DCORNAMUSA_GC_STRESS=ON
	cmake --build $(BUILD_DIR) -j $(JOBS)

run: build
	$(BUILD_DIR)/cornamusa $(ARGS)

repl: build
	$(BUILD_DIR)/cornamusa

test: build
	cd $(BUILD_DIR) && ctest --output-on-failure

test-unit: build
	cd $(BUILD_DIR) && ctest --output-on-failure -L unit

test-integracion: build
	@bash tests/integracion/correr_tests.sh $(BUILD_DIR)/cornamusa

install: build
	cmake --install $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)
