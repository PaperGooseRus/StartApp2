#include "document.hpp"
#include <algorithm>
#include <stdexcept>
namespace studio {
void Document::beginChange() {
    if (runtime) throw std::logic_error("Stop preview before editing");
    if (!before_) before_ = project;
}
void Document::changed() { project.refreshSprites(); ++revision; dirty = true; }
void Document::trim() {
    std::size_t bytes = 0;
    for (auto it = undo_.rbegin(); it != undo_.rend(); ++it) {
        bytes += 1024 * 1024;
        for (const auto& a : it->art) bytes += a.frames.size() * a.frames[0].pixels().size() * sizeof(pixel::Color);
    }
    while (undo_.size() > 1 && (undo_.size() > 12 || bytes > 64 * 1024 * 1024)) {
        bytes -= 1024 * 1024;
        for (const auto& a : undo_.front().art) bytes -= a.frames.size() * a.frames[0].pixels().size() * sizeof(pixel::Color);
        undo_.pop_front();
    }
}
void Document::commit() {
    if (!before_) return;
    undo_.push_back(std::move(*before_)); before_.reset(); redo_.clear(); trim();
}
bool Document::undo() {
    if (runtime) return false;
    commit(); if (undo_.empty()) return false;
    redo_.push_back(std::move(project)); project = std::move(undo_.back()); undo_.pop_back(); changed(); return true;
}
bool Document::redo() {
    if (runtime) return false;
    commit(); if (redo_.empty()) return false;
    undo_.push_back(std::move(project)); project = std::move(redo_.back()); redo_.pop_back(); changed(); trim(); return true;
}
void Document::reset(Project value) {
    value.validate(); value.refreshSprites(); project = std::move(value);
    runtime.reset(); before_.reset(); undo_.clear(); redo_.clear(); ++revision; dirty = false; playTime = 0;
}
void Document::start(bool simulation) { commit(); runtime = project.game; simulationOnly = simulation; paused = false; playTime = 0; }
void Document::stop() { runtime.reset(); paused = false; playTime = 0; }
void Document::step(int direction, bool jump) {
    if (!runtime) return;
    if (simulationOnly) runtime->world.tick(); else runtime->tick(direction, jump);
    playTime += 1.0 / 60;
}
pixel::ObjectId Document::place(const std::string& preset, pixel::Vec2 position, std::size_t artIndex) {
    if (project.game.scene.size() >= 4096) throw std::runtime_error("Scene object limit reached");
    static const Sandbox templates;
    pixel::SceneObject o;
    if (preset == "door") o = templates.scene.object(templates.gate);
    else if (preset == "chest") o = templates.scene.object(templates.chest);
    else if (preset == "tree") {
        for (std::size_t i = 0; i < templates.scene.size(); ++i) {
            const auto& t = templates.scene.object(static_cast<pixel::ObjectId>(i));
            if (!t.visuals.empty() && t.visuals[0].sprite) { o = t; break; }
        }
    } else if (preset == "sprite") {
        const auto& a = project.art.at(artIndex);
        o.name = a.name;
        pixel::Visual v{pixel::RenderLayer::Interactables, 30, {0, 0, a.frames[0].width() / 8.0f, a.frames[0].height() / 8.0f}};
        v.sprite = pixel::Sprite{a.key(), a.clip().frames()[0].source, a.clip()}; o.visuals.push_back(v);
    } else throw std::invalid_argument("Unknown preset");
    o.position = position; beginChange(); const auto id = project.game.scene.add(std::move(o)); changed(); commit(); return id;
}
SpriteArt& Document::addArt(int width, int height) {
    if (width < 1 || width > 128 || height < 1 || height > 128 || project.art.size() >= 32) throw std::invalid_argument("Sprite limit: 32 assets of up to 128x128");
    SpriteArt a; a.id = project.nextArtId; a.frames = {pixel::Image(width, height)};
    a.name = "Спрайт " + std::to_string(a.id); beginChange(); ++project.nextArtId;
    project.art.push_back(std::move(a)); changed(); commit(); return project.art.back();
}
void Document::duplicateFrame(std::size_t a, std::size_t f, bool blank) {
    auto& asset = project.art.at(a); (void)asset.frames.at(f);
    if (asset.frames.size() >= 32) throw std::runtime_error("Animation limit: 32 frames");
    auto image = blank ? pixel::Image(asset.frames[f].width(), asset.frames[f].height()) : asset.frames[f];
    const auto duration = asset.durations[f]; beginChange();
    asset.frames.insert(asset.frames.begin() + static_cast<std::ptrdiff_t>(f + 1), std::move(image));
    asset.durations.insert(asset.durations.begin() + static_cast<std::ptrdiff_t>(f + 1), duration); changed(); commit();
}
void Document::removeFrame(std::size_t a, std::size_t f) {
    auto& asset = project.art.at(a); (void)asset.frames.at(f); if (asset.frames.size() == 1) return;
    beginChange(); asset.frames.erase(asset.frames.begin() + static_cast<std::ptrdiff_t>(f)); asset.durations.erase(asset.durations.begin() + static_cast<std::ptrdiff_t>(f)); changed(); commit();
}
void Document::moveFrame(std::size_t a, std::size_t from, std::size_t to) {
    auto& asset = project.art.at(a); (void)asset.frames.at(from); (void)asset.frames.at(to); if (from == to) return;
    beginChange(); auto image = std::move(asset.frames[from]); auto ms = asset.durations[from];
    asset.frames.erase(asset.frames.begin() + static_cast<std::ptrdiff_t>(from)); asset.durations.erase(asset.durations.begin() + static_cast<std::ptrdiff_t>(from));
    asset.frames.insert(asset.frames.begin() + static_cast<std::ptrdiff_t>(to), std::move(image)); asset.durations.insert(asset.durations.begin() + static_cast<std::ptrdiff_t>(to), ms); changed(); commit();
}
}
