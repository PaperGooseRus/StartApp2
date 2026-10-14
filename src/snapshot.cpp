#include "pixel/world.hpp"
#include <cmath>
#include <cstring>
#include <istream>
#include <limits>
#include <ostream>
#include <stdexcept>

namespace pixel {
namespace {
constexpr std::size_t maxSnapshotCells = 1024 * 1024;
constexpr std::uint64_t offset = 14695981039346656037ull, prime = 1099511628211ull;
static_assert(sizeof(float) == 4 && sizeof(double) == 8 &&
              std::numeric_limits<float>::is_iec559 && std::numeric_limits<double>::is_iec559,
              "Snapshots require IEEE 754 floating point");
class Writer {
public:
    explicit Writer(std::ostream& stream) : stream_(stream) {}
    void number(std::uint64_t value, unsigned bytes, bool hash = true) {
        for (unsigned i = 0; i < bytes; ++i) {
            const auto byte = static_cast<unsigned char>(value >> (i * 8));
            stream_.put(static_cast<char>(byte));
            if (hash) checksum = (checksum ^ byte) * prime;
        }
        if (!stream_) throw std::runtime_error("Cannot write world snapshot");
    }
    void real(double value) {
        std::uint64_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        number(bits, 8);
    }
    void real(float value) {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        number(bits, 4);
    }
    std::uint64_t checksum = offset;
private:
    std::ostream& stream_;
};
class Reader {
public:
    explicit Reader(std::istream& stream) : stream_(stream) {}
    std::uint64_t number(unsigned bytes, bool hash = true) {
        std::uint64_t result = 0;
        for (unsigned i = 0; i < bytes; ++i) {
            const auto c = stream_.get();
            if (c == std::char_traits<char>::eof()) throw std::runtime_error("Truncated world snapshot");
            const auto byte = static_cast<unsigned char>(c);
            result |= static_cast<std::uint64_t>(byte) << (i * 8);
            if (hash) checksum = (checksum ^ byte) * prime;
        }
        return result;
    }
    double real64() {
        const auto bits = number(8);
        double result;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }
    float real32() {
        const auto bits = static_cast<std::uint32_t>(number(4));
        float result;
        std::memcpy(&result, &bits, sizeof(result));
        return result;
    }
    std::uint64_t checksum = offset;
private:
    std::istream& stream_;
};
bool unit(double value) { return std::isfinite(value) && value >= 0 && value <= 1; }
}
void World::save(std::ostream& output) const {
    if (cells_.size() > maxSnapshotCells) throw std::runtime_error("Snapshot limit: 1048576 cells");
    Writer writer(output);
    for (char c : "PXWORLD") writer.number(static_cast<unsigned char>(c), 1); // Includes NUL.
    writer.number(2, 4);
    writer.number(static_cast<std::uint32_t>(width_), 4);
    writer.number(static_cast<std::uint32_t>(height_), 4);
    writer.number(ticks_, 8);
    for (std::size_t i = 0; i < cells_.size(); ++i) {
        writer.number(static_cast<unsigned char>(cells_[i]), 1);
        writer.real(water_[i]);
        writer.real(gas_[i]);
        writer.real(ground_[i].grass);
        writer.real(ground_[i].flattened);
        writer.real(ground_[i].compaction);
        writer.real(ground_[i].moisture);
    }
    writer.number(writer.checksum, 8, false);
}
World World::load(std::istream& input) {
    Reader reader(input);
    for (char c : "PXWORLD")
        if (reader.number(1) != static_cast<unsigned char>(c)) throw std::runtime_error("Not a world snapshot");
    const auto version = reader.number(4);
    if (version != 1 && version != 2) throw std::runtime_error("Unsupported world snapshot version");
    const auto width = reader.number(4), height = reader.number(4);
    if (width == 0 || height == 0 || width > 4096 || height > 4096 || width * height > maxSnapshotCells)
        throw std::runtime_error("Invalid snapshot dimensions");
    const auto ticks = reader.number(8);
    World result(static_cast<int>(width), static_cast<int>(height));
    result.ticks_ = ticks;
    for (std::size_t i = 0; i < result.cells_.size(); ++i) {
        const auto m = reader.number(1);
        const auto water = reader.real64(), gas = reader.real64();
        Ground ground;
        ground.grass = reader.real32();
        ground.flattened = reader.real32();
        ground.compaction = reader.real32();
        ground.moisture = version == 1 ? static_cast<double>(reader.real32()) : reader.real64();
        if (m > static_cast<unsigned char>(Material::Sand) || !std::isfinite(water) || water < 0 || water > 1 ||
            !std::isfinite(gas) || gas < 0 || gas > 1e6 * result.cells_.size() ||
            (m != static_cast<unsigned char>(Material::Air) && (water != 0 || gas != 0)) ||
            !unit(ground.grass) || !unit(ground.flattened) || !unit(ground.compaction) || !unit(ground.moisture))
            throw std::runtime_error("Invalid cell state in snapshot");
        result.cells_[i] = static_cast<Material>(m);
        result.water_[i] = water;
        result.gas_[i] = gas;
        result.ground_[i] = ground;
    }
    const auto expected = reader.checksum;
    if (reader.number(8, false) != expected) throw std::runtime_error("World snapshot checksum mismatch");
    if (input.rdbuf()->sgetc() != std::char_traits<char>::eof() || input.bad())
        throw std::runtime_error("Unexpected data after world snapshot");
    return result;
}
}
