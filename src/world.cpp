#include "pixel/world.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <numeric>

namespace pixel {
World::World(int width, int height) : width_(width), height_(height) {
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096)
        throw std::invalid_argument("World dimensions must be in [1, 4096]");
    const auto size = static_cast<std::size_t>(width) * height;
    cells_.assign(size, Material::Air);
    water_.assign(size, 0);
    gas_.assign(size, 0);
    ground_.resize(size);
    moved_.resize(size);
}
bool World::inside(int x, int y) const noexcept {
    return x >= 0 && y >= 0 && x < width_ && y < height_;
}
std::size_t World::index(int x, int y) const {
    if (!inside(x, y)) throw std::out_of_range("Cell outside world");
    return static_cast<std::size_t>(y) * width_ + x;
}
Material World::material(int x, int y) const {
    const auto i = index(x, y);
    if (cells_[i] != Material::Air) return cells_[i];
    if (water_[i] > 0) return Material::Water;
    return gas_[i] > 0 ? Material::Gas : Material::Air;
}
double World::water(int x, int y) const { return water_[index(x, y)]; }
double World::gas(int x, int y) const { return gas_[index(x, y)]; }
double World::gasPressure(int x, int y) const {
    const auto i = index(x, y);
    return gas_[i] / std::max(0.01, 1.0 - water_[i]);
}
void World::setFluid(int x, int y, Material m, double amount) {
    index(x, y);
    if ((m != Material::Water && m != Material::Gas) || !std::isfinite(amount) ||
        amount < 0 || amount > (m == Material::Water ? 1.0 : 1e6))
        throw std::invalid_argument("Invalid fluid type or amount");
    set(x, y, Material::Air);
    (m == Material::Water ? water_ : gas_)[index(x, y)] = amount;
}
void World::set(int x, int y, Material material) {
    const auto i = index(x, y);
    if (material < Material::Air || material > Material::Gas)
        throw std::invalid_argument("Unknown material");
    cells_[i] = (material == Material::Water || material == Material::Gas) ? Material::Air : material;
    water_[i] = material == Material::Water ? 1.0 : 0.0;
    gas_[i] = material == Material::Gas ? 1.0 : 0.0;
    ground_[i] = Ground{};
}
const Ground& World::ground(int x, int y) const { return ground_[index(x, y)]; }
void World::stepOn(int x, int y, float strength) {
    const auto i = index(x, y);
    if (!std::isfinite(strength) || strength < 0)
        throw std::invalid_argument("Contact strength must be finite and nonnegative");
    if (cells_[i] != Material::Soil) return;
    auto& g = ground_[i];
    strength = std::min(strength, 10.0f);
    g.flattened = std::min(1.0f, g.flattened + strength * 0.25f);
    g.compaction = std::min(1.0f, g.compaction + strength * 0.025f * static_cast<float>(0.5 + 1.5 * g.moisture));
    g.grass = std::max(0.0f, g.grass - strength * 0.04f * static_cast<float>(0.75 + 0.75 * g.moisture));
}
bool World::move(int x, int y, int nx, int ny) {
    if (!inside(nx, ny)) return false; // Closed boundaries.
    const auto from = index(x, y), to = index(nx, ny);
    if (moved_[from] || moved_[to]) return false;
    if (cells_[from] != Material::Sand || cells_[to] != Material::Air) return false;
    std::swap(cells_[from], cells_[to]);
    std::swap(water_[from], water_[to]);
    std::swap(gas_[from], gas_[to]);
    moved_[from] = moved_[to] = true;
    return true;
}
void World::transferWater(std::size_t from, std::size_t to, double amount) {
    if (cells_[from] != Material::Air || cells_[to] != Material::Air) return;
    amount = std::clamp(amount, 0.0, std::min(water_[from], 1.0 - water_[to]));
    if (amount <= 0) return;
    // Incoming water displaces the gas in the volume it occupies, rather than deleting it.
    const double displaced = gas_[to] * (amount / (1.0 - water_[to]));
    gas_[to] -= displaced;
    gas_[from] += displaced;
    water_[from] -= amount;
    water_[to] += amount;
}
void World::exchangeGas(std::size_t a, std::size_t b, bool vertical) {
    if (cells_[a] != Material::Air || cells_[b] != Material::Air) return;
    const double va = 1.0 - water_[a], vb = 1.0 - water_[b];
    if (va <= 0 || vb <= 0) return;
    // a is above b. This modest bias models a light gas, not atmospheric air.
    const double bias = vertical ? 1.1 : 1.0;
    const double targetA = (gas_[a] + gas_[b]) * bias * va / (bias * va + vb);
    double flow = 0.25 * (targetA - gas_[a]);
    flow = std::clamp(flow, -gas_[a], gas_[b]);
    gas_[a] += flow;
    gas_[b] -= flow;
}
void World::tick() {
    std::fill(moved_.begin(), moved_.end(), false);
    const int direction = (ticks_ % 2 == 0) ? 1 : -1;
    for (int y = height_ - 1; y >= 0; --y) {
        for (int n = 0; n < width_; ++n) {
            const int x = direction == 1 ? n : width_ - 1 - n;
            if (cells_[index(x, y)] != Material::Sand) continue;
            if (move(x, y, x, y + 1)) continue;
            if (move(x, y, x + direction, y + 1)) continue;
            move(x, y, x - direction, y + 1);
        }
    }
    // Incompressible water: fill lower cells first, then equalize neighbours.
    for (int y = height_ - 2; y >= 0; --y) {
        for (int x = 0; x < width_; ++x) {
            const auto a = index(x, y), b = index(x, y + 1);
            transferWater(a, b, water_[a]);
        }
    }
    for (int y = 0; y < height_; ++y) {
        for (int n = 0; n < width_ - 1; ++n) {
            const int x = direction == 1 ? n : width_ - 2 - n;
            const auto a = index(x, y), b = index(x + 1, y);
            const double difference = (water_[a] - water_[b]) * 0.5;
            if (difference > 0) transferWater(a, b, difference);
            else transferWater(b, a, -difference);
        }
    }
    for (int y = 0; y < height_; ++y) {
        for (int n = 0; n < width_; ++n) {
            const int x = direction == 1 ? n : width_ - 1 - n;
            const auto a = index(x, y);
            if (y + 1 < height_) exchangeGas(a, index(x, y + 1), true);
            if (x + 1 < width_) exchangeGas(a, index(x + 1, y), false);
        }
    }
    updateSoil();
    ++ticks_;
}
std::size_t World::count(Material material) const noexcept {
    if (material != Material::Water && material != Material::Gas && material != Material::Air)
        return static_cast<std::size_t>(std::count(cells_.begin(), cells_.end(), material));
    std::size_t result = 0;
    for (std::size_t i = 0; i < cells_.size(); ++i) {
        if (material == Material::Water && water_[i] > 0) ++result;
        if (material == Material::Gas && gas_[i] > 0) ++result;
        if (material == Material::Air && cells_[i] == Material::Air && water_[i] == 0 && gas_[i] == 0) ++result;
    }
    return result;
}
double World::mass(Material m) const noexcept {
    if (m == Material::Water) return freeWaterMass() + absorbedWaterMass();
    if (m == Material::Gas) return std::accumulate(gas_.begin(), gas_.end(), 0.0);
    return static_cast<double>(count(m));
}
}
