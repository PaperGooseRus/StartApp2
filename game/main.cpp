#include "sandbox.hpp"
#include "storage.hpp"
#include "project.hpp"
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "render.hpp"
#include <iostream>
#include "platform.hpp"
#include <stdexcept>
#include <string>
#include <memory>
#include <sstream>

constexpr int scale = GameRenderer::scale;
void smoke(Sandbox& game, SDL_Renderer* renderer) {
    GameRenderer view(renderer);
    SpriteCache cache(renderer);
    const auto& oak = cache.get("oak.png");
    if (oak.width <= 0 || oak.height <= 0 || &oak != &cache.get("oak.png"))
        throw std::runtime_error("Sprite loading/cache failed");
    bool rejected = false;
    try { cache.get("../oak.png"); } catch (const std::runtime_error&) { rejected = true; }
    if (!rejected) throw std::runtime_error("Sprite path traversal accepted");
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
    view.draw(game, false);
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
    pixel::SceneObject back, front;
    back.visuals = {{pixel::RenderLayer::Background, 1000, {5, 5, 1, 1}, {0, 255, 0, 255}}};
    front.visuals = {{pixel::RenderLayer::Characters, 1000, {5, 5, 1, 1}, {0, 0, 255, 255}},
                    {pixel::RenderLayer::Foreground, -1000, {5, 5, 1, 1}, {255, 80, 0, 255}}};
    game.scene.add(front); game.scene.add(back);
    view.draw(game, false);
    sample = SDL_Rect{5 * scale, 5 * scale, 1, 1};
    if (SDL_RenderReadPixels(renderer, &sample, SDL_PIXELFORMAT_RGBA32, rgba, 4) != 0 || rgba[0] != 255 || rgba[1] != 80)
        throw std::runtime_error("Foreground must cover other layers");
    view.showForeground = false;
    view.draw(game, false);
    if (SDL_RenderReadPixels(renderer, &sample, SDL_PIXELFORMAT_RGBA32, rgba, 4) != 0 || rgba[2] != 255 || rgba[0] != 0)
        throw std::runtime_error("Foreground toggle must reveal character layer");
    view.camera().moveTo({16.09f, 0});
    const auto brushTarget = view.mouseToWorld(44 * scale, 10 * scale);
    if (brushTarget.x != 60 || brushTarget.y != 10) throw std::runtime_error("Camera brush coordinates failed");
    game.paint(static_cast<int>(brushTarget.x), static_cast<int>(brushTarget.y), pixel::Material::Water);
    view.draw(game, false);
    sample = SDL_Rect{44 * scale, 10 * scale, 1, 1};
    if (SDL_RenderReadPixels(renderer, &sample, SDL_PIXELFORMAT_RGBA32, rgba, 4) != 0 || rgba[2] != 226 || rgba[0] != 49)
        throw std::runtime_error("Panned water brush must align with rendering");
    // Verify transparent PNG corners, cropped source regions and color modulation.
    view.camera().moveTo({0, 0});
    pixel::SceneObject png;
    pixel::Visual image{pixel::RenderLayer::Characters, 2000, {5, 20, 16, 16}};
    image.sprite = pixel::Sprite{"oak.png", {0, 0, 1, 1}};
    png.visuals.push_back(image);
    image.bounds = {30, 20, 4, 4};
    image.sprite->source = {0.45f, 0.75f, 0.1f, 0.1f};
    image.color = {0, 255, 255, 255};
    png.visuals.push_back(image);
    game.scene.add(png);
    view.draw(game, false);
    sample = SDL_Rect{5 * scale, 20 * scale, 1, 1};
    if (SDL_RenderReadPixels(renderer, &sample, SDL_PIXELFORMAT_RGBA32, rgba, 4) != 0 || rgba[0] != 19 || rgba[1] != 27)
        throw std::runtime_error("PNG transparency failed");
    sample = SDL_Rect{32 * scale, 22 * scale, 1, 1};
    if (SDL_RenderReadPixels(renderer, &sample, SDL_PIXELFORMAT_RGBA32, rgba, 4) != 0 || rgba[0] > 2 || rgba[1] <= 27)
        throw std::runtime_error("PNG crop/tint failed");
    pixel::Image atlas(2,1); atlas.at(0,0)={255,0,0,255}; atlas.at(1,0)={0,0,255,255};
    view.sprites().put("@test-animation",atlas);
    pixel::SceneObject animated;
    pixel::Visual animation{pixel::RenderLayer::Characters,3000,{45,20,2,2}};
    animation.sprite = pixel::Sprite{"@test-animation",{0,0,.5f,1},pixel::AnimationClip({{{0,0,.5f,1},100},{{.5f,0,.5f,1},100}})};
    animated.visuals.push_back(animation); game.scene.add(animated);
    sample = SDL_Rect{45*scale,20*scale,1,1};
    view.draw(game,false,0,0);
    if(SDL_RenderReadPixels(renderer,&sample,SDL_PIXELFORMAT_RGBA32,rgba,4)!=0||rgba[0]!=255||rgba[2]!=0)throw std::runtime_error("Animation first frame failed");
    view.draw(game,false,0,.1);
    if(SDL_RenderReadPixels(renderer,&sample,SDL_PIXELFORMAT_RGBA32,rgba,4)!=0||rgba[0]!=0||rgba[2]!=255)throw std::runtime_error("Animation second frame failed");
    SDL_RenderPresent(renderer);
    std::stringstream snapshot(std::ios::in | std::ios::out | std::ios::binary);
    storage::write(game, snapshot);
    auto restored = storage::read(snapshot);
    if (restored.x != game.x || restored.y != game.y ||
        restored.world.mass(pixel::Material::Water) != game.world.mass(pixel::Material::Water))
        throw std::runtime_error("Game save restore failed");
    std::cout << "Game smoke checks passed: movement, floor, footsteps, jump, landing, brush, rendering, layers, camera brush, PNG transparency/crop/cache, animated rendering, save/load\n";
}
int main(int argc, char** argv) {
    const auto arguments = applicationArguments(argc, argv);
    argc = static_cast<int>(arguments.size());
    const bool projectCheck = argc > 2 && arguments[1] == "--check-project";
    const bool testing = projectCheck || (argc > 1 && arguments[1] == "--smoke-test");
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) { std::cerr << SDL_GetError() << '\n'; return 1; }
    SDL_Window* window = SDL_CreateWindow("Pixel sandbox 0.7", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          GameRenderer::viewportWidth * scale, GameRenderer::viewportHeight * scale,
                                          testing ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE) : nullptr;
    int result = 0;
    if (!renderer) { std::cerr << SDL_GetError() << '\n'; result = 1; }
    else try {
        GameRenderer view(renderer);
        std::optional<studio::Project> project;
        if (argc > 2 && (arguments[1] == "--project" || projectCheck)) {
            project = studio::load(std::filesystem::u8path(arguments[2]));
            for (const auto& art : project->art) view.sprites().put(art.key(), art.atlas());
        }
        Sandbox game = project ? project->game : Sandbox{};
        double animationTime = 0;
        if (projectCheck) {
            for (int i=0;i<60;++i) game.tick(0,false);
            view.draw(game,false,0,0.5);
            std::cout << "Project loaded, simulated and rendered successfully\n";
        } else if (testing) smoke(game, renderer);
        else {
            std::unique_ptr<char, decltype(&SDL_free)> prefPath(SDL_GetPrefPath("PaperGoose", "PixelSandbox"), SDL_free);
            std::filesystem::path saveFile;
            if (prefPath) saveFile = std::filesystem::u8path(prefPath.get()) / (project ? "project-quick-save.pxproj" : "quick-save.pxs");
            if (!saveFile.empty()) std::cout << "Save file: " << saveFile.u8string() << '\n';
            else std::cerr << "Save directory unavailable: " << SDL_GetError() << '\n';
            std::string notice;
            double noticeUntil = 0;
            std::cout << "A/D or arrows: move; Space: jump; 1-6: sand/water/gas/stone/soil/erase;\n"
                         "Left mouse: paint; Right mouse: erase; P: pause; F: fast simulation; R: reset; F5: save; F9: load; E: interact; C: camera follow; J/L: pan; Tab: foreground; Esc: exit\n";
            pixel::Material brush = pixel::Material::Sand;
            const char* names[] = {"AIR", "STONE", "SOIL", "SAND", "WATER", "GAS"};
            bool running = true, paused = false, fast = false, followCamera = true;
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
                    case SDLK_r: game = project ? project->game : Sandbox{}; animationTime = 0; accumulator = 0; jump = false; view.followPlayer(game); break;
                    case SDLK_c: followCamera = !followCamera; break;
                    case SDLK_TAB: view.showForeground = !view.showForeground; break;
                    case SDLK_e: {
                        const auto interaction = game.interact();
                        if (interaction == Sandbox::Interaction::Opened) notice = "DOOR OPEN";
                        else if (interaction == Sandbox::Interaction::Closed) notice = "DOOR CLOSED";
                        else if (interaction == Sandbox::Interaction::Occupied) notice = "STEP OUT OF THE GATE";
                        else if (interaction == Sandbox::Interaction::ChestOpened) notice = "CHEST OPEN";
                        else if (interaction == Sandbox::Interaction::ChestClosed) notice = "CHEST CLOSED";
                        else notice = "NO ACCESSIBLE OBJECT NEARBY";
                        noticeUntil = static_cast<double>(SDL_GetPerformanceCounter()) / SDL_GetPerformanceFrequency() + 4;
                        break;
                    }
                    case SDLK_F5:
                    case SDLK_F9: {
                        try {
                            if (saveFile.empty()) throw std::runtime_error("Save directory unavailable");
                            if (event.key.keysym.sym == SDLK_F5) {
                                if (project) { auto saved = *project; saved.game = game; studio::save(saved, saveFile); }
                                else storage::save(game, saveFile);
                                notice = "SAVED";
                            } else {
                                if (project) {
                                    auto saved = studio::load(saveFile); game = saved.game;
                                    for (const auto& art : saved.art) view.sprites().put(art.key(), art.atlas());
                                } else game = storage::load(saveFile);
                                accumulator = 0;
                                jump = false;
                                notice = "LOADED";
                                view.followPlayer(game);
                            }
                        } catch (const std::exception& error) {
                            notice = event.key.keysym.sym == SDLK_F5 ? "SAVE FAILED" : "LOAD FAILED";
                            std::cerr << notice << ": " << error.what() << '\n';
                        }
                        std::cout << notice << '\n';
                        noticeUntil = static_cast<double>(SDL_GetPerformanceCounter()) / SDL_GetPerformanceFrequency() + 5;
                        break;
                    }
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
                const double frameTime = std::min(now - previous, 0.1);
                if (!paused) accumulator += frameTime;
                previous = now;
                const auto* keys = SDL_GetKeyboardState(nullptr);
                int direction = int(keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) -
                                int(keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT]);
                if (!followCamera) {
                    const int pan = int(keys[SDL_SCANCODE_L]) - int(keys[SDL_SCANCODE_J]);
                    view.camera().pan({static_cast<float>(pan * 24 * frameTime), 0}, {Sandbox::width, Sandbox::height});
                }
                int mx, my;
                auto buttons = SDL_GetMouseState(&mx, &my);
                while (!paused && accumulator >= 1.0 / 60) {
                    const auto target = view.mouseToWorld(mx, my);
                    if (mx >= 0 && my >= 0 && mx < GameRenderer::viewportWidth * scale && my < GameRenderer::viewportHeight * scale) {
                        if (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) game.paint(static_cast<int>(std::floor(target.x)), static_cast<int>(std::floor(target.y)), brush);
                        if (buttons & SDL_BUTTON(SDL_BUTTON_RIGHT)) game.paint(static_cast<int>(std::floor(target.x)), static_cast<int>(std::floor(target.y)), pixel::Material::Air);
                    }
                    game.tick(direction, jump);
                    animationTime += 1.0 / 60;
                    jump = false;
                    if (fast) for (int i = 0; i < 29; ++i) game.world.tick();
                    accumulator -= 1.0 / 60;
                }
                if (paused) jump = false;
                if (followCamera) view.followPlayer(game);
                std::string soilInfo;
                const auto hover = view.mouseToWorld(mx, my);
                const int hoverX = static_cast<int>(std::floor(hover.x)), hoverY = static_cast<int>(std::floor(hover.y));
                if (mx >= 0 && my >= 0 && hoverX >= 0 && hoverY >= 0 && hoverX < Sandbox::width && hoverY < Sandbox::height &&
                    game.world.material(hoverX, hoverY) == pixel::Material::Soil) {
                    const auto& ground = game.world.ground(hoverX, hoverY);
                    soilInfo = " | Wet=" + std::to_string(static_cast<int>(ground.moisture * 100)) +
                        "% Compacted=" + std::to_string(static_cast<int>(ground.compaction * 100)) + "%";
                }
                std::string title = std::string("Pixel sandbox 0.7") + (now < noticeUntil ? " | " + notice : "") +
                    (game.interactionPrompt().empty() ? "" : " | " + game.interactionPrompt()) +
                    soilInfo + " | Free=" + std::to_string(static_cast<int>(game.world.freeWaterMass())) +
                    " Soil=" + std::to_string(static_cast<int>(game.world.absorbedWaterMass())) +
                    " Gas=" + std::to_string(static_cast<int>(game.world.mass(pixel::Material::Gas))) +
                    " | Brush=" + names[static_cast<int>(brush)] + (paused ? " | PAUSED" : "") +
                    (fast ? " | WORLD x30" : "") +  (followCamera ? " | CAMERA FOLLOW" : " | CAMERA J/L") +
                    (view.showForeground ? " | FRONT ON" : " | FRONT OFF") + " | E interact C camera Tab front F5/F9";
                SDL_SetWindowTitle(window, title.c_str());
                view.draw(game, true, direction, animationTime);
                SDL_Delay(1);
            }
        }
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; result = 1; }
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
