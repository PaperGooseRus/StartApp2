#include "pixel/world.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
pixel::World grass(double moisture) {
    pixel::World world(1, 1);
    world.set(0, 0, pixel::Material::Soil);
    world.setMoisture(0, 0, moisture);
    for (int i = 0; i < 6; ++i) world.stepOn(0, 0, 10);
    return world;
}
int main() {
    using pixel::Material;
    try {
        pixel::World absorption(1, 2);
        absorption.set(0, 1, Material::Soil);
        absorption.setMoisture(0, 1, 0);
        absorption.setFluid(0, 0, Material::Water, 0.2);
        absorption.tick();
        check(absorption.water(0, 0) < 0.2 && absorption.soilWater(0, 1) > 0, "water is absorbed");
        check(std::abs(absorption.mass(Material::Water) - 0.2) < 1e-12, "absorption conserves water");
        for (int i = 0; i < 500; ++i) absorption.tick();
        check(absorption.water(0, 0) == 0 && std::abs(absorption.ground(0, 1).moisture - 0.4) < 1e-12,
              "dry soil stores the entire small puddle");

        pixel::World capacity(1, 2);
        capacity.set(0, 1, Material::Soil);
        capacity.setMoisture(0, 1, 0);
        capacity.set(0, 0, Material::Water);
        for (int i = 0; i < 2000; ++i) capacity.tick();
        check(capacity.ground(0, 1).moisture == 1, "soil saturates at capacity");
        check(std::abs(capacity.water(0, 0) - 0.5) < 1e-12, "excess water stays free");

        pixel::World permeable(1, 2);
        permeable.set(0, 1, Material::Soil);
        permeable.setMoisture(0, 1, 0);
        permeable.set(0, 0, Material::Water);
        auto compacted = permeable;
        compacted.stepOn(0, 1, 10);
        permeable.tick(); compacted.tick();
        check(permeable.soilWater(0, 1) > compacted.soilWater(0, 1), "compaction slows infiltration");

        pixel::World diffusion(2, 1);
        for (int x = 0; x < 2; ++x) diffusion.set(x, 0, Material::Soil);
        diffusion.setMoisture(0, 0, 1);
        diffusion.setMoisture(1, 0, 0);
        diffusion.tick();
        check(diffusion.ground(1, 0).moisture > 0 && diffusion.ground(0, 0).moisture < 1, "soil shares moisture");
        for (int i = 0; i < 2000; ++i) diffusion.tick();
        check(std::abs(diffusion.ground(1, 0).moisture - 0.5) < 1e-8, "moisture reaches equilibrium");
        check(std::abs(diffusion.mass(Material::Water) - 0.5) < 1e-12, "diffusion conserves water");

        pixel::World barrier(1, 3);
        barrier.set(0, 0, Material::Water);
        barrier.set(0, 1, Material::Stone);
        barrier.set(0, 2, Material::Soil);
        barrier.setMoisture(0, 2, 0);
        for (int i = 0; i < 1000; ++i) barrier.tick();
        check(barrier.ground(0, 2).moisture == 0, "stone blocks moisture transfer");

        pixel::World dry(1, 1), wet(1, 1);
        dry.set(0, 0, Material::Soil); wet.set(0, 0, Material::Soil);
        dry.setMoisture(0, 0, 0.1); wet.setMoisture(0, 0, 0.9);
        dry.stepOn(0, 0); wet.stepOn(0, 0);
        check(wet.ground(0, 0).compaction > dry.ground(0, 0).compaction, "wet soil compacts more");
        auto drought = grass(0), optimum = grass(0.6), soaked = grass(1);
        for (int i = 0; i < 10000; ++i) { drought.tick(); optimum.tick(); soaked.tick(); }
        check(optimum.ground(0, 0).grass > 0.1 && drought.ground(0, 0).grass == 0 && soaked.ground(0, 0).grass == 0,
              "growth depends on suitable moisture");
        check(soaked.ground(0, 0).compaction > optimum.ground(0, 0).compaction, "wet trail recovers more slowly");

        pixel::World covered(1, 2);
        covered.set(0, 0, Material::Stone);
        covered.set(0, 1, Material::Soil);
        for (int i = 0; i < 6; ++i) covered.stepOn(0, 1, 10);
        for (int i = 0; i < 10000; ++i) covered.tick();
        check(!covered.exposedSoil(0, 1) && covered.ground(0, 1).grass == 0, "buried grass cannot grow");

        pixel::World sealed(8, 8);
        for (int y = 4; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                sealed.set(x, y, Material::Soil);
                sealed.setMoisture(x, y, (x + y) % 3 * 0.4);
                sealed.stepOn(x, y, 2);
            }
        for (int y = 0; y < 3; ++y) {
            sealed.setFluid(1, y, Material::Water, 0.7);
            sealed.setFluid(5, y, Material::Gas, 0.5);
            sealed.set(3, y, Material::Sand);
        }
        const double waterMass = sealed.mass(Material::Water), gasMass = sealed.mass(Material::Gas);
        for (int i = 0; i < 5000; ++i) sealed.tick();
        check(std::abs(sealed.mass(Material::Water) - waterMass) < 1e-8, "mixed soil/fluid world conserves water");
        check(std::abs(sealed.mass(Material::Gas) - gasMass) < 1e-8, "absorption does not destroy gas");
        for (int y = 4; y < 8; ++y)
            for (int x = 0; x < 8; ++x)
                check(sealed.ground(x, y).moisture >= 0 && sealed.ground(x, y).moisture <= 1, "moisture remains bounded");
        bool rejected = false;
        try { absorption.setMoisture(0, 1, 1.1); } catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "invalid moisture rejected");
        rejected = false;
        try { absorption.setMoisture(0, 0, 0.5); } catch (const std::invalid_argument&) { rejected = true; }
        check(rejected, "moisture edits require soil");
        std::cout << "Soil checks passed: absorption, saturation, diffusion, mass balance, grass, compaction, barriers\n";
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
