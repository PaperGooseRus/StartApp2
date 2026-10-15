#include "pixel/animation.hpp"
#include <cmath>
#include <stdexcept>
namespace pixel {
AnimationClip::AnimationClip(std::vector<AnimationFrame> frames, bool loop) : frames_(std::move(frames)), loop_(loop) {
    if (frames_.empty() || frames_.size() > 256) throw std::invalid_argument("Invalid animation frame count");
    for (const auto& frame : frames_) {
        const auto r = frame.source;
        if (frame.durationMs < 1 || frame.durationMs > 60000 || !std::isfinite(r.x) || !std::isfinite(r.y) ||
            !std::isfinite(r.width) || !std::isfinite(r.height) || r.x < 0 || r.y < 0 || r.width <= 0 || r.height <= 0 ||
            r.x + r.width > 1.000001f || r.y + r.height > 1.000001f)
            throw std::invalid_argument("Invalid animation frame");
        duration_ += frame.durationMs;
    }
}
std::size_t AnimationClip::frameAt(double seconds) const {
    if (!std::isfinite(seconds) || seconds < 0) throw std::invalid_argument("Invalid animation time");
    const double durationSeconds = static_cast<double>(duration_) / 1000;
    if (!loop_ && seconds >= durationSeconds) return frames_.size() - 1;
    double time = loop_ ? std::fmod(seconds, durationSeconds) : seconds;
    // Work in seconds to avoid overflowing integer conversion for long running games.
    std::uint64_t end = 0;
    for (std::size_t i = 0; i < frames_.size(); ++i) {
        end += frames_[i].durationMs;
        if (time < static_cast<double>(end) / 1000 - 1e-12) return i;
    }
    return loop_ ? 0 : frames_.size() - 1;
}
}
