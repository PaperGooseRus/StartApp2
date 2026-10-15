#pragma once
#include "geometry.hpp"
#include <filesystem>
#include <vector>
namespace pixel {
class Image {
public:
    Image(int width = 32, int height = 32, Color fill = {0, 0, 0, 0});
    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }
    Color& at(int x, int y);
    const Color& at(int x, int y) const;
    const std::vector<Color>& pixels() const noexcept { return pixels_; }
    bool operator==(const Image& other) const noexcept;
    bool operator!=(const Image& other) const noexcept { return !(*this == other); }
    static Image loadPng(const std::filesystem::path& path);
    void savePng(const std::filesystem::path& path) const;
    void line(int x0, int y0, int x1, int y1, Color color, int radius = 0);
    void floodFill(int x, int y, Color color);
private:
    int width_, height_;
    std::vector<Color> pixels_;
};
// Writes all bytes first, then atomically replaces the destination file.
void writeAtomic(const std::filesystem::path& path, const std::vector<unsigned char>& bytes);
}
