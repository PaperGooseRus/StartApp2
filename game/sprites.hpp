#pragma once
#include <SDL.h>
#include "pixel/image.hpp"
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
    bool contains(const std::string& name) const { return textures_.find(name) != textures_.end(); }
    void put(const std::string& name, const pixel::Image& image);
private:
    SDL_Renderer* renderer_;
    std::filesystem::path root_;
    std::map<std::string, SpriteTexture> textures_;
};
