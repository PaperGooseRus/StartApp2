#pragma once
#include <cstdint>
#include <cstddef>
#include <optional>
#include <vector>

namespace pixel {
struct Vec2 { float x = 0, y = 0; };
struct Rect { float x = 0, y = 0, width = 1, height = 1; };
struct Color { std::uint8_t r = 255, g = 255, b = 255, a = 255; };
enum class RenderLayer : std::uint8_t { Background, Interactables, Characters, Foreground };
enum class CollisionGroup : std::uint32_t { Solid = 1, Trigger = 2, Actor = 4 };
constexpr std::uint32_t mask(CollisionGroup group) { return static_cast<std::uint32_t>(group); }
bool overlaps(Rect a, Rect b) noexcept;
struct Visual {
    RenderLayer layer = RenderLayer::Interactables;
    int order = 0;
    Rect bounds;
    Color color{};
    float parallax = 1;
    bool visible = true;
};
struct Collider {
    Rect bounds;
    std::uint32_t groups = mask(CollisionGroup::Solid);
    int plane = 0;
    bool enabled = true;
};
struct SceneObject {
    Vec2 position;
    std::vector<Visual> visuals;
    std::optional<Collider> collider;
};
using ObjectId = std::uint32_t;
struct DrawCommand {
    ObjectId object;
    Visual visual; // bounds are now in world coordinates.
};
class Scene {
public:
    ObjectId add(SceneObject object);
    const SceneObject& object(ObjectId id) const;
    void setPosition(ObjectId id, Vec2 position);
    void setColliderEnabled(ObjectId id, bool enabled);
    void setVisualVisible(ObjectId id, std::size_t visual, bool visible);
    std::vector<DrawCommand> drawList() const;
    bool overlaps(Rect box, std::uint32_t groups = mask(CollisionGroup::Solid), int plane = 0) const;
private:
    std::vector<SceneObject> objects_;
};
class Camera {
public:
    Camera(float viewportWidth, float viewportHeight);
    Vec2 position() const noexcept { return position_; }
    Vec2 viewport() const noexcept { return viewport_; }
    void moveTo(Vec2 position);
    void follow(Vec2 target, Vec2 worldSize);
    void pan(Vec2 delta, Vec2 worldSize);
    Vec2 worldToScreen(Vec2 point, float parallax = 1) const;
    Vec2 screenToWorld(Vec2 point) const;
private:
    void clamp(Vec2 worldSize);
    Vec2 position_, viewport_;
};
}
