#include "pixel/scene.hpp"
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
namespace pixel {
namespace {
void rect(std::ostream& out, Rect r) { out << r.x << ' ' << r.y << ' ' << r.width << ' ' << r.height << ' '; }
Rect rect(std::istream& in) { Rect r; if (!(in >> r.x >> r.y >> r.width >> r.height)) throw std::runtime_error("Invalid scene rectangle"); return r; }
bool flag(std::istream& in) { int v = -1; if (!(in >> v) || (v != 0 && v != 1)) throw std::runtime_error("Invalid scene flag"); return v != 0; }
std::size_t count(std::istream& in, std::size_t limit) { std::size_t n; if (!(in >> n) || n > limit) throw std::runtime_error("Scene count exceeds limit"); return n; }
std::string string(std::istream& in, std::size_t limit) {
    in >> std::ws;
    if (in.get() != '"') throw std::runtime_error("Invalid scene string");
    std::string s;
    for (;;) {
        int c = in.get();
        if (c == std::char_traits<char>::eof()) throw std::runtime_error("Truncated scene string");
        if (c == '"') break;
        if (c == '\\') c = in.get();
        if (c == std::char_traits<char>::eof() || c == 0 || s.size() >= limit) throw std::runtime_error("Invalid scene string");
        s += static_cast<char>(c);
    }
    return s;
}
}
void Scene::save(std::ostream& output) const {
    std::ostringstream out; out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<float>::max_digits10) << "PXSCENE 1 " << size() << '\n';
    for (const auto& o : objects_) {
        out << o.position.x << ' ' << o.position.y << ' ' << std::quoted(o.name) << ' ' << std::quoted(o.action) << ' '
            << o.state << ' ' << o.active << ' ' << o.visuals.size() << '\n';
        for (const auto& v : o.visuals) {
            out << static_cast<int>(v.layer) << ' ' << v.order << ' '; rect(out, v.bounds);
            out << int(v.color.r) << ' ' << int(v.color.g) << ' ' << int(v.color.b) << ' ' << int(v.color.a) << ' '
                << v.parallax << ' ' << v.visible << ' ' << bool(v.sprite) << ' ';
            if (v.sprite) {
                out << std::quoted(v.sprite->asset) << ' '; rect(out, v.sprite->source);
                out << bool(v.sprite->animation) << ' ';
                if (v.sprite->animation) {
                    const auto& a = *v.sprite->animation;
                    out << a.looping() << ' ' << a.frames().size() << ' ';
                    for (const auto& f : a.frames()) { rect(out, f.source); out << f.durationMs << ' '; }
                }
            }
            out << '\n';
        }
        out << bool(o.collider) << ' ';
        if (o.collider) { rect(out, o.collider->bounds); out << o.collider->groups << ' ' << o.collider->plane << ' ' << o.collider->enabled << ' '; }
        out << bool(o.interaction) << ' ';
        if (o.interaction) { rect(out, o.interaction->bounds); out << o.interaction->reach << ' ' << o.interaction->plane << ' ' << o.interaction->enabled; }
        out << '\n';
    }
    output << out.str();
    if (!output) throw std::runtime_error("Cannot write scene");
}
Scene Scene::load(std::istream& input) {
    // Parse with a local classic locale without changing the caller's stream locale.
    std::string bytes;
    char buffer[4096];
    while (input) {
        input.read(buffer, sizeof(buffer)); bytes.append(buffer, static_cast<std::size_t>(input.gcount()));
        if (bytes.size() > 16 * 1024 * 1024) throw std::runtime_error("Scene too large");
    }
    if (input.bad()) throw std::runtime_error("Cannot read scene");
    std::istringstream in(bytes); in.imbue(std::locale::classic());
    std::string magic; int version;
    if (!(in >> magic >> version) || magic != "PXSCENE" || version != 1) throw std::runtime_error("Invalid scene version");
    const auto n = count(in, 4096); Scene result;
    for (std::size_t i = 0; i < n; ++i) {
        SceneObject o;
        if (!(in >> o.position.x >> o.position.y)) throw std::runtime_error("Invalid object position");
        o.name = string(in, 256); o.action = string(in, 64); o.state = flag(in); o.active = flag(in);
        const auto visuals = count(in, 128);
        for (std::size_t j = 0; j < visuals; ++j) {
            Visual v; int layer;
            if (!(in >> layer >> v.order) || layer < 0 || layer > 3) throw std::runtime_error("Invalid layer");
            v.layer = static_cast<RenderLayer>(layer); v.bounds = rect(in);
            int c[4]; for (auto& channel : c) if (!(in >> channel) || channel < 0 || channel > 255) throw std::runtime_error("Invalid color");
            v.color = {std::uint8_t(c[0]), std::uint8_t(c[1]), std::uint8_t(c[2]), std::uint8_t(c[3])};
            if (!(in >> v.parallax)) throw std::runtime_error("Invalid parallax");
            v.visible = flag(in);
            if (flag(in)) {
                Sprite s; s.asset = string(in, 512); s.source = rect(in);
                if (flag(in)) {
                    const bool loop = flag(in); const auto frames = count(in, 256);
                    std::vector<AnimationFrame> f;
                    for (std::size_t k = 0; k < frames; ++k) { auto source = rect(in); std::uint32_t ms; if (!(in >> ms)) throw std::runtime_error("Invalid duration"); f.push_back({source, ms}); }
                    s.animation = AnimationClip(std::move(f), loop);
                }
                v.sprite = std::move(s);
            }
            o.visuals.push_back(std::move(v));
        }
        if (flag(in)) { Collider c; c.bounds = rect(in); if (!(in >> c.groups >> c.plane)) throw std::runtime_error("Invalid collider"); c.enabled = flag(in); o.collider = c; }
        if (flag(in)) { InteractionArea a; a.bounds = rect(in); if (!(in >> a.reach >> a.plane)) throw std::runtime_error("Invalid interaction"); a.enabled = flag(in); o.interaction = a; }
        result.add(std::move(o));
    }
    in >> std::ws; if (!in.eof()) throw std::runtime_error("Unexpected scene data");
    return result;
}
}
