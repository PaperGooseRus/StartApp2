#pragma once
#include <SDL.h>
#include <filesystem>
#include <map>
#include <memory>
#include <string>

struct SpriteTexture {
    std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> image{nullptr, SDL_DestroyTexture};
    int width = 0, height = 0;
};
class SpriteCache {
public:
    explicit SpriteCache(SDL_Renderer* renderer);
    const SpriteTexture& get(const std::string& name);
private:
    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    std::map<std::string, SpriteTexture> textures_;
};
