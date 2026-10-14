CXX ?= g++
CPPFLAGS += -Iinclude
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic

.PHONY: all test clean
all: build/demo build/tests
build:
	mkdir -p build
build/demo: src/world.cpp include/pixel/world.hpp examples/demo.cpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/world.cpp examples/demo.cpp -o $@
build/tests: src/world.cpp include/pixel/world.hpp tests/world_test.cpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/world.cpp tests/world_test.cpp -o $@
test: build/tests
	./build/tests
clean:
	rm -rf build

# SDL2 is required only for the graphical sandbox.
SDL_CFLAGS ?= $(shell pkg-config --cflags sdl2)
SDL_LIBS ?= $(shell pkg-config --libs sdl2)
.PHONY: game game-test
game: build/game
build/game: src/world.cpp include/pixel/world.hpp game/main.cpp game/sandbox.hpp | build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SDL_CFLAGS) src/world.cpp game/main.cpp -o $@ $(SDL_LIBS)
game-test: build/game
	SDL_VIDEODRIVER=dummy ./build/game --smoke-test
