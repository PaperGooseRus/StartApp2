#define SDL_MAIN_HANDLED
#include "application.hpp"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"
#include <iostream>
#include "platform.hpp"
#include <fstream>
#include <memory>
#include <vector>
int main(int argc, char** argv) {
    const auto arguments = applicationArguments(argc, argv);
    argc = static_cast<int>(arguments.size());
    const bool test = argc > 1 && arguments[1] == "--smoke-test";
    const bool shot = argc > 2 && (arguments[1] == "--screenshot" || arguments[1] == "--screenshot-sprites");
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) { std::cerr << SDL_GetError() << '\n'; return 1; }
    auto window = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>(SDL_CreateWindow("PixelStudio 0.7 — редактор игр", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1440, 900,
        (test || shot ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN) | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI), SDL_DestroyWindow);
    if (!window) { std::cerr << SDL_GetError() << '\n'; SDL_Quit(); return 1; }
    SDL_SetWindowMinimumSize(window.get(), 1024, 700);
    auto renderer = std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)>(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_TARGETTEXTURE), SDL_DestroyRenderer);
    if (!renderer) { std::cerr << SDL_GetError() << '\n'; window.reset(); SDL_Quit(); return 1; }
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    auto& io = ImGui::GetIO(); io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // Dock layout lasts for the current session; project data is stored separately.
    int result = 0;
    bool platformReady = false, rendererReady = false;
    try {
        std::unique_ptr<char, decltype(&SDL_free)> base(SDL_GetBasePath(), SDL_free);
        auto assets = base ? std::filesystem::u8path(base.get()) / "assets" : std::filesystem::path("assets");
        if (!std::filesystem::is_directory(assets)) assets = "assets";
        std::ifstream font(assets / "fonts/DejaVuSans.ttf", std::ios::binary | std::ios::ate);
        if (!font) throw std::runtime_error("Missing assets/fonts/DejaVuSans.ttf. Extract the entire package.");
        const auto fontSize = font.tellg();
        if (fontSize <= 0 || fontSize > 4 * 1024 * 1024) throw std::runtime_error("Invalid editor font");
        std::vector<unsigned char> fontBytes(static_cast<std::size_t>(fontSize)); font.seekg(0);
        if (!font.read(reinterpret_cast<char*>(fontBytes.data()), fontSize)) throw std::runtime_error("Cannot read editor font");
        ImFontConfig cfg; cfg.FontDataOwnedByAtlas = false;
        ImFontGlyphRangesBuilder glyphs; glyphs.AddRanges(io.Fonts->GetGlyphRangesCyrillic()); glyphs.AddText(u8"—–…→←▶■▷");
        ImVector<ImWchar> ranges; glyphs.BuildRanges(&ranges);
        io.Fonts->AddFontFromMemoryTTF(fontBytes.data(), int(fontBytes.size()), 15.0f, &cfg, ranges.Data);
        platformReady = ImGui_ImplSDL2_InitForSDLRenderer(window.get(), renderer.get());
        rendererReady = ImGui_ImplSDLRenderer2_Init(renderer.get());
        if (!platformReady || !rendererReady) throw std::runtime_error("Cannot initialize editor UI");
        {
            studio::Application app(window.get(), renderer.get());
            if (argc > 1 && !test && !shot) app.open(std::filesystem::u8path(arguments[1]));
            if (test) app.smoke();
            else if (shot) { if (arguments[1] == "--screenshot-sprites") app.showSprites(); for (int i = 0; i < 5; ++i) app.frame(1.0 / 60); app.screenshot(std::filesystem::u8path(arguments[2])); }
            else {
                bool running = true; auto previous = SDL_GetPerformanceCounter();
                while (running) {
                    SDL_Event event;
                    while (SDL_PollEvent(&event)) {
                        ImGui_ImplSDL2_ProcessEvent(&event);
                        if (event.type == SDL_QUIT || (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE))
                            if (app.requestQuit()) running = false;
                    }
                    const auto now = SDL_GetPerformanceCounter();
                    app.frame(std::min(double(now - previous) / SDL_GetPerformanceFrequency(), 0.1)); previous = now;
                    SDL_Delay(8);
                }
            }
        }
        // Font memory must survive until the backend has finished using the atlas.
        if (rendererReady) { ImGui_ImplSDLRenderer2_Shutdown(); rendererReady = false; }
        if (platformReady) { ImGui_ImplSDL2_Shutdown(); platformReady = false; }
        ImGui::DestroyContext();
    } catch (const std::exception& e) {
        std::cerr << "PixelStudio: " << e.what() << '\n'; result = 1;
        if (rendererReady) ImGui_ImplSDLRenderer2_Shutdown();
        if (platformReady) ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
    }
    renderer.reset(); window.reset(); SDL_Quit(); return result;
}
