#include "pixel/scene.hpp"
#include "sandbox.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main() {
    using namespace pixel;
    try {
        Scene scene;
        SceneObject shared;
        shared.position = {3, 4};
        shared.visuals = {
            {RenderLayer::Foreground, -100, {1, 0, 2, 2}, {1, 2, 3, 255}, 1},
            {RenderLayer::Background, 100, {0, 0, 2, 2}, {4, 5, 6, 255}, 0.25f}
        };
        shared.collider = Collider{{0, 0, 2, 2}};
        const auto object = scene.add(shared);
        const auto list = scene.drawList();
        check(list.size() == 2 && list[0].visual.layer == RenderLayer::Background &&
              list[1].visual.layer == RenderLayer::Foreground, "layers override local order");
        check(list[0].object == list[1].object && list[1].visual.bounds.x == 4, "one object spans multiple layers");
        check(scene.overlaps({3.5f, 4.5f, 1, 1}), "background visual can have a solid collider");
        check(!scene.overlaps({3.5f, 4.5f, 1, 1}, mask(CollisionGroup::Trigger)), "collision masks filter queries");
        check(!scene.overlaps({3.5f, 4.5f, 1, 1}, mask(CollisionGroup::Solid), 1), "collision planes filter queries");
        check(!scene.overlaps({5, 4, 1, 1}), "touching edges do not intersect");
        scene.setVisualVisible(object, 1, false);
        check(scene.drawList().size() == 1 && scene.overlaps({3.5f, 4.5f, 1, 1}), "visibility does not disable physics");
        scene.setColliderEnabled(object, false);
        check(!scene.overlaps({3.5f, 4.5f, 1, 1}), "disabled collider allows passage");
        scene.setColliderEnabled(object, true);
        scene.setPosition(object, {7, 8});
        check(!scene.overlaps({3.5f, 4.5f, 1, 1}) && scene.overlaps({7.5f, 8.5f, 1, 1}), "position moves physics and visuals together");
        Scene stable;
        SceneObject equal;
        equal.visuals = {{RenderLayer::Characters, 2, {0, 0, 1, 1}}};
        const auto first = stable.add(equal), second = stable.add(equal);
        auto sorted = stable.drawList();
        check(sorted[0].object == first && sorted[1].object == second, "equal sort keys preserve creation order");

        Camera camera(20, 10);
        camera.follow({30, 20}, {100, 50});
        check(camera.position().x == 20 && camera.position().y == 15, "camera centers target");
        const auto screen = camera.worldToScreen({25, 20});
        const auto inverse = camera.screenToWorld(screen);
        check(screen.x == 5 && screen.y == 5 && inverse.x == 25 && inverse.y == 20, "world/screen transforms are inverse");
        const auto background = camera.worldToScreen({25, 20}, 0.25f);
        check(background.x == 20 && background.y == 16.25f, "parallax transforms background independently");
        camera.follow({100, 50}, {100, 50});
        check(camera.position().x == 80 && camera.position().y == 40, "camera clamps right and bottom");
        camera.pan({-1000, -1000}, {100, 50});
        check(camera.position().x == 0 && camera.position().y == 0, "camera clamps left and top");
        camera.follow({7, 3}, {10, 5});
        check(camera.position().x == 0 && camera.position().y == 0, "small world does not produce negative camera");
        bool rejected = false;
        try { camera.moveTo({std::numeric_limits<float>::quiet_NaN(), 0}); }
        catch (const std::invalid_argument&) { rejected = true; }
        check(rejected && camera.position().x == 0, "invalid camera edit is rejected without mutation");
        rejected = false;
        try { equal.visuals[0].parallax = -1; stable.add(equal); }
        catch (const std::invalid_argument&) { rejected = true; }
        check(rejected && stable.drawList().size() == 2, "invalid visual rejected without insertion");

        Sandbox game;
        check(game.interact() == Sandbox::Interaction::TooFar, "gate requires proximity");
        for (int i = 0; i < 180; ++i) game.tick(1, false);
        check(game.x < 35 && game.x > 33 && game.grounded, "closed gate blocks player across draw layers");
        check(game.interact() == Sandbox::Interaction::Opened, "nearby gate opens");
        for (int i = 0; i < 30; ++i) game.tick(1, false);
        check(game.x > 38, "player passes through open gate");
        game.x = 36;
        check(game.interact() == Sandbox::Interaction::Occupied && game.gateOpen, "gate cannot close onto player");
        game.x = 39;
        check(game.interact() == Sandbox::Interaction::Closed && !game.gateOpen, "gate can close after passage");
        std::cout << "Scene checks passed: layer sorting, shared objects, collision masks, camera, parallax, gate interactions\n";
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
