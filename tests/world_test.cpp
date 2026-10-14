#include "pixel/world.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
#include <cmath>

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int main() {
    using pixel::Material;
    try {
        pixel::World falling(5, 5);
        falling.set(2, 0, Material::Sand);
        falling.tick();
        check(falling.material(2, 1) == Material::Sand, "sand moves one cell per tick");
        for (int x = 0; x < 5; ++x) falling.set(x, 3, Material::Stone);
        for (int i = 0; i < 20; ++i) falling.tick();
        check(falling.material(2, 2) == Material::Sand, "stone stops sand");
        check(falling.count(Material::Sand) == 1, "sand conserved");

        pixel::World water(5, 3);
        for (int x = 0; x < 5; ++x) water.set(x, 2, Material::Stone);
        water.set(2, 1, Material::Water);
        water.tick();
        check(water.material(3, 1) == Material::Water, "water spreads sideways");

        pixel::World gas(3, 5);
        gas.set(1, 4, Material::Gas);
        gas.tick();
        check(gas.material(1, 3) == Material::Gas, "gas rises one cell");

        pixel::World layers(1, 3);
        layers.set(0, 0, Material::Sand);
        layers.set(0, 1, Material::Water);
        layers.set(0, 2, Material::Stone);
        layers.tick();
        check(layers.material(0, 1) == Material::Sand, "sand sinks in water");
        check(layers.material(0, 0) == Material::Water, "displaced water conserved");

        pixel::World mixed(20, 20);
        for (int x = 0; x < 20; ++x) mixed.set(x, 19, Material::Stone);
        for (int y = 0; y < 10; ++y) {
            mixed.set(3, y, Material::Sand);
            mixed.set(8, y, Material::Water);
            mixed.set(13, y, Material::Gas);
        }
        for (int i = 0; i < 1000; ++i) mixed.tick();
        for (auto m : {Material::Sand, Material::Water, Material::Gas})
            check(std::abs(mixed.mass(m) - 10.0) < 1e-8, "mixed simulation conserves mass");
        check(mixed.count(Material::Stone) == 20, "static material stays intact");

        pixel::World level(5, 1);
        level.setFluid(2, 0, Material::Water, 0.75);
        for (int i = 0; i < 300; ++i) level.tick();
        for (int x = 0; x < 5; ++x)
            check(std::abs(level.water(x, 0) - 0.15) < 1e-8, "partial water finds level");
        check(std::abs(level.mass(Material::Water) - 0.75) < 1e-10, "fractional mass preserved");

        pixel::World vessel(5, 3);
        for (int x = 0; x < 5; ++x) {
            vessel.set(x, 0, Material::Stone);
            vessel.set(x, 2, Material::Stone);
        }
        vessel.set(0, 1, Material::Stone);
        vessel.set(4, 1, Material::Stone);
        vessel.setFluid(1, 1, Material::Gas, 3);
        for (int i = 0; i < 500; ++i) vessel.tick();
        for (int x = 1; x <= 3; ++x)
            check(std::abs(vessel.gasPressure(x, 1) - 1) < 1e-8, "sealed gas equalizes pressure");
        check(vessel.gas(0, 1) == 0 && vessel.gas(4, 1) == 0, "gas cannot cross stone");

        pixel::World displacement(1, 2);
        displacement.setFluid(0, 0, Material::Water, 0.5);
        displacement.setFluid(0, 1, Material::Gas, 1);
        displacement.tick();
        check(displacement.gas(0, 0) > 0, "water displaces gas upward");
        check(displacement.water(0, 1) == 0.5, "water partially fills gas cell");
        check(std::abs(displacement.mass(Material::Gas) - 1) < 1e-10, "displaced gas conserved");
        for (int i = 0; i < 500; ++i) displacement.tick();
        check(std::abs(displacement.gasPressure(0, 0) / displacement.gasPressure(0, 1) - 1.1) < 1e-8,
              "light gas equilibrium uses accessible volume");

        pixel::World bounded(12, 10);
        for (int y = 0; y < 10; ++y)
            for (int x = 0; x < 12; ++x) {
                if ((x + y) % 4 == 0) bounded.setFluid(x, y, Material::Water, 0.37);
                else if ((x + y) % 4 == 1) bounded.setFluid(x, y, Material::Gas, 1.7);
                else if ((x + y) % 4 == 2) bounded.set(x, y, Material::Sand);
            }
        auto replica = bounded;
        const double wm = bounded.mass(Material::Water), gm = bounded.mass(Material::Gas);
        for (int t = 0; t < 1000; ++t) {
            bounded.tick();
            replica.tick();
            for (int y = 0; y < 10; ++y)
                for (int x = 0; x < 12; ++x) {
                    check(bounded.water(x, y) >= 0 && bounded.water(x, y) <= 1, "water capacity valid");
                    check(bounded.gas(x, y) >= 0 && std::isfinite(bounded.gas(x, y)), "gas stays valid");
                    check(bounded.water(x, y) == replica.water(x, y) &&
                          bounded.gas(x, y) == replica.gas(x, y) &&
                          bounded.material(x, y) == replica.material(x, y), "simulation deterministic");
                }
        }
        check(std::abs(bounded.mass(Material::Water) - wm) < 1e-8, "stress water conserved");
        check(std::abs(bounded.mass(Material::Gas) - gm) < 1e-8, "stress gas conserved");

        pixel::World path(1, 1);
        path.set(0, 0, Material::Soil);
        for (int i = 0; i < 30; ++i) path.stepOn(0, 0);
        check(path.ground(0, 0).grass < 0.01f, "repeated steps create bare path");
        check(path.ground(0, 0).flattened == 1.0f, "steps flatten grass");
        for (int i = 0; i < 60000; ++i) path.tick();
        check(path.ground(0, 0).grass > 0.99f, "unused path regrows");
        check(path.ground(0, 0).compaction == 0, "soil recovers");
        check(path.ground(0, 0).flattened == 0, "grass rises again");

        bool rejected = false;
        try { path.material(-1, 0); } catch (const std::out_of_range&) { rejected = true; }
        check(rejected, "bounds validated");
        rejected = false;
        try { pixel::World invalid(0, 4); } catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "dimensions validated");
        rejected = false;
        try { path.stepOn(0, 0, std::numeric_limits<float>::quiet_NaN()); }
        catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "nonfinite strength rejected");
        for (double amount : {-0.1, 1.1, std::numeric_limits<double>::infinity(),
                              std::numeric_limits<double>::quiet_NaN()}) {
            rejected = false;
            try { path.setFluid(0, 0, Material::Water, amount); }
            catch (const std::invalid_argument&) { rejected = true; }
            check(rejected, "invalid water amount rejected");
            check(path.material(0, 0) == Material::Soil, "rejected edit leaves world unchanged");
        }
        std::cout << "All simulation checks passed\n";
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
