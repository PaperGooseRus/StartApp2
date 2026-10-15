#include "pixel/image.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 4096
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#include "../third_party/stb_image.h"
#include "../third_party/stb_image_write.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
namespace pixel {
namespace {
bool equal(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }
}
Image::Image(int w, int h, Color fill) : width_(w), height_(h) {
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096 || static_cast<std::uint64_t>(w) * h > 4 * 1024 * 1024)
        throw std::invalid_argument("Image size exceeds limits");
    pixels_.assign(static_cast<std::size_t>(w) * h, fill);
}
Color& Image::at(int x, int y) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) throw std::out_of_range("Pixel outside image");
    return pixels_[static_cast<std::size_t>(y) * width_ + x];
}
const Color& Image::at(int x, int y) const { return const_cast<Image*>(this)->at(x, y); }
bool Image::operator==(const Image& o) const noexcept {
    return width_ == o.width_ && height_ == o.height_ && std::equal(pixels_.begin(), pixels_.end(), o.pixels_.begin(), equal);
}
Image Image::loadPng(const std::filesystem::path& file) {
    const auto size = std::filesystem::file_size(file);
    if (size == 0 || size > 32 * 1024 * 1024) throw std::runtime_error("PNG file exceeds limits");
    std::ifstream input(file, std::ios::binary);
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) throw std::runtime_error("Cannot read PNG");
    int w, h, c;
    if (!stbi_info_from_memory(bytes.data(), int(bytes.size()), &w, &h, &c)) throw std::runtime_error("Invalid PNG header");
    Image image(w, h);
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(stbi_load_from_memory(bytes.data(), int(bytes.size()), &w, &h, &c, 4), stbi_image_free);
    if (!pixels) throw std::runtime_error("Cannot decode PNG");
    static_assert(sizeof(Color) == 4, "RGBA layout required");
    std::memcpy(image.pixels_.data(), pixels.get(), image.pixels_.size() * 4);
    return image;
}
void writeAtomic(const std::filesystem::path& file, const std::vector<unsigned char>& bytes) {
    if (!file.parent_path().empty()) std::filesystem::create_directories(file.parent_path());
    auto temporary = file;
    temporary += ".tmp-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    try {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out || !out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) throw std::runtime_error("Cannot write file");
        out.flush(); out.close(); if (!out) throw std::runtime_error("Cannot finish file");
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::system_error(int(GetLastError()), std::system_category(), "Cannot replace file");
#else
        std::filesystem::rename(temporary, file);
#endif
    } catch (...) { std::error_code ignored; std::filesystem::remove(temporary, ignored); throw; }
}
void Image::savePng(const std::filesystem::path& file) const {
    struct Output { std::vector<unsigned char> bytes; bool failed = false; } output;
    // Do not let C++ allocation exceptions escape through the C callback.
    const auto callback = [](void* context, void* data, int count) {
        auto& out = *static_cast<Output*>(context);
        try { const auto* p = static_cast<unsigned char*>(data); out.bytes.insert(out.bytes.end(), p, p + count); }
        catch (...) { out.failed = true; }
    };
    if (!stbi_write_png_to_func(callback, &output, width_, height_, 4, pixels_.data(), width_ * 4) || output.failed)
        throw std::runtime_error("Cannot encode PNG");
    writeAtomic(file, output.bytes);
}
void Image::line(int x0, int y0, int x1, int y1, Color c, int radius) {
    // Canvas callers provide pixel coordinates; reject extreme endpoints before Bresenham.
    if (x0 < 0 || x0 >= width_ || x1 < 0 || x1 >= width_ || y0 < 0 || y0 >= height_ || y1 < 0 || y1 >= height_ || radius < 0 || radius > 16)
        throw std::invalid_argument("Invalid brush stroke");
    const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        for (int y = std::max(0, y0 - radius); y <= std::min(height_ - 1, y0 + radius); ++y)
            for (int x = std::max(0, x0 - radius); x <= std::min(width_ - 1, x0 + radius); ++x) at(x, y) = c;
        if (x0 == x1 && y0 == y1) break;
        const int e = 2 * error;
        if (e >= dy) { error += dy; x0 += sx; }
        if (e <= dx) { error += dx; y0 += sy; }
    }
}
void Image::floodFill(int x, int y, Color c) {
    const Color from = at(x, y); if (equal(from, c)) return;
    std::vector<std::pair<int, int>> stack{{x, y}}; at(x, y) = c;
    while (!stack.empty()) {
        const auto p = stack.back(); stack.pop_back();
        for (auto d : {std::pair<int,int>{-1, 0}, {1, 0}, {0, -1}, {0, 1}}) {
            const int nx = p.first + d.first, ny = p.second + d.second;
            if (nx >= 0 && ny >= 0 && nx < width_ && ny < height_ && equal(at(nx, ny), from)) { at(nx, ny) = c; stack.emplace_back(nx, ny); }
        }
    }
}
}
