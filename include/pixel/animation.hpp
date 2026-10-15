#pragma once
#include "geometry.hpp"
#include <cstdint>
#include <vector>

namespace pixel {
struct AnimationFrame {
    Rect source{0, 0, 1, 1};
    std::uint32_t durationMs = 100;
};
class AnimationClip {
public:
    explicit AnimationClip(std::vector<AnimationFrame> frames, bool loop = true);
    const std::vector<AnimationFrame>& frames() const noexcept { return frames_; }
    bool looping() const noexcept { return loop_; }
    std::uint64_t durationMs() const noexcept { return duration_; }
    std::size_t frameAt(double seconds) const;
private:
    std::vector<AnimationFrame> frames_;
    bool loop_;
    std::uint64_t duration_ = 0;
};
}
