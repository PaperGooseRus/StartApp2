#include "pixel/scene.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace pixel {
namespace {
bool finite(Vec2 value) { return std::isfinite(value.x) && std::isfinite(value.y); }
bool valid(Rect rect) {
    return finite({rect.x, rect.y}) && std::isfinite(rect.width) && std::isfinite(rect.height) &&
        rect.width > 0 && rect.height > 0 && std::isfinite(rect.x + rect.width) && std::isfinite(rect.y + rect.height);
}
Rect positioned(Rect local, Vec2 position) {
    local.x += position.x; local.y += position.y;
    return local;
}
}
bool overlaps(Rect a, Rect b) noexcept {
    return a.x < b.x + b.width && a.x + a.width > b.x && a.y < b.y + b.height && a.y + a.height > b.y;
}
ObjectId Scene::add(SceneObject object) {
    if (!finite(object.position) || objects_.size() >= std::numeric_limits<ObjectId>::max())
        throw std::invalid_argument("Invalid scene position or object limit");
    for (const auto& visual : object.visuals) {
        if (visual.layer > RenderLayer::Foreground || !valid(visual.bounds) ||
            !valid(positioned(visual.bounds, object.position)) || !std::isfinite(visual.parallax) ||
            visual.parallax < 0 || visual.parallax > 4)
            throw std::invalid_argument("Invalid visual");
    }
    if (object.collider && (!valid(object.collider->bounds) || !valid(positioned(object.collider->bounds, object.position))))
        throw std::invalid_argument("Invalid collider");
    objects_.push_back(std::move(object));
    return static_cast<ObjectId>(objects_.size() - 1);
}
const SceneObject& Scene::object(ObjectId id) const { return objects_.at(id); }
void Scene::setPosition(ObjectId id, Vec2 position) {
    auto& value = objects_.at(id);
    if (!finite(position)) throw std::invalid_argument("Invalid scene position");
    for (const auto& visual : value.visuals)
        if (!valid(positioned(visual.bounds, position))) throw std::invalid_argument("Invalid visual position");
    if (value.collider && !valid(positioned(value.collider->bounds, position)))
        throw std::invalid_argument("Invalid collider position");
    value.position = position;
}
void Scene::setColliderEnabled(ObjectId id, bool enabled) {
    auto& collider = objects_.at(id).collider;
    if (!collider) throw std::invalid_argument("Object has no collider");
    collider->enabled = enabled;
}
void Scene::setVisualVisible(ObjectId id, std::size_t visual, bool visible) {
    objects_.at(id).visuals.at(visual).visible = visible;
}
std::vector<DrawCommand> Scene::drawList() const {
    std::vector<DrawCommand> result;
    for (std::size_t i = 0; i < objects_.size(); ++i) {
        for (auto visual : objects_[i].visuals) {
            if (!visual.visible) continue;
            visual.bounds = positioned(visual.bounds, objects_[i].position);
            result.push_back({static_cast<ObjectId>(i), visual});
        }
    }
    std::stable_sort(result.begin(), result.end(), [](const DrawCommand& a, const DrawCommand& b) {
        if (a.visual.layer != b.visual.layer) return a.visual.layer < b.visual.layer;
        return a.visual.order < b.visual.order;
    });
    return result;
}
bool Scene::overlaps(Rect box, std::uint32_t groups, int plane) const {
    if (!valid(box)) throw std::invalid_argument("Invalid collision query");
    for (const auto& object : objects_) {
        if (!object.collider) continue;
        const auto& collider = *object.collider;
        if (collider.enabled && collider.plane == plane && (collider.groups & groups) != 0 &&
            pixel::overlaps(box, positioned(collider.bounds, object.position))) return true;
    }
    return false;
}
Camera::Camera(float width, float height) : viewport_{width, height} {
    if (!finite(viewport_) || width <= 0 || height <= 0) throw std::invalid_argument("Invalid camera viewport");
}
void Camera::moveTo(Vec2 position) {
    if (!finite(position)) throw std::invalid_argument("Invalid camera position");
    position_ = position;
}
void Camera::clamp(Vec2 size) {
    if (!finite(size) || size.x <= 0 || size.y <= 0) throw std::invalid_argument("Invalid world size");
    position_.x = std::clamp(position_.x, 0.0f, std::max(0.0f, size.x - viewport_.x));
    position_.y = std::clamp(position_.y, 0.0f, std::max(0.0f, size.y - viewport_.y));
}
void Camera::follow(Vec2 target, Vec2 size) {
    if (!finite(target) || !finite(size) || size.x <= 0 || size.y <= 0)
        throw std::invalid_argument("Invalid camera target or world size");
    moveTo({target.x - viewport_.x / 2, target.y - viewport_.y / 2});
    clamp(size);
}
void Camera::pan(Vec2 delta, Vec2 size) {
    if (!finite(size) || size.x <= 0 || size.y <= 0) throw std::invalid_argument("Invalid world size");
    moveTo({position_.x + delta.x, position_.y + delta.y});
    clamp(size);
}
Vec2 Camera::worldToScreen(Vec2 point, float parallax) const {
    if (!finite(point) || !std::isfinite(parallax) || parallax < 0 || parallax > 4)
        throw std::invalid_argument("Invalid screen transform");
    const Vec2 result{point.x - position_.x * parallax, point.y - position_.y * parallax};
    if (!finite(result)) throw std::invalid_argument("Screen transform overflow");
    return result;
}
Vec2 Camera::screenToWorld(Vec2 point) const {
    if (!finite(point)) throw std::invalid_argument("Invalid world transform");
    const Vec2 result{point.x + position_.x, point.y + position_.y};
    if (!finite(result)) throw std::invalid_argument("World transform overflow");
    return result;
}
}
