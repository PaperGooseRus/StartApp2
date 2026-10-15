#pragma once
#include "pixel/world.hpp"
#include "pixel/scene.hpp"
#include <algorithm>
#include <cmath>

struct Sandbox {
    static constexpr int width = 128, height = 72, surface = 55;
    pixel::World world{width, height};
    pixel::Scene scene;
    pixel::ObjectId gate = 0, chest = 0;
    bool gateOpen = false, chestOpen = false;
    enum class Interaction { TooFar, Opened, Closed, Occupied, ChestOpened, ChestClosed };
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
            pixel::SceneObject mountain; mountain.name = "Горы";
            for (int step = -12; step <= 12; step += 2) {
                const float h = 4 + (12 - std::abs(step)) * 1.2f;
                mountain.visuals.push_back({RenderLayer::Background, -10,
                    {static_cast<float>(peak + step), surface - 3 - h, 2, h}, {38, 55, 77, 255}, 0.25f});
            }
            scene.add(std::move(mountain));
        }
        for (int cx : {10, 62, 120}) {
            pixel::SceneObject tree; tree.name = "Дуб";
            tree.position = {static_cast<float>(cx), static_cast<float>(surface)};
            pixel::Visual back{RenderLayer::Background, 10, {-8, -17, 16, 17}, {255, 255, 255, 255}, 1};
            back.sprite = pixel::Sprite{"oak.png", {0, 0, 1, 1}};
            tree.visuals.push_back(back);
            // Re-draw only the lower hanging foliage in front of the actor.
            pixel::Visual front{RenderLayer::Foreground, 0, {-8, -5.1f, 5.76f, 2.55f}, {255, 255, 255, 210}, 1};
            front.sprite = pixel::Sprite{"oak.png", {0, 0.7f, 0.36f, 0.15f}};
            tree.visuals.push_back(front);
            front.bounds = {3.04f, -5.1f, 4.96f, 2.55f};
            front.sprite->source = {0.69f, 0.7f, 0.31f, 0.15f};
            tree.visuals.push_back(front);
            scene.add(std::move(tree));
        }
        for (int cx : {22, 92, 116}) {
            pixel::SceneObject bush; bush.name = "Куст";
            bush.position = {static_cast<float>(cx), static_cast<float>(surface)};
            bush.visuals.push_back({RenderLayer::Foreground, 5, {-2, -1.5f, 4, 1.5f}, {47, 119, 64, 200}, 1.15f});
            scene.add(std::move(bush));
        }
        pixel::SceneObject door; door.name = "Дверь"; door.action = "door";
        door.position = {36, static_cast<float>(surface - 5)};
        door.collider = pixel::Collider{{0, 0, 1, 5}};
        door.interaction = pixel::InteractionArea{{0, 0, 1, 5}};
        door.visuals = {
            {RenderLayer::Interactables, 20, {0, 0, 1, 5}, {207, 139, 64, 255}, 1, true},
            {RenderLayer::Interactables, 20, {0, 0, 3, 0.4f}, {94, 172, 140, 255}, 1, false}
        };
        gate = scene.add(std::move(door));
        pixel::SceneObject box; box.name = "Сундук"; box.action = "chest";
        box.position = {69, surface - 1.5f};
        box.collider = pixel::Collider{{0, 0, 2.5f, 1.5f}};
        box.interaction = pixel::InteractionArea{{0, 0, 2.5f, 1.5f}, 3.5f};
        box.visuals = {
            {RenderLayer::Interactables, 20, {0, 0.5f, 2.5f, 1}, {103, 57, 32, 255}},
            {RenderLayer::Interactables, 21, {0, 0, 2.5f, 0.6f}, {168, 103, 49, 255}},
            {RenderLayer::Interactables, 22, {0, -0.6f, 2.5f, 0.5f}, {168, 103, 49, 255}, 1, false},
            {RenderLayer::Interactables, 23, {0.2f, 0, 0.2f, 1.5f}, {209, 168, 83, 255}},
            {RenderLayer::Interactables, 23, {2.1f, 0, 0.2f, 1.5f}, {209, 168, 83, 255}},
            {RenderLayer::Interactables, 24, {1, 0.45f, 0.5f, 0.45f}, {240, 202, 108, 255}}
        };
        chest = scene.add(std::move(box));
    }
    void setObjectOpen(pixel::ObjectId id, bool open) {
        auto object = scene.object(id);
        if (object.action == "door") {
            if (object.collider) object.collider->enabled = !open;
            if (object.visuals.size() >= 2) { object.visuals[0].visible = !open; object.visuals[1].visible = open; }
        } else if (object.action == "chest" && object.visuals.size() >= 6) {
            object.visuals[1].visible = !open; object.visuals[2].visible = open; object.visuals[5].visible = !open;
        }
        object.state = open; scene.replace(id, std::move(object));
        if (id == gate) gateOpen = open;
        if (id == chest) chestOpen = open;
    }
    void setGateOpen(bool open) { setObjectOpen(gate, open); }
    void setChestOpen(bool open) { setObjectOpen(chest, open); }
    std::optional<pixel::InteractionCandidate> interactionTarget() const {
        const pixel::Vec2 origin{x + 1, y + 1.5f};
        for (const auto& candidate : scene.interactions(origin)) {
            const auto& action = scene.object(candidate.object).action;
            if (action != "door" && action != "chest") continue;
            const int steps = std::max(1, static_cast<int>(std::ceil(candidate.distance * 8)));
            bool visible = true;
            for (int i = 1; i <= steps; ++i) {
                const float t = static_cast<float>(i) / steps;
                const int cx = static_cast<int>(std::floor(origin.x + (candidate.point.x - origin.x) * t));
                const int cy = static_cast<int>(std::floor(origin.y + (candidate.point.y - origin.y) * t));
                if (solid(cx, cy)) { visible = false; break; }
            }
            if (visible) return candidate;
        }
        return std::nullopt;
    }
    std::string interactionPrompt() const {
        const auto target = interactionTarget();
        if (!target) return "";
        const auto& object = scene.object(target->object);
        if (object.action == "door") return object.state ? "E: CLOSE DOOR" : "E: OPEN DOOR";
        if (object.action == "chest") return object.state ? "E: CLOSE CHEST" : "E: OPEN CHEST";
        return "";
    }
    Interaction interact() {
        const auto target = interactionTarget();
        if (!target) return Interaction::TooFar;
        const auto& object = scene.object(target->object);
        const bool open = !object.state;
        if (object.action == "door" && !open && object.collider) {
            auto bounds = object.collider->bounds;
            bounds.x += object.position.x; bounds.y += object.position.y;
            if (pixel::overlaps({x, y, 2, 3}, bounds)) return Interaction::Occupied;
        }
        const bool isChest = object.action == "chest";
        setObjectOpen(target->object, open);
        return isChest ? (open ? Interaction::ChestOpened : Interaction::ChestClosed) :
                         (open ? Interaction::Opened : Interaction::Closed);
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
