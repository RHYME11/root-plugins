.PHONY: all unix test install clean

GROOT_SOURCE_DIR ?= $(abspath ../groot)
GROOT_BUILD_DIR ?= $(GROOT_SOURCE_DIR)/build
INSTALL_PREFIX ?= $(HOME)/.local

all: unix

unix: CMakeLists.txt
	@if [ ! -f "$(GROOT_SOURCE_DIR)/include/Plugin/GPlugin.h" ]; then \
		echo "Groot source tree not found: $(GROOT_SOURCE_DIR)"; \
		exit 1; \
	fi
	@if [ ! -f "$(GROOT_BUILD_DIR)/lib/libGPLUGIN.dylib" ] && \
	   [ ! -f "$(GROOT_BUILD_DIR)/lib/libGPLUGIN.so" ]; then \
		echo "Groot Plugin library not found under: $(GROOT_BUILD_DIR)/lib"; \
		echo "Build Groot with 'make' before building root-plugins."; \
		exit 1; \
	fi
	@cmake -S ./ -B ./build \
		-DBUILD_TESTING=OFF \
		-DGROOT_SOURCE_DIR="$(GROOT_SOURCE_DIR)" \
		-DGROOT_BUILD_DIR="$(GROOT_BUILD_DIR)" || \
		cmake3 -S ./ -B ./build \
			-DBUILD_TESTING=OFF \
			-DGROOT_SOURCE_DIR="$(GROOT_SOURCE_DIR)" \
			-DGROOT_BUILD_DIR="$(GROOT_BUILD_DIR)"
	@cmake --build ./build -j4

test:
	@cmake -S ./ -B ./build \
		-DBUILD_TESTING=ON \
		-DGROOT_SOURCE_DIR="$(GROOT_SOURCE_DIR)" \
		-DGROOT_BUILD_DIR="$(GROOT_BUILD_DIR)" || \
		cmake3 -S ./ -B ./build \
			-DBUILD_TESTING=ON \
			-DGROOT_SOURCE_DIR="$(GROOT_SOURCE_DIR)" \
			-DGROOT_BUILD_DIR="$(GROOT_BUILD_DIR)"
	@cmake --build ./build -j4
	@ctest --test-dir ./build --output-on-failure

install: unix
	@cmake --install ./build --prefix "$(INSTALL_PREFIX)"

clean:
	@if [ -d "./build" ]; then rm -rf ./build; fi
	@if [ -d "./build-photopeak" ]; then rm -rf ./build-photopeak; fi
