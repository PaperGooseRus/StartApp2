#include "pixel/world.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace pixel {
double World::soilWater(int x, int y) const {
    const auto i = index(x, y);
    return cells_[i] == Material::Soil ? ground_[i].moisture * soilWaterCapacity : 0;
}
double World::freeWaterMass() const noexcept {
    return std::accumulate(water_.begin(), water_.end(), 0.0);
}
double World::absorbedWaterMass() const noexcept {
    double result = 0;
    for (std::size_t i = 0; i < cells_.size(); ++i)
        if (cells_[i] == Material::Soil) result += ground_[i].moisture * soilWaterCapacity;
    return result;
}
void World::setMoisture(int x, int y, double saturation) {
    const auto i = index(x, y);
    if (cells_[i] != Material::Soil || !std::isfinite(saturation) || saturation < 0 || saturation > 1)
        throw std::invalid_argument("Moisture requires soil and a finite saturation in [0, 1]");
    ground_[i].moisture = saturation;
}
bool World::exposedSoil(int x, int y) const {
    const auto i = index(x, y);
    if (cells_[i] != Material::Soil) return false;
    if (y == 0) return true;
    const auto above = index(x, y - 1);
    return cells_[above] == Material::Air && water_[above] <= 0.05;
}
void World::updateSoil() {
    constexpr double dt = 1.0 / 60;
    const int direction = ticks_ % 2 == 0 ? 1 : -1;
    for (int y = 0; y < height_; ++y) {
        for (int n = 0; n < width_; ++n) {
            const int x = direction == 1 ? n : width_ - 1 - n;
            const auto i = index(x, y);
            if (cells_[i] != Material::Soil) continue;
            auto& g = ground_[i];
            const double rate = 0.08 * dt * (1.0 - 0.9 * g.compaction);
            const int dx[] = {0, direction, -direction, 0};
            const int dy[] = {-1, 0, 0, 1};
            for (int k = 0; k < 4; ++k) {
                if (!inside(x + dx[k], y + dy[k])) continue;
                const auto neighbour = index(x + dx[k], y + dy[k]);
                if (cells_[neighbour] != Material::Air) continue;
                const double amount = std::min({water_[neighbour], soilWaterCapacity * (1.0 - g.moisture), rate});
                water_[neighbour] -= amount;
                g.moisture += amount / soilWaterCapacity;
            }
        }
    }
    // Exchange each soil-soil edge once; both sides have identical fixed capacity.
    auto diffuse = [&](std::size_t a, std::size_t b) {
        if (cells_[a] != Material::Soil || cells_[b] != Material::Soil) return;
        auto& ga = ground_[a];
        auto& gb = ground_[b];
        const double permeability = 1.0 - 0.9 * std::max(ga.compaction, gb.compaction);
        const double delta = (ga.moisture - gb.moisture) * 0.3 * dt * permeability;
        ga.moisture -= delta;
        gb.moisture += delta;
    };
    for (int y = 0; y < height_; ++y) {
        for (int n = 0; n < width_; ++n) {
            const int x = direction == 1 ? n : width_ - 1 - n;
            const auto a = index(x, y);
            if (x + 1 < width_) diffuse(a, index(x + 1, y));
            if (y + 1 < height_) diffuse(a, index(x, y + 1));
        }
    }
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const auto i = index(x, y);
            if (cells_[i] != Material::Soil) continue;
            auto& g = ground_[i];
            g.flattened = std::max(0.0f, g.flattened - static_cast<float>(dt * 0.08));
            g.compaction = std::max(0.0f, g.compaction - static_cast<float>(dt * 0.005 * (1.0 - 0.75 * g.moisture)));
            if (!exposedSoil(x, y)) continue;
            // Too dry or waterlogged soil grows slowly; optimum is 60% saturation.
            const double suitability = g.moisture <= 0.6 ? g.moisture / 0.6 : (1.0 - g.moisture) / 0.4;
            g.grass = std::min(1.0f, g.grass + static_cast<float>(dt * 0.03 * suitability *
                              (1.0 - g.compaction) * (1.0 - g.grass)));
        }
    }
}
}
