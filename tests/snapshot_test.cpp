#include "pixel/world.hpp"
#include "storage.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <locale>
#include <sstream>
#include <stdexcept>

void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
std::string snapshot(const pixel::World& world) {
    std::ostringstream output(std::ios::binary);
    world.save(output);
    return output.str();
}
std::string snapshot(const Sandbox& game) {
    std::ostringstream output(std::ios::binary);
    storage::write(game, output);
    return output.str();
}
void rejects(const std::string& data, const char* message) {
    bool rejected = false;
    try { std::istringstream input(data, std::ios::binary); pixel::World::load(input); }
    catch (const std::runtime_error&) { rejected = true; }
    check(rejected, message);
}
// Fixed snapshot emitted using the v0.3 format: two cells, 0.4 free water,
// soil with 25% moisture. It is independent of the current snapshot writer.
std::string legacyFixture() {
    const unsigned char bytes[] = {
        0x50,0x58,0x57,0x4f,0x52,0x4c,0x44,0x00,0x01,0x00,0x00,0x00,0x02,0x00,0x00,0x00,
        0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x9a,0x99,0x99,
        0x99,0x99,0x99,0xd9,0x3f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,
        0x3f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x3e,0x02,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x80,0x3f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x3e,0x38,0xcd,
        0xe9,0x7c,0x86,0x37,0x82,0xbd,
    };
    return std::string(reinterpret_cast<const char*>(bytes), sizeof(bytes));
}
struct TestDirectory {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        std::filesystem::u8path(u8"PixelSandbox-проверка-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    TestDirectory() { check(std::filesystem::create_directory(path), "unique test directory created"); }
    ~TestDirectory() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
};
struct Comma : std::numpunct<char> { char do_decimal_point() const override { return ','; } };
int main() {
    try {
        using pixel::Material;
        pixel::World world(8, 6);
        for (int x = 0; x < 8; ++x) world.set(x, 5, Material::Soil);
        for (int i = 0; i < 7; ++i) world.stepOn(3, 5);
        world.setFluid(2, 1, Material::Water, 0.37);
        world.setFluid(3, 2, Material::Gas, 2.5);
        world.set(4, 0, Material::Sand);
        world.set(7, 4, Material::Stone);
        for (int i = 0; i < 17; ++i) world.tick();
        const auto data = snapshot(world);
        std::istringstream legacyInput(legacyFixture(), std::ios::binary);
        auto legacy = pixel::World::load(legacyInput);
        check(legacy.water(0, 0) == 0.4 && legacy.ground(1, 0).moisture == 0.25,
              "v0.3 snapshot remains readable");
        check(legacy.soilWater(1, 0) == 0.125 && snapshot(legacy)[8] == 2,
              "legacy moisture becomes stored water and is saved as v2");
        check(data.size() == 28 + 8 * 6 * 37 + 8, "portable record size");
        std::istringstream input(data, std::ios::binary);
        auto loaded = pixel::World::load(input);
        check(snapshot(loaded) == data, "world round trip preserves every field");
        std::istringstream throwingStream(data, std::ios::binary);
        throwingStream.exceptions(std::ios::failbit | std::ios::badbit);
        check(snapshot(pixel::World::load(throwingStream)) == data, "exception-enabled stream supported");
        for (int i = 0; i < 37; ++i) { loaded.tick(); world.tick(); }
        check(snapshot(loaded) == snapshot(world), "restored tick phase preserves future simulation");
        for (std::size_t length : {std::size_t(0), std::size_t(1), std::size_t(27), std::size_t(28), data.size() - 1})
            rejects(data.substr(0, length), "truncated snapshot rejected");
        rejects(data + "extra", "trailing bytes rejected");
        auto bad = data; bad[0] = 'X'; rejects(bad, "bad magic rejected");
        bad = data; bad[8] = 3; rejects(bad, "unknown version rejected");
        bad = data; bad[12] = 0; bad[13] = 0x10; bad[16] = 0; bad[17] = 0x10;
        rejects(bad, "oversized dimensions rejected before allocation");
        bad = data; bad[28] = static_cast<char>(255); rejects(bad, "invalid material rejected");
        bad = data;
        for (int i = 0; i < 8; ++i) bad[29 + i] = static_cast<char>(0x7ff8000000000000ull >> (i * 8));
        rejects(bad, "NaN fluid rejected");
        bad = data; bad[20] ^= 1; rejects(bad, "corrupted tick detected by checksum");
        std::ostringstream failed;
        failed.setstate(std::ios::badbit);
        bool rejected = false;
        try { world.save(failed); } catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "output failure reported");

        Sandbox game;
        for (int i = 0; i < 39; ++i) game.tick(1, false);
        game.tick(0, true); // Save while in flight, including velocity and step distance.
        const auto gameData = snapshot(game);
        check(!game.gateOpen, "new scene starts with closed gate");
        game.setGateOpen(true);
        game.setChestOpen(true);
        std::istringstream openInput(snapshot(game), std::ios::binary);
        auto openGame = storage::read(openInput);
        check(openGame.gateOpen && !openGame.scene.object(openGame.gate).collider->enabled,
              "gate state and physics restored");
        check(openGame.chestOpen && openGame.scene.object(openGame.chest).visuals[2].visible, "chest state and art restored");
        Sandbox oldScene;
        std::istringstream oldGameInput("PXSANDBOX 1 18 52 0 0 1 15483850418005241176\n" + snapshot(oldScene.world), std::ios::binary);
        auto oldGame = storage::read(oldGameInput);
        check(oldGame.gateOpen && oldGame.x == 18, "legacy game header loads with a clear gate");
        std::istringstream v2Input("PXSANDBOX 2 18 52 0 0 1 0 5242225865643081145\n" + snapshot(oldScene.world), std::ios::binary);
        auto v2Game = storage::read(v2Input);
        check(!v2Game.gateOpen && !v2Game.chestOpen, "version 2 preserves gate and defaults chest closed");
        game.setGateOpen(false);
        game.setChestOpen(false);
        std::istringstream gameInput(gameData, std::ios::binary);
        auto restored = storage::read(gameInput);
        check(snapshot(restored) == gameData, "character and world round trip");
        for (int i = 0; i < 60; ++i) { game.tick(0, false); restored.tick(0, false); }
        check(snapshot(restored) == snapshot(game), "restored character lands identically");
        const auto originalLocale = std::locale();
        std::locale::global(std::locale(originalLocale, new Comma));
        const auto commaData = snapshot(game);
        std::istringstream commaInput(commaData, std::ios::binary);
        restored = storage::read(commaInput);
        std::locale::global(originalLocale);
        check(snapshot(restored) == snapshot(game), "locale independent game saves");

        auto corruptedHeader = snapshot(game);
        const auto position = corruptedHeader.find(' ', 10) + 1;
        corruptedHeader[position] = corruptedHeader[position] == '1' ? '2' : '1';
        rejected = false;
        try { std::istringstream damaged(corruptedHeader, std::ios::binary); storage::read(damaged); }
        catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "character header corruption detected");

        TestDirectory directory;
        const auto file = directory.path / std::filesystem::u8path(u8"мир с пробелами.pxs");
        storage::save(game, file);
        check(snapshot(storage::load(file)) == snapshot(game), "Unicode file path save/load");
        game.world.setFluid(4, 4, Material::Water, 0.25);
        storage::save(game, file);
        check(snapshot(storage::load(file)) == snapshot(game), "existing save replaced");
        const auto previous = snapshot(storage::load(file));
        auto invalid = game;
        invalid.x = -1;
        rejected = false;
        try { storage::save(invalid, file); } catch (const std::runtime_error&) { rejected = true; }
        check(rejected && snapshot(storage::load(file)) == previous, "failed save preserves previous file");
        const auto blockedFile = directory.path / "directory.pxs";
        std::filesystem::create_directory(blockedFile);
        rejected = false;
        try { storage::save(game, blockedFile); } catch (const std::exception&) { rejected = true; }
        check(rejected && std::filesystem::is_directory(blockedFile), "failed replacement preserves target");
        for (const auto& entry : std::filesystem::directory_iterator(directory.path))
            check(entry.path().filename().u8string().find(".tmp-") == std::string::npos, "temporary saves cleaned up");
        rejected = false;
        try { storage::load(directory.path / "missing.pxs"); } catch (const std::exception&) { rejected = true; }
        check(rejected, "missing save reported");
        const auto beforeFailure = snapshot(restored);
        { std::ofstream output(file, std::ios::binary); output << "broken"; }
        rejected = false;
        try { restored = storage::load(file); } catch (const std::exception&) { rejected = true; }
        check(rejected && snapshot(restored) == beforeFailure, "failed load leaves current game untouched");
        std::cout << "Snapshot checks passed: portable round trip, deterministic continuation, corruption, Unicode paths, replacement, failure recovery\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
