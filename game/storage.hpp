#pragma once
#include "sandbox.hpp"
#include <filesystem>
#include <iosfwd>

namespace storage {
void write(const Sandbox& game, std::ostream& output);
Sandbox read(std::istream& input);
// Writes a temporary file, then replaces the previous save on success.
void save(const Sandbox& game, const std::filesystem::path& file);
Sandbox load(const std::filesystem::path& file);
}
