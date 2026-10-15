#pragma once
#include "pixel/world.hpp"
#include "pixel/scene.hpp"
#include <algorithm>
#include <cmath>

struct Sandbox {
    static constexpr int width = 128, height = 72, surface = 55;
    pixel::World world{width, height};
    pixel::Scene scene;
    pixel::ObjectId gate = 0;
    bool gateOpen = false;
    enum class Interaction { TooFar, Opened, Closed, Occupied };
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
        createScene();
    }
    void createScene() {
        using pixel::RenderLayer;
        for (int peak : {12, 48, 86, 122}) {
            pixel::SceneObject mountain;
            for (int step = -12; step <= 12; step += 2) {
                const float h = 4 + (12 - std::abs(step)) * 1.2f;
                mountain.visuals.push_back({RenderLayer::Background, -10,
                    {static_cast<float>(peak + step), surface - 3 - h, 2, h}, {38, 55, 77, 255}, 0.25f});
            }
            scene.add(std::move(mountain));
        }
        pixel::SceneObject tree;
        tree.position = {62, 0};
        tree.visuals = {
            {RenderLayer::Background, 10, {-6, 43, 13, 8}, {31, 69, 48, 255}, 1},
            {RenderLayer::Interactables, 10, {0, 48, 1, 7}, {107, 75, 47, 255}, 1},
            {RenderLayer::Foreground, 0, {-5, 51, 11, 2}, {50, 109, 62, 230}, 1}
        };
        scene.add(std::move(tree));
        for (int cx : {22, 92, 116}) {
            pixel::SceneObject bush;
            bush.position = {static_cast<float>(cx), static_cast<float>(surface)};
            bush.visuals.push_back({RenderLayer::Foreground, 5, {-2, -1.5f, 4, 1.5f}, {47, 119, 64, 200}, 1.15f});
            scene.add(std::move(bush));
        }
        pixel::SceneObject door;
        door.position = {36, static_cast<float>(surface - 5)};
        door.collider = pixel::Collider{{0, 0, 1, 5}};
        door.visuals = {
            {RenderLayer::Interactables, 20, {0, 0, 1, 5}, {207, 139, 64, 255}, 1, true},
            {RenderLayer::Interactables, 20, {0, 0, 3, 0.4f}, {94, 172, 140, 255}, 1, false}
        };
        gate = scene.add(std::move(door));
    }
    void setGateOpen(bool open) {
        gateOpen = open;
        scene.setColliderEnabled(gate, !open);
        scene.setVisualVisible(gate, 0, !open);
        scene.setVisualVisible(gate, 1, open);
    }
    Interaction interact() {
        const auto position = scene.object(gate).position;
        if (std::abs(x + 1 - (position.x + 0.5f)) > 4.5f ||
            std::abs(y + 1.5f - (position.y + 2.5f)) > 4) return Interaction::TooFar;
        if (gateOpen && pixel::overlaps({x, y, 2, 3}, {position.x, position.y, 1, 5}))
            return Interaction::Occupied;
        setGateOpen(!gateOpen);
        return gateOpen ? Interaction::Opened : Interaction::Closed;
    }
    bool solid(int cx, int cy) const {
        if (cx < 0 || cx >= width || cy < 0 || cy >= height) return true;
        auto m = world.material(cx, cy);
        return m == pixel::Material::Stone || m == pixel::Material::Soil || m == pixel::Material::Sand;
    }
    bool blocked(float px, float py) const {
        if (scene.overlaps({px, py, 2, 3})) return true;
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
