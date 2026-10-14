#pragma once
#include <cstdint>
#include <iosfwd>
#include <vector>

namespace pixel {
enum class Material : std::uint8_t { Air, Stone, Soil, Sand, Water, Gas };
struct Ground {
    float grass = 1.0f;
    float flattened = 0.0f;
    float compaction = 0.0f;
    float moisture = 0.6f;
};
// A simulation cell is independent of screen resolution.
class World {
public:
    World(int width, int height);
    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }
    Material material(int x, int y) const;
    void set(int x, int y, Material material);
    // Replaces cell contents. Water amount is [0, 1]; gas mass is [0, 1e6].
    void setFluid(int x, int y, Material material, double amount);
    double water(int x, int y) const;
    double gas(int x, int y) const;
    double gasPressure(int x, int y) const;
    // count() counts occupied cells; mass() tracks conserved fluid quantity.
    double mass(Material material) const noexcept;
    const Ground& ground(int x, int y) const;
    // Foot contact is an event, not a call made every rendering frame.
    void stepOn(int x, int y, float strength = 1.0f);
    // Fixed simulation tick: 1/60 second. Rendering frequency is independent.
    void tick();
    std::size_t count(Material material) const noexcept;
    // Versioned, little-endian snapshot with a corruption checksum.
    // Loading constructs a new world; it never modifies an existing one.
    void save(std::ostream& output) const;
    static World load(std::istream& input);
private:
    std::size_t index(int x, int y) const;
    bool inside(int x, int y) const noexcept;
    bool move(int x, int y, int nx, int ny);
    void transferWater(std::size_t from, std::size_t to, double amount);
    void exchangeGas(std::size_t a, std::size_t b, bool vertical);
    int width_, height_;
    std::uint64_t ticks_ = 0;
    std::vector<Material> cells_;
    std::vector<double> water_, gas_;
    std::vector<Ground> ground_;
    std::vector<bool> moved_;
};
}
