#pragma once
#include <cstdint>
namespace pixel {
struct Vec2 { float x = 0, y = 0; };
struct Rect { float x = 0, y = 0, width = 1, height = 1; };
struct Color { std::uint8_t r = 255, g = 255, b = 255, a = 255; };
}
