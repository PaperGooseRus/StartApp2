#include "sprites.hpp"
#include <fstream>
#include <stdexcept>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 2048
#include "../third_party/stb_image.h"

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
    const auto path = root_ / relative;
    const auto size = std::filesystem::file_size(path);
    if (size == 0 || size > 32 * 1024 * 1024) throw std::runtime_error("Invalid PNG file size");
    std::ifstream input(path, std::ios::binary);
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Cannot read sprite: " + name);
    int width, height, channels;
    if (!stbi_info_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels) ||
        width <= 0 || height <= 0 || width > 2048 || height > 2048) throw std::runtime_error("Invalid PNG dimensions");
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
        stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels, 4), stbi_image_free);
    if (!pixels) throw std::runtime_error("Cannot decode PNG sprite: " + name);
    SpriteTexture texture;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    texture.image.reset(SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height));
    if (!texture.image || SDL_UpdateTexture(texture.image.get(), nullptr, pixels.get(), width * 4) != 0 ||
        SDL_SetTextureBlendMode(texture.image.get(), SDL_BLENDMODE_BLEND) != 0)
        throw std::runtime_error("Cannot upload sprite texture: " + std::string(SDL_GetError()));
    texture.width = width;
    texture.height = height;
    return textures_.emplace(name, std::move(texture)).first->second;
}
