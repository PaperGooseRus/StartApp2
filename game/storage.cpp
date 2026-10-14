#include "storage.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace storage {
namespace {
std::uint64_t checksum(const std::string& bytes) {
    std::uint64_t value = 14695981039346656037ull;
    for (unsigned char c : bytes) value = (value ^ c) * 1099511628211ull;
    return value;
}
bool validCharacter(float x, float y, float velocity, float walked) {
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(velocity) && std::isfinite(walked) &&
           x >= 0 && x <= Sandbox::width - 2 && y >= 0 && y <= Sandbox::height - 3 &&
           velocity >= -22 && velocity <= 35 && walked >= 0 && walked <= 1.21f;
}
}
void write(const Sandbox& game, std::ostream& output) {
    if (!validCharacter(game.x, game.y, game.velocityY, game.walked) ||
        game.world.width() != Sandbox::width || game.world.height() != Sandbox::height)
        throw std::runtime_error("Cannot save invalid game state");
    std::ostringstream header;
    header.imbue(std::locale::classic());
    header << std::setprecision(std::numeric_limits<float>::max_digits10)
           << "PXSANDBOX 1 " << game.x << ' ' << game.y << ' ' << game.velocityY << ' '
           << game.walked << ' ' << static_cast<int>(game.grounded);
    const auto text = header.str();
    header << ' ' << checksum(text) << '\n';
    output << header.str();
    game.world.save(output);
}
Sandbox read(std::istream& input) {
    std::string line;
    for (;;) {
        const int c = input.get();
        if (c == std::char_traits<char>::eof() || line.size() >= 256)
            throw std::runtime_error("Invalid game save header");
        if (c == '\n') break;
        line += static_cast<char>(c);
    }
    std::istringstream header(line);
    header.imbue(std::locale::classic());
    std::string magic;
    int version, grounded;
    std::uint64_t storedChecksum;
    float x, y, velocity, walked;
    if (!(header >> magic >> version >> x >> y >> velocity >> walked >> grounded >> storedChecksum) ||
        magic != "PXSANDBOX" || version != 1 ||
        !validCharacter(x, y, velocity, walked) || (grounded != 0 && grounded != 1))
        throw std::runtime_error("Invalid character state or game save version");
    const auto checksumStart = line.find_last_of(' ');
    if (checksumStart == std::string::npos || storedChecksum != checksum(line.substr(0, checksumStart)))
        throw std::runtime_error("Character snapshot checksum mismatch");
    header >> std::ws;
    if (!header.eof()) throw std::runtime_error("Unexpected character data");
    auto world = pixel::World::load(input);
    if (world.width() != Sandbox::width || world.height() != Sandbox::height)
        throw std::runtime_error("Save belongs to a different game map size");
    Sandbox game;
    game.world = std::move(world);
    game.x = x;
    game.y = y;
    game.velocityY = velocity;
    game.walked = walked;
    game.grounded = grounded != 0;
    return game;
}
void save(const Sandbox& game, const std::filesystem::path& file) {
    if (!file.parent_path().empty()) std::filesystem::create_directories(file.parent_path());
    auto temporary = file;
    temporary += ".tmp-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    try {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("Cannot create save file");
        write(game, output);
        output.flush();
        output.close();
        if (!output) throw std::runtime_error("Cannot finish writing save file");
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "Cannot replace save file");
#else
        std::filesystem::rename(temporary, file);
#endif
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}
Sandbox load(const std::filesystem::path& file) {
    // Bound input size before allocating or parsing it.
    if (std::filesystem::file_size(file) > 40 * 1024 * 1024)
        throw std::runtime_error("Save file is too large");
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open save file");
    return read(input);
}
}
