#include "sprites.hpp"
#include <fstream>
#include <stdexcept>
#include <vector>
SpriteCache::SpriteCache(SDL_Renderer* renderer) : renderer_(renderer) {
    std::unique_ptr<char, decltype(&SDL_free)> base(SDL_GetBasePath(), SDL_free);
    if (!base) throw std::runtime_error("Cannot locate application assets");
    root_ = std::filesystem::u8path(base.get()) / "assets";
    if (!std::filesystem::is_directory(root_)) root_ = std::filesystem::current_path() / "assets";
}
const SpriteTexture& SpriteCache::get(const std::string& name) {
    const auto existing = textures_.find(name);
    if (existing != textures_.end()) return existing->second;
    auto relative = std::filesystem::u8path(name);
    if (name.find('\0') != std::string::npos || relative.is_absolute() || relative.has_root_name())
        throw std::runtime_error("Invalid sprite path");
    for (const auto& part : relative)
        if (part == "..") throw std::runtime_error("Sprite path leaves assets directory");
    put(name, pixel::Image::loadPng(root_ / relative));
    return textures_.at(name);
}
void SpriteCache::put(const std::string& name, const pixel::Image& pixels) {
    const int width = pixels.width(), height = pixels.height();
    SpriteTexture texture;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    texture.image.reset(SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height));
    if (!texture.image || SDL_UpdateTexture(texture.image.get(), nullptr, pixels.pixels().data(), width * 4) != 0 ||
        SDL_SetTextureBlendMode(texture.image.get(), SDL_BLENDMODE_BLEND) != 0)
        throw std::runtime_error("Cannot upload sprite texture: " + std::string(SDL_GetError()));
    texture.width = width;
    texture.height = height;
    textures_.insert_or_assign(name, std::move(texture));
}
