.PHONY: all unix test install clean

GROOT_PLUGIN_SDK ?= $(abspath ../groot/build/plugin-sdk)
INSTALL_PREFIX ?= $(HOME)/.local

all: unix

unix: CMakeLists.txt
	@if [ ! -f "$(GROOT_PLUGIN_SDK)/lib/cmake/GrootPlugin/GrootPluginConfig.cmake" ]; then \
		echo "Groot Plugin SDK not found: $(GROOT_PLUGIN_SDK)"; \
		echo "Build Groot with 'make' before building root-plugins."; \
		exit 1; \
	fi
	@cmake -S ./ -B ./build \
		-DBUILD_TESTING=OFF \
		-DCMAKE_PREFIX_PATH="$(GROOT_PLUGIN_SDK)" || \
		cmake3 -S ./ -B ./build \
			-DBUILD_TESTING=OFF \
			-DCMAKE_PREFIX_PATH="$(GROOT_PLUGIN_SDK)"
	@cmake --build ./build -j4

test:
	@cmake -S ./ -B ./build \
		-DBUILD_TESTING=ON \
		-DCMAKE_PREFIX_PATH="$(GROOT_PLUGIN_SDK)" || \
		cmake3 -S ./ -B ./build \
			-DBUILD_TESTING=ON \
			-DCMAKE_PREFIX_PATH="$(GROOT_PLUGIN_SDK)"
	@cmake --build ./build -j4
	@ctest --test-dir ./build --output-on-failure

install: unix
	@cmake --install ./build --prefix "$(INSTALL_PREFIX)"

clean:
	@if [ -d "./build" ]; then rm -rf ./build; fi
	@if [ -d "./build-photopeak" ]; then rm -rf ./build-photopeak; fi
