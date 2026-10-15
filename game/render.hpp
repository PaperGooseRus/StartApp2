#pragma once
#include "sandbox.hpp"
#include <SDL.h>

class GameRenderer {
public:
    static constexpr int scale = 8, viewportWidth = 96, viewportHeight = Sandbox::height;
    explicit GameRenderer(SDL_Renderer* renderer);
    pixel::Camera& camera() noexcept { return camera_; }
    void followPlayer(const Sandbox& game);
    pixel::Vec2 mouseToWorld(int pixelX, int pixelY) const;
    void draw(const Sandbox& game, bool present = true, int movement = 0);
    bool showForeground = true;
private:
    pixel::Camera frameCamera() const;
    void quad(pixel::Rect rect, pixel::Color color, float parallax = 1);
    void terrain(const pixel::World& world);
    SDL_Renderer* renderer_;
    pixel::Camera camera_{viewportWidth, viewportHeight};
};
