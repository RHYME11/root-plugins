.PHONY: all unix install clean

BUILD_DIR ?= build
CMAKE ?= cmake
JOBS ?= 4
INSTALL_PREFIX ?= $(HOME)/.local

all: unix

unix: CMakeLists.txt
	@$(CMAKE) -S . -B "$(BUILD_DIR)"
	@$(CMAKE) --build "$(BUILD_DIR)" -j$(JOBS)

install: unix
	@$(CMAKE) --install "$(BUILD_DIR)" --prefix "$(INSTALL_PREFIX)"

clean:
	@if [ -d "$(BUILD_DIR)" ]; then $(CMAKE) --build "$(BUILD_DIR)" --target clean; fi
