#include "sandbox.hpp"
#include <SDL.h>
#include <iostream>
#include <stdexcept>
#include <string>

constexpr int scale = 8;
void render(SDL_Renderer* renderer, const Sandbox& game, bool present = true) {
    SDL_SetRenderDrawColor(renderer, 19, 27, 43, 255);
    SDL_RenderClear(renderer);
    using pixel::Material;
    for (int y = 0; y < Sandbox::height; ++y) {
        for (int x = 0; x < Sandbox::width; ++x) {
            auto m = game.world.material(x, y);
            if (m == Material::Air) continue;
            switch (m) {
            case Material::Stone: SDL_SetRenderDrawColor(renderer, 112, 122, 138, 255); break;
            case Material::Soil: SDL_SetRenderDrawColor(renderer, 104, 70, 43, 255); break;
            case Material::Sand: SDL_SetRenderDrawColor(renderer, 231, 193, 93, 255); break;
            case Material::Water: SDL_SetRenderDrawColor(renderer, 49, 143, 226, 255); break;
            case Material::Gas: SDL_SetRenderDrawColor(renderer, 179, 141, 206, 255); break;
            default: break;
            }
            SDL_Rect cell{x * scale, y * scale, scale, scale};
            if (game.world.gas(x, y) > 0) {
                const auto opacity = static_cast<Uint8>(std::clamp(game.world.gasPressure(x, y) * 180, 0.0, 230.0));
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 179, 141, 206, opacity);
                SDL_Rect gasCell = cell;
                gasCell.h = scale - static_cast<int>(std::ceil(game.world.water(x, y) * scale));
                if (gasCell.h > 0) SDL_RenderFillRect(renderer, &gasCell);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            }
            if (m == Material::Gas) continue;
            if (m == Material::Water) SDL_SetRenderDrawColor(renderer, 49, 143, 226, 255);
            if (m == Material::Water) {
                const int depth = std::max(1, static_cast<int>(std::ceil(game.world.water(x, y) * scale)));
                cell.y += scale - depth;
                cell.h = depth;
            }
            SDL_RenderFillRect(renderer, &cell);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            if (m == Material::Soil && y > 0 && (game.world.material(x, y - 1) == Material::Air || game.world.material(x, y - 1) == Material::Gas)) {
                const auto& g = game.world.ground(x, y);
                const int grass = static_cast<int>(g.grass * (1 - 0.7f * g.flattened) * 5);
                SDL_SetRenderDrawColor(renderer, 74, 177, 74, 255);
                SDL_Rect blade{x * scale, y * scale - grass, scale, grass};
                if (grass) SDL_RenderFillRect(renderer, &blade);
            }
        }
    }
    SDL_SetRenderDrawColor(renderer, 255, 218, 148, 255);
    SDL_Rect player{static_cast<int>(game.x * scale), static_cast<int>(game.y * scale), 2 * scale, 3 * scale};
    SDL_RenderFillRect(renderer, &player);
    SDL_SetRenderDrawColor(renderer, 24, 32, 45, 255);
    SDL_Rect eye{player.x + scale, player.y + 4, 3, 3};
    SDL_RenderFillRect(renderer, &eye);
    if (present) SDL_RenderPresent(renderer);
}
void smoke(Sandbox& game, SDL_Renderer* renderer) {
    const float start = game.x;
    for (int i = 0; i < 60; ++i) game.tick(1, false);
    if (game.x < start + 10 || !game.grounded) throw std::runtime_error("Player movement/floor failed");
    bool trampled = false;
    for (int x = 18; x < 32; ++x) if (game.world.ground(x, Sandbox::surface).grass < 1) trampled = true;
    if (!trampled) throw std::runtime_error("Foot contact failed");
    float before = game.y;
    game.tick(0, true);
    if (game.y >= before) throw std::runtime_error("Jump failed");
    for (int i = 0; i < 120; ++i) game.tick(0, false);
    if (!game.grounded) throw std::runtime_error("Landing failed");
    auto count = game.world.mass(pixel::Material::Water);
    double replaced = 0;
    for (int y = 9; y <= 11; ++y)
        for (int x = 9; x <= 11; ++x) replaced += game.world.water(x, y);
    game.paint(10, 10, pixel::Material::Water);
    if (std::abs(game.world.mass(pixel::Material::Water) - (count - replaced + 9)) > 1e-8) throw std::runtime_error("Brush failed");
    game.world.setFluid(40, 10, pixel::Material::Water, 0.25);
    game.world.setFluid(42, 10, pixel::Material::Gas, 1);
    render(renderer, game, false);
    unsigned char rgba[4]{};
    SDL_Rect sample{10 * scale, 10 * scale, 1, 1};
    if (SDL_RenderReadPixels(renderer, &sample, SDL_PIXELFORMAT_RGBA32, rgba, 4) != 0 ||
        rgba[0] != 49 || rgba[1] != 143 || rgba[2] != 226)
        throw std::runtime_error("Rendered water pixel failed");
    sample = SDL_Rect{40 * scale, 10 * scale, 1, 1};
    if (SDL_RenderReadPixels(renderer, &sample, SDL_PIXELFORMAT_RGBA32, rgba, 4) != 0 || rgba[0] != 19)
        throw std::runtime_error("Partial water upper space failed");
    sample.y += scale - 1;
    if (SDL_RenderReadPixels(renderer, &sample, SDL_PIXELFORMAT_RGBA32, rgba, 4) != 0 || rgba[2] != 226)
        throw std::runtime_error("Partial water depth failed");
    sample = SDL_Rect{42 * scale, 10 * scale, 1, 1};
    if (SDL_RenderReadPixels(renderer, &sample, SDL_PIXELFORMAT_RGBA32, rgba, 4) != 0 || rgba[0] <= 19)
        throw std::runtime_error("Gas density rendering failed");
    SDL_RenderPresent(renderer);
    std::cout << "Game smoke checks passed: movement, floor, footsteps, jump, landing, brush, rendering\n";
}
int main(int argc, char** argv) {
    const bool testing = argc > 1 && std::string(argv[1]) == "--smoke-test";
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) { std::cerr << SDL_GetError() << '\n'; return 1; }
    SDL_Window* window = SDL_CreateWindow("Pixel sandbox", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          Sandbox::width * scale, Sandbox::height * scale,
                                          testing ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE) : nullptr;
    int result = 0;
    if (!renderer) { std::cerr << SDL_GetError() << '\n'; result = 1; }
    else try {
        Sandbox game;
        if (testing) smoke(game, renderer);
        else {
            std::cout << "A/D or arrows: move; Space: jump; 1-6: sand/water/gas/stone/soil/erase;\n"
                         "Left mouse: paint; Right mouse: erase; P: pause; F: fast simulation; R: reset; Esc: exit\n";
            pixel::Material brush = pixel::Material::Sand;
            const char* names[] = {"AIR", "STONE", "SOIL", "SAND", "WATER", "GAS"};
            bool running = true, paused = false, fast = false;
            double previous = static_cast<double>(SDL_GetPerformanceCounter()) / SDL_GetPerformanceFrequency();
            double accumulator = 0;
            bool jump = false;
            while (running) {
                SDL_Event event;
                while (SDL_PollEvent(&event)) {
                    if (event.type == SDL_QUIT) running = false;
                    if (event.type != SDL_KEYDOWN || event.key.repeat) continue;
                    switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE: running = false; break;
                    case SDLK_SPACE: jump = true; break;
                    case SDLK_p: paused = !paused; accumulator = 0; break;
                    case SDLK_f: fast = !fast; break;
                    case SDLK_r: game = Sandbox{}; accumulator = 0; break;
                    case SDLK_1: brush = pixel::Material::Sand; break;
                    case SDLK_2: brush = pixel::Material::Water; break;
                    case SDLK_3: brush = pixel::Material::Gas; break;
                    case SDLK_4: brush = pixel::Material::Stone; break;
                    case SDLK_5: brush = pixel::Material::Soil; break;
                    case SDLK_6: brush = pixel::Material::Air; break;
                    default: break;
                    }
                }
                double now = static_cast<double>(SDL_GetPerformanceCounter()) / SDL_GetPerformanceFrequency();
                if (!paused) accumulator += std::min(now - previous, 0.1);
                previous = now;
                const auto* keys = SDL_GetKeyboardState(nullptr);
                int direction = int(keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) -
                                int(keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]);
                int mx, my;
                auto buttons = SDL_GetMouseState(&mx, &my);
                while (!paused && accumulator >= 1.0 / 60) {
                    if (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) game.paint(mx / scale, my / scale, brush);
                    if (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) game.paint(mx / scale, my / scale, pixel::Material::Air);
                    game.tick(direction, jump);
                    jump = false;
                    if (fast) for (int i = 0; i < 29; ++i) game.world.tick();
                    accumulator -= 1.0 / 60;
                }
                if (paused) jump = false;
                std::string title = std::string("Pixel sandbox | 1 Sand 2 Water 3 Gas 4 Stone 5 Soil 6 Erase | ") +
                    names[static_cast<int>(brush)] + (paused ? " | PAUSED" : "") + (fast ? " | WORLD x30" : "") +
                    " | Water=" + std::to_string(static_cast<int>(game.world.mass(pixel::Material::Water))) +
                    " Gas=" + std::to_string(static_cast<int>(game.world.mass(pixel::Material::Gas))) +
                    " | A/D move Space jump | P pause F fast R reset";
                SDL_SetWindowTitle(window, title.c_str());
                render(renderer, game);
                SDL_Delay(1);
            }
        }
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; result = 1; }
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
