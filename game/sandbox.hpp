#pragma once
#include "pixel/world.hpp"
#include <algorithm>
#include <cmath>

struct Sandbox {
    static constexpr int width = 128, height = 72, surface = 55;
    pixel::World world{width, height};
    float x = 18, y = surface - 3, velocityY = 0, walked = 0;
    bool grounded = true;
    Sandbox() {
        using pixel::Material;
        for (int cy = surface; cy < height; ++cy)
            for (int cx = 0; cx < width; ++cx)
                world.set(cx, cy, cy == height - 1 ? Material::Stone : Material::Soil);
        // A dry patch lets players test irrigation and regrowth immediately.
        for (int cy = surface; cy < height - 1; ++cy)
            for (int cx = 8; cx < 30; ++cx) world.setMoisture(cx, cy, 0.12);
        for (int cy = 43; cy < surface; ++cy) {
            world.set(78, cy, Material::Stone);
            world.set(106, cy, Material::Stone);
        }
        for (int cy = 48; cy < surface; ++cy)
            for (int cx = 79; cx < 106; ++cx) world.set(cx, cy, Material::Water);
        for (int cy = 35; cy < 41; ++cy)
            for (int cx = 48; cx < 54; ++cx) world.set(cx, cy, Material::Sand);
        for (int cy = 34; cy < 39; ++cy)
            for (int cx = 112; cx < 116; ++cx) world.set(cx, cy, Material::Gas);
    }
    bool solid(int cx, int cy) const {
        if (cx < 0 || cx >= width || cy < 0 || cy >= height) return true;
        auto m = world.material(cx, cy);
        return m == pixel::Material::Stone || m == pixel::Material::Soil || m == pixel::Material::Sand;
    }
    bool blocked(float px, float py) const {
        for (int cy = static_cast<int>(std::floor(py)); cy <= static_cast<int>(std::floor(py + 2.99f)); ++cy)
            for (int cx = static_cast<int>(std::floor(px)); cx <= static_cast<int>(std::floor(px + 1.99f)); ++cx)
                if (solid(cx, cy)) return true;
        return false;
    }
    void tick(int direction, bool jump) {
        constexpr float dt = 1.0f / 60;
        world.tick();
        if (jump && grounded) { velocityY = -22; grounded = false; }
        float dx = direction * 12.0f * dt;
        if (!blocked(x + dx, y)) {
            x += dx;
            if (grounded) walked += std::abs(dx);
        }
        velocityY = std::min(velocityY + 55 * dt, 35.0f);
        float dy = velocityY * dt;
        // Small substeps prevent tunnelling through one-cell floors.
        int steps = std::max(1, static_cast<int>(std::ceil(std::abs(dy) / 0.2f)));
        grounded = false;
        for (int i = 0; i < steps; ++i) {
            if (blocked(x, y + dy / steps)) {
                grounded = velocityY > 0;
                velocityY = 0;
                break;
            }
            y += dy / steps;
        }
        if (grounded && walked >= 1.0f) {
            const int cx = static_cast<int>(x + 1), cy = static_cast<int>(std::floor(y + 3.05f));
            if (cx >= 0 && cx < width && cy >= 0 && cy < height) world.stepOn(cx, cy, 3);
            walked = 0;
        }
    }
    void paint(int cx, int cy, pixel::Material material) {
        for (int yy = cy - 1; yy <= cy + 1; ++yy)
            for (int xx = cx - 1; xx <= cx + 1; ++xx) {
                if (xx < 0 || xx >= width || yy < 0 || yy >= height - 1) continue;
                bool player = xx >= std::floor(x) && xx <= std::floor(x + 1.99f) &&
                              yy >= std::floor(y) && yy <= std::floor(y + 2.99f);
                if (!player) world.set(xx, yy, material);
            }
    }
};
