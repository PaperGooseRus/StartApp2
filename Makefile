CXX ?= g++
CPPFLAGS += -Iinclude
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic

.PHONY: all test clean
ENGINE_SOURCES = src/world.cpp src/snapshot.cpp

all: build/demo build/tests build/snapshot-tests
build:
	mkdir -p build
build/demo: $(ENGINE_SOURCES) include/pixel/world.hpp examples/demo.cpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ENGINE_SOURCES) examples/demo.cpp -o $@
build/tests: $(ENGINE_SOURCES) include/pixel/world.hpp tests/world_test.cpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ENGINE_SOURCES) tests/world_test.cpp -o $@
build/snapshot-tests: $(ENGINE_SOURCES) include/pixel/world.hpp tests/snapshot_test.cpp game/storage.cpp game/storage.hpp game/sandbox.hpp | build
	$(CXX) $(CPPFLAGS) -Igame $(CXXFLAGS) $(ENGINE_SOURCES) game/storage.cpp tests/snapshot_test.cpp -o $@
test: build/tests build/snapshot-tests
	./build/tests
	./build/snapshot-tests
clean:
	rm -rf build

# SDL2 is required only for the graphical sandbox.
SDL_CFLAGS ?= $(shell pkg-config --cflags sdl2)
SDL_LIBS ?= $(shell pkg-config --libs sdl2)
.PHONY: game game-test
game: build/game
build/game: $(ENGINE_SOURCES) include/pixel/world.hpp game/main.cpp game/sandbox.hpp game/storage.cpp game/storage.hpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SDL_CFLAGS) $(ENGINE_SOURCES) game/main.cpp game/storage.cpp -o $@ $(SDL_LIBS)
game-test: build/game
	SDL_VIDEODRIVER=dummy ./build/game --smoke-test
