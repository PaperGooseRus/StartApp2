#pragma once
#include "project.hpp"
#include <deque>
#include <optional>
namespace studio {
class Document {
public:
    Project project = Project::example();
    std::uint64_t revision = 1;
    bool dirty = false;
    std::optional<Sandbox> runtime;
    bool simulationOnly = false, paused = false;
    double playTime = 0;
    void beginChange();
    void changed();
    void commit();
    bool undo();
    bool redo();
    bool canUndo() const { return before_.has_value() || !undo_.empty(); }
    bool canRedo() const { return !before_ && !redo_.empty(); }
    void reset(Project value);
    void start(bool simulation);
    void stop();
    void step(int direction = 0, bool jump = false);
    pixel::ObjectId place(const std::string& preset, pixel::Vec2 position, std::size_t artIndex = 0);
    SpriteArt& addArt(int width, int height);
    void duplicateFrame(std::size_t art, std::size_t frame, bool blank = false);
    void removeFrame(std::size_t art, std::size_t frame);
    void moveFrame(std::size_t art, std::size_t from, std::size_t to);
private:
    std::optional<Project> before_;
    std::deque<Project> undo_, redo_;
    void trim();
};
}
