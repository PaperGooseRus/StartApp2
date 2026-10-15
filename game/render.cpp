#include "render.hpp"
#include "player_visual.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

GameRenderer::GameRenderer(SDL_Renderer* renderer) : renderer_(renderer), sprites_(renderer) {
    if (!renderer) throw std::invalid_argument("Renderer must exist");
}
void GameRenderer::followPlayer(const Sandbox& game) {
    camera_.follow({game.x + 1, game.y + 1.5f}, {Sandbox::width, Sandbox::height});
    const auto p = camera_.position();
    camera_.moveTo({std::floor(p.x * scale) / scale, std::floor(p.y * scale) / scale});
}
pixel::Camera GameRenderer::frameCamera() const {
    auto result = camera_;
    const auto position = result.position();
    result.moveTo({std::floor(position.x * scale) / scale, std::floor(position.y * scale) / scale});
    return result;
}
pixel::Vec2 GameRenderer::mouseToWorld(int x, int y) const {
    return frameCamera().screenToWorld({static_cast<float>(x) / scale, static_cast<float>(y) / scale});
}
void GameRenderer::quad(pixel::Rect rect, pixel::Color color, float parallax) {
    const auto screen = frameCamera().worldToScreen({rect.x, rect.y}, parallax);
    const auto view = camera_.viewport();
    if (screen.x >= view.x || screen.y >= view.y || screen.x + rect.width <= 0 || screen.y + rect.height <= 0) return;
    const float left = std::max(0.0f, screen.x), top = std::max(0.0f, screen.y);
    const float right = std::min(view.x, screen.x + rect.width), bottom = std::min(view.y, screen.y + rect.height);
    SDL_Rect box{static_cast<int>(std::floor(left * scale)), static_cast<int>(std::floor(top * scale)),
        static_cast<int>(std::ceil(right * scale)) - static_cast<int>(std::floor(left * scale)),
        static_cast<int>(std::ceil(bottom * scale)) - static_cast<int>(std::floor(top * scale))};
    SDL_SetRenderDrawBlendMode(renderer_, color.a < 255 ? SDL_BLENDMODE_BLEND : SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer_, &box);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
}
void GameRenderer::sprite(const pixel::Visual& visual) {
    const auto screen = frameCamera().worldToScreen({visual.bounds.x, visual.bounds.y}, visual.parallax);
    const auto view = camera_.viewport();
    if (screen.x >= view.x || screen.y >= view.y || screen.x + visual.bounds.width <= 0 || screen.y + visual.bounds.height <= 0) return;
    const auto& texture = sprites_.get(visual.sprite->asset);
    const auto region = visual.sprite->animation ? visual.sprite->animation->frames()[visual.sprite->animation->frameAt(animationSeconds_)].source : visual.sprite->source;
    const float left = std::max(0.0f, screen.x), top = std::max(0.0f, screen.y);
    const float right = std::min(view.x, screen.x + visual.bounds.width), bottom = std::min(view.y, screen.y + visual.bounds.height);
    const int sx = std::clamp(static_cast<int>((region.x + (static_cast<double>(left) - screen.x) / visual.bounds.width * region.width) * texture.width), 0, texture.width - 1);
    const int sy = std::clamp(static_cast<int>((region.y + (static_cast<double>(top) - screen.y) / visual.bounds.height * region.height) * texture.height), 0, texture.height - 1);
    const int ex = std::clamp(static_cast<int>(std::ceil((region.x + (static_cast<double>(right) - screen.x) / visual.bounds.width * region.width) * texture.width)), sx + 1, texture.width);
    const int ey = std::clamp(static_cast<int>(std::ceil((region.y + (static_cast<double>(bottom) - screen.y) / visual.bounds.height * region.height) * texture.height)), sy + 1, texture.height);
    SDL_Rect source{sx, sy, ex - sx, ey - sy};
    SDL_Rect destination{static_cast<int>(std::floor(left * scale)), static_cast<int>(std::floor(top * scale)),
        static_cast<int>(std::ceil(right * scale)) - static_cast<int>(std::floor(left * scale)),
        static_cast<int>(std::ceil(bottom * scale)) - static_cast<int>(std::floor(top * scale))};
    SDL_SetTextureColorMod(texture.image.get(), visual.color.r, visual.color.g, visual.color.b);
    SDL_SetTextureAlphaMod(texture.image.get(), visual.color.a);
    if (SDL_RenderCopy(renderer_, texture.image.get(), &source, &destination) != 0)
        throw std::runtime_error("Cannot draw sprite");
}
void GameRenderer::interactionHint(const Sandbox& game) {
    const auto target = game.interactionTarget();
    if (!target) return;
    const auto bounds = target->bounds;
    const auto screen = frameCamera().worldToScreen({bounds.x, bounds.y});
    SDL_SetRenderDrawColor(renderer_, 255, 223, 119, 255);
    SDL_Rect outline{static_cast<int>(screen.x * scale) - 1, static_cast<int>(screen.y * scale) - 1,
        static_cast<int>(bounds.width * scale) + 2, static_cast<int>(bounds.height * scale) + 2};
    SDL_RenderDrawRect(renderer_, &outline);
    // A readable E key badge, without a font/runtime dependency.
    SDL_Rect badge{outline.x + outline.w / 2 - 6, outline.y - 15, 12, 12};
    SDL_SetRenderDrawColor(renderer_, 24, 32, 45, 255); SDL_RenderFillRect(renderer_, &badge);
    SDL_SetRenderDrawColor(renderer_, 255, 223, 119, 255); SDL_RenderDrawRect(renderer_, &badge);
    SDL_RenderDrawLine(renderer_, badge.x + 4, badge.y + 3, badge.x + 4, badge.y + 8);
    for (int y : {3, 5, 8}) SDL_RenderDrawLine(renderer_, badge.x + 4, badge.y + y, badge.x + 8, badge.y + y);
}
void GameRenderer::terrain(const pixel::World& world) {
    const auto camera = frameCamera();
    const auto position = camera.position(), viewport = camera.viewport();
    const int firstX = static_cast<int>(std::floor(std::clamp(position.x, 0.0f, static_cast<float>(world.width()))));
    const int firstY = static_cast<int>(std::floor(std::clamp(position.y, 0.0f, static_cast<float>(world.height()))));
    const int lastX = static_cast<int>(std::ceil(std::clamp(position.x + viewport.x, 0.0f, static_cast<float>(world.width()))));
    const int lastY = static_cast<int>(std::ceil(std::clamp(position.y + viewport.y, 0.0f, static_cast<float>(world.height()))));
    using pixel::Material;
    for (int y = firstY; y < lastY; ++y) {
        for (int x = firstX; x < lastX; ++x) {
            const auto material = world.material(x, y);
            if (material == Material::Air) continue;
            pixel::Rect cell{static_cast<float>(x), static_cast<float>(y), 1, 1};
            pixel::Color color;
            switch (material) {
            case Material::Stone: color = {112, 122, 138, 255}; break;
            case Material::Soil: {
                const double wet = world.ground(x, y).moisture;
                color = {static_cast<std::uint8_t>(130 - 60 * wet), static_cast<std::uint8_t>(89 - 40 * wet),
                    static_cast<std::uint8_t>(52 - 20 * wet), 255};
                break;
            }
            case Material::Sand: color = {231, 193, 93, 255}; break;
            case Material::Water: color = {49, 143, 226, 255}; break;
            default: break;
            }
            if (world.gas(x, y) > 0) {
                auto gasCell = cell;
                gasCell.height = 1 - world.water(x, y);
                if (gasCell.height > 0) quad(gasCell, {179, 141, 206,
                    static_cast<std::uint8_t>(std::clamp(world.gasPressure(x, y) * 180, 0.0, 230.0))});
            }
            if (material == Material::Gas) continue;
            if (material == Material::Water) {
                cell.height = std::max(1.0f / scale, static_cast<float>(world.water(x, y)));
                cell.y += 1 - cell.height;
            }
            quad(cell, color);
            if (world.exposedSoil(x, y)) {
                const auto& g = world.ground(x, y);
                const float height = static_cast<int>(g.grass * (1 - 0.7f * g.flattened) * 5) / static_cast<float>(scale);
                if (height > 0) quad({static_cast<float>(x), y - height, 1, height}, {74, 177, 74, 255});
            }
        }
    }
}
void GameRenderer::draw(const Sandbox& game, bool present, int movement, double animationSeconds) {
    animationSeconds_ = animationSeconds;
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer_, 19, 27, 43, 255);
    SDL_RenderClear(renderer_);
    const auto commands = game.scene.drawList();
    auto layer = [&](pixel::RenderLayer target, bool beforeAnchor = false) {
        if (!layerVisible[static_cast<std::size_t>(target)]) return;
        for (const auto& command : commands) {
            const auto& visual = command.visual;
            if (visual.layer != target) continue;
            if ((target == pixel::RenderLayer::Interactables || target == pixel::RenderLayer::Characters) && (visual.order < 0) != beforeAnchor) continue;
            if (visual.sprite) sprite(visual);
            else quad(visual.bounds, visual.color, visual.parallax);
        }
    };
    layer(pixel::RenderLayer::Background);
    layer(pixel::RenderLayer::Interactables, true);
    if (layerVisible[1]) terrain(game.world); // Terrain occupies order=0 in the interaction layer.
    layer(pixel::RenderLayer::Interactables);
    layer(pixel::RenderLayer::Characters, true);
    const auto player = frameCamera().worldToScreen({game.x, game.y});
    if (layerVisible[2]) drawPlayer(renderer_, player.x, player.y, game.grounded, game.walked, movement, scale);
    layer(pixel::RenderLayer::Characters);
    if (showForeground) layer(pixel::RenderLayer::Foreground);
    if (showHints) interactionHint(game);
    if (present) SDL_RenderPresent(renderer_);
}
