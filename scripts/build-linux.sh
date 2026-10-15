#!/bin/sh
set -eu
project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
for tool in cmake c++; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Missing $tool. On Ubuntu/Debian: sudo apt install build-essential cmake libsdl2-dev" >&2
        exit 1
    fi
done
cmake -S "$project_dir" -B "$project_dir/build/linux" -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build "$project_dir/build/linux" --parallel 2
ctest --test-dir "$project_dir/build/linux" --output-on-failure
cmake --install "$project_dir/build/linux" --prefix "$project_dir/build/package-linux"
echo "Ready: $project_dir/build/package-linux/StartEditor.sh (IDE), StartGame.sh (game)"
