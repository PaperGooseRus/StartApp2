#pragma once
#include "sandbox.hpp"
#include "pixel/image.hpp"
#include <filesystem>
namespace studio {
struct SpriteArt {
    std::uint32_t id = 1;
    std::string name = "Новый спрайт";
    std::vector<pixel::Image> frames{pixel::Image{32, 32}};
    std::vector<std::uint32_t> durations{120};
    bool loop = true;
    std::string key() const { return "@art/" + std::to_string(id); }
    void validate() const;
    pixel::Image atlas() const;
    pixel::AnimationClip clip() const;
};
struct Project {
    std::string name = "Моя игра";
    Sandbox game;
    std::vector<SpriteArt> art;
    std::uint32_t nextArtId = 1;
    void validate() const;
    void refreshSprites();
    static Project example();
};
void save(const Project& project, const std::filesystem::path& file);
Project load(const std::filesystem::path& file);
SpriteArt importPng(const std::filesystem::path& file, int columns, std::uint32_t id);
}
