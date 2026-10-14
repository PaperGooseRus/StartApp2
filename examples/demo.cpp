#include "pixel/world.hpp"
#include <iostream>

int main() {
    using pixel::Material;
    pixel::World world(32, 16);
    for (int x = 0; x < world.width(); ++x) world.set(x, 15, Material::Soil);
    for (int y = 0; y < 6; ++y) {
        world.set(8, y, Material::Sand);
        world.set(20, y, Material::Water);
    }
    world.set(25, 13, Material::Gas);
    for (int i = 0; i < 30; ++i) world.stepOn(12, 15);
    for (int i = 0; i < 120; ++i) world.tick();
    const char symbols[] = {' ', '#', '=', ':', '~', '^'};
    for (int y = 0; y < world.height(); ++y) {
        for (int x = 0; x < world.width(); ++x) {
            const auto m = world.material(x, y);
            std::cout << (m == Material::Soil && world.ground(x, y).grass < 0.2f
                          ? '.' : symbols[static_cast<int>(m)]);
        }
        std::cout << '\n';
    }
    std::cout << "Path grass: " << world.ground(12, 15).grass << '\n';
}
