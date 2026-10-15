#pragma once
#include "document.hpp"
#include "render.hpp"
#include "imgui.h"
#include <map>
namespace studio {
class Application {
public:
    Application(SDL_Window* window, SDL_Renderer* renderer);
    void frame(double dt);
    void smoke();
    void screenshot(const std::filesystem::path& file);
    void open(const std::filesystem::path& file);
    bool requestQuit();
    void showSprites() { requestedWorkspace_ = 1; workspace_ = 1; }
private:
    SDL_Window* window_;
    SDL_Renderer* renderer_;
    GameRenderer view_;
    std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> target_{nullptr, SDL_DestroyTexture};
    Document doc_;
    std::map<std::string, ImVec4> controls_;
    std::uint64_t textureRevision_ = 0;
    std::filesystem::path projectPath_;
    std::string status_ = "Проект готов. Выберите вкладку «Спрайты», чтобы рисовать.";
    std::optional<pixel::ObjectId> selection_;
    std::size_t art_ = 0, frame_ = 0;
    int requestedWorkspace_ = -1;
    int workspace_ = 0, tool_ = 0, levelTool_ = 0, brush_ = 2, brushRadius_ = 1;
    int pixelRadius_ = 0;
    float fluidAmount_ = 1, moisture_ = 0.6f;
    int newWidth_ = 32, newHeight_ = 32, importColumns_ = 1, visual_ = 0;
    float color_[4]{1, 0.65f, 0.25f, 1};
    float zoom_ = 14;
    bool grid_ = true, onion_ = false, previewPlaying_ = true, resetLayout_ = false, quit_ = false;
    bool gameFocus_ = false, draggingObject_ = false;
    double previewTime_ = 0, accumulator_ = 0;
    pixel::Vec2 dragStart_, objectStart_;
    std::optional<std::pair<int, int>> lastPixel_;
    std::string dialog_;
    char path_[2048]{};
    void layout();
    void toolbar();
    void scenePanel();
    void properties();
    void workspace();
    void canvas();
    void level();
    void timeline();
    void assets();
    void dialogs();
    void syncTextures();
    void mark(const char* name);
    bool button(const char* label, const char* id = nullptr);
    void changed();
    bool save();
    void fileDialog(const char* action);
    void place(const char* preset);
    SpriteArt* selectedArt();
    void imagePreview(const SpriteArt& asset, std::size_t frame, ImVec2 size);
};
}
