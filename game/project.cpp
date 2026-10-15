#include "project.hpp"
#include "storage.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
namespace studio {
namespace {
constexpr std::size_t limit = 96 * 1024 * 1024;
using Bytes = std::vector<unsigned char>;
void integer(Bytes& b, std::uint64_t n, int count = 4) { for (int i = 0; i < count; ++i) b.push_back(static_cast<unsigned char>(n >> (i * 8))); }
void string(Bytes& b, const std::string& s) { integer(b, s.size()); b.insert(b.end(), s.begin(), s.end()); }
std::uint64_t checksum(const Bytes& b) { std::uint64_t h = 14695981039346656037ull; for (auto c : b) h = (h ^ c) * 1099511628211ull; return h; }
struct Reader {
    const Bytes& bytes; std::size_t pos = 0;
    std::uint64_t integer(int n = 4) {
        if (pos + n > bytes.size()) throw std::runtime_error("Truncated project");
        std::uint64_t v = 0; for (int i = 0; i < n; ++i) v |= std::uint64_t(bytes[pos++]) << (i * 8); return v;
    }
    std::string string(std::size_t maximum) {
        const auto size = integer(); if (size > maximum || size > bytes.size() - pos) throw std::runtime_error("Invalid project string size");
        std::string s(reinterpret_cast<const char*>(bytes.data() + pos), static_cast<std::size_t>(size)); pos += static_cast<std::size_t>(size); return s;
    }
};
}
void SpriteArt::validate() const {
    if (id == 0 || name.empty() || name.size() > 128 || frames.empty() || frames.size() > 32 || durations.size() != frames.size()) throw std::runtime_error("Invalid sprite asset");
    const int w = frames[0].width(), h = frames[0].height();
    if (w > 128 || h > 128) throw std::runtime_error("Canvas supports sprites up to 128x128");
    for (std::size_t i = 0; i < frames.size(); ++i)
        if (frames[i].width() != w || frames[i].height() != h || durations[i] < 1 || durations[i] > 60000) throw std::runtime_error("Invalid sprite frame");
}
pixel::Image SpriteArt::atlas() const {
    validate(); const int w = frames[0].width(), h = frames[0].height();
    pixel::Image result(w * int(frames.size()), h);
    for (std::size_t i = 0; i < frames.size(); ++i)
        for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) result.at(int(i) * w + x, y) = frames[i].at(x, y);
    return result;
}
pixel::AnimationClip SpriteArt::clip() const {
    validate(); std::vector<pixel::AnimationFrame> out;
    for (std::size_t i = 0; i < frames.size(); ++i) out.push_back({{float(i) / frames.size(), 0, 1.0f / frames.size(), 1}, durations[i]});
    return pixel::AnimationClip(std::move(out), loop);
}
void Project::validate() const {
    if (name.empty() || name.size() > 128 || art.size() > 32 || nextArtId == 0 || game.scene.size() > 4096 ||
        game.gate >= game.scene.size() || game.chest >= game.scene.size()) throw std::runtime_error("Invalid project");
    std::set<std::uint32_t> ids; std::set<std::string> keys;
    for (const auto& a : art) { a.validate(); if (a.id >= nextArtId || !ids.insert(a.id).second) throw std::runtime_error("Duplicate sprite ID"); keys.insert(a.key()); }
    for (std::size_t i = 0; i < game.scene.size(); ++i) {
        const auto& o = game.scene.object(static_cast<pixel::ObjectId>(i));
        if (o.action != "" && o.action != "door" && o.action != "chest") throw std::runtime_error("Unknown object action");
        if (!o.active) continue;
        for (const auto& v : o.visuals) if (v.sprite) {
            const auto& key = v.sprite->asset;
            if (key.rfind("@art/", 0) == 0) { if (!keys.count(key)) throw std::runtime_error("Missing embedded sprite"); }
            else if (key != "oak.png") throw std::runtime_error("Project references an unavailable sprite");
        }
    }
}
void Project::refreshSprites() {
    for (std::size_t i = 0; i < game.scene.size(); ++i) {
        auto o = game.scene.object(static_cast<pixel::ObjectId>(i)); bool changed = false;
        for (auto& v : o.visuals) if (v.sprite) for (const auto& a : art) if (v.sprite->asset == a.key()) {
            v.sprite->animation = a.clip(); v.sprite->source = v.sprite->animation->frames()[0].source; changed = true;
        }
        if (changed) game.scene.replace(static_cast<pixel::ObjectId>(i), std::move(o));
    }
}
Project Project::example() {
    Project p; SpriteArt a; a.id = p.nextArtId++; a.name = "Огонёк";
    a.frames.clear(); a.durations = {140, 100, 180, 100};
    for (int f = 0; f < 4; ++f) {
        pixel::Image im(16, 16);
        for (int y = 3; y < 14; ++y) for (int x = 3; x < 13; ++x) {
            const int dx = x - 8, dy = y - (9 - f % 2);
            if (dx * dx + dy * dy < 23 + f * 2) im.at(x, y) = {255, static_cast<std::uint8_t>(145 + f * 12), 46, 255};
            if (dx * dx + (dy - 1) * (dy - 1) < 7) im.at(x, y) = {255, 239, 144, 255};
        }
        a.frames.push_back(std::move(im));
    }
    p.art.push_back(a);
    pixel::SceneObject object; object.name = "Анимированный огонёк"; object.position = {28, 50};
    pixel::Visual v{pixel::RenderLayer::Interactables, 30, {0, 0, 3, 3}};
    v.sprite = pixel::Sprite{a.key(), {0, 0, 1, 1}, a.clip()}; object.visuals.push_back(v); p.game.scene.add(object);
    p.refreshSprites(); return p;
}
void save(const Project& project, const std::filesystem::path& path) {
    project.validate();
    std::ostringstream game(std::ios::binary), scene;
    storage::write(project.game, game); project.game.scene.save(scene);
    Bytes payload; string(payload, project.name); integer(payload, project.nextArtId);
    integer(payload, project.game.gate); integer(payload, project.game.chest);
    string(payload, game.str()); string(payload, scene.str()); integer(payload, project.art.size());
    for (const auto& a : project.art) {
        integer(payload, a.id); string(payload, a.name); integer(payload, a.loop ? 1 : 0);
        integer(payload, a.frames[0].width()); integer(payload, a.frames[0].height()); integer(payload, a.frames.size());
        for (std::size_t i = 0; i < a.frames.size(); ++i) {
            integer(payload, a.durations[i]);
            for (const auto c : a.frames[i].pixels()) { payload.push_back(c.r); payload.push_back(c.g); payload.push_back(c.b); payload.push_back(c.a); }
        }
    }
    if (payload.size() > limit) throw std::runtime_error("Project exceeds size limit");
    Bytes bytes{'P','X','I','D','E','0','1','\n'}; integer(bytes, checksum(payload), 8);
    bytes.insert(bytes.end(), payload.begin(), payload.end()); pixel::writeAtomic(path, bytes);
}
Project load(const std::filesystem::path& path) {
    const auto size = std::filesystem::file_size(path);
    if (size < 16 || size > limit + 16) throw std::runtime_error("Invalid project file size");
    std::ifstream in(path, std::ios::binary); Bytes bytes(static_cast<std::size_t>(size));
    if (!in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size))) throw std::runtime_error("Cannot read project");
    if (std::memcmp(bytes.data(), "PXIDE01\n", 8) != 0) throw std::runtime_error("Invalid project version");
    Reader header{bytes, 8}; const auto expected = header.integer(8);
    Bytes payload(bytes.begin() + 16, bytes.end()); if (checksum(payload) != expected) throw std::runtime_error("Project checksum mismatch");
    Reader r{payload}; Project p; p.name = r.string(128); p.nextArtId = static_cast<std::uint32_t>(r.integer());
    const auto gate = r.integer(), chest = r.integer();
    std::istringstream game(r.string(40 * 1024 * 1024), std::ios::binary); p.game = storage::read(game);
    std::istringstream scene(r.string(16 * 1024 * 1024)); p.game.scene = pixel::Scene::load(scene);
    p.game.gate = static_cast<pixel::ObjectId>(gate); p.game.chest = static_cast<pixel::ObjectId>(chest);
    const auto count = r.integer(); if (count > 32) throw std::runtime_error("Too many sprite assets");
    for (std::size_t i = 0; i < count; ++i) {
        SpriteArt a; a.id = static_cast<std::uint32_t>(r.integer()); a.name = r.string(128);
        const auto loop = r.integer(); if (loop > 1) throw std::runtime_error("Invalid loop flag"); a.loop = loop != 0;
        const auto w = r.integer(), h = r.integer(), frames = r.integer();
        if (w < 1 || h < 1 || w > 128 || h > 128 || frames < 1 || frames > 32) throw std::runtime_error("Invalid sprite dimensions");
        a.frames.clear(); a.durations.clear();
        for (std::size_t f = 0; f < frames; ++f) {
            a.durations.push_back(static_cast<std::uint32_t>(r.integer())); pixel::Image im{int(w), int(h)};
            for (int y = 0; y < int(h); ++y) for (int x = 0; x < int(w); ++x) {
                const auto red = r.integer(1), green = r.integer(1), blue = r.integer(1), alpha = r.integer(1);
                im.at(x, y) = {std::uint8_t(red), std::uint8_t(green), std::uint8_t(blue), std::uint8_t(alpha)};
            }
            a.frames.push_back(std::move(im));
        }
        p.art.push_back(std::move(a));
    }
    if (r.pos != payload.size()) throw std::runtime_error("Unexpected project data");
    p.validate(); p.refreshSprites(); return p;
}
SpriteArt importPng(const std::filesystem::path& file, int columns, std::uint32_t id) {
    const auto image = pixel::Image::loadPng(file);
    if (columns < 1 || columns > 32 || image.width() % columns || image.width() / columns > 128 || image.height() > 128)
        throw std::runtime_error("PNG must be a horizontal strip of 1-32 frames, each up to 128x128");
    SpriteArt a; a.id = id; a.name = file.stem().u8string(); if(a.name.empty() || a.name.size()>128) a.name="Импортированный спрайт"; a.frames.clear(); a.durations.clear();
    const int w = image.width() / columns;
    for (int f = 0; f < columns; ++f) {
        pixel::Image im(w, image.height());
        for (int y = 0; y < im.height(); ++y) for (int x = 0; x < w; ++x) im.at(x, y) = image.at(f * w + x, y);
        a.frames.push_back(std::move(im)); a.durations.push_back(120);
    }
    a.validate(); return a;
}
}
