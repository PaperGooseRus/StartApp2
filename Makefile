CXX ?= g++
CPPFLAGS += -Iinclude
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic

.PHONY: all test clean
ENGINE_SOURCES = src/world.cpp src/soil.cpp src/snapshot.cpp src/scene.cpp
ENGINE_HEADERS = $(wildcard include/pixel/*.hpp)

all: build/demo build/tests build/soil-tests build/scene-tests build/snapshot-tests
build:
	mkdir -p build
build/demo: $(ENGINE_SOURCES) $(ENGINE_HEADERS) examples/demo.cpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ENGINE_SOURCES) examples/demo.cpp -o $@
build/tests: $(ENGINE_SOURCES) $(ENGINE_HEADERS) tests/world_test.cpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ENGINE_SOURCES) tests/world_test.cpp -o $@
build/soil-tests: $(ENGINE_SOURCES) $(ENGINE_HEADERS) tests/soil_test.cpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(ENGINE_SOURCES) tests/soil_test.cpp -o $@
build/scene-tests: $(ENGINE_SOURCES) $(ENGINE_HEADERS) game/sandbox.hpp tests/scene_test.cpp | build
	$(CXX) $(CPPFLAGS) -Igame $(CXXFLAGS) $(ENGINE_SOURCES) tests/scene_test.cpp -o $@
build/snapshot-tests: $(ENGINE_SOURCES) $(ENGINE_HEADERS) tests/snapshot_test.cpp game/storage.cpp game/storage.hpp game/sandbox.hpp | build
	$(CXX) $(CPPFLAGS) -Igame $(CXXFLAGS) $(ENGINE_SOURCES) game/storage.cpp tests/snapshot_test.cpp -o $@
test: build/tests build/soil-tests build/scene-tests build/snapshot-tests
	./build/tests
	./build/soil-tests
	./build/scene-tests
	./build/snapshot-tests
clean:
	rm -rf build

# SDL2 is required only for the graphical sandbox.
SDL_CFLAGS ?= $(shell pkg-config --cflags sdl2)
SDL_LIBS ?= $(shell pkg-config --libs sdl2)
.PHONY: game game-test
game: build/game
build/game: $(ENGINE_SOURCES) $(ENGINE_HEADERS) game/main.cpp game/sandbox.hpp game/storage.cpp game/storage.hpp game/player_visual.hpp game/render.cpp game/render.hpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SDL_CFLAGS) $(ENGINE_SOURCES) game/main.cpp game/render.cpp game/storage.cpp -o $@ $(SDL_LIBS)
game-test: build/game
	SDL_VIDEODRIVER=dummy ./build/game --smoke-test
