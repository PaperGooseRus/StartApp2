#!/usr/bin/env bash
set -euo pipefail
project_dir=$(cd -- "$(dirname -- "$0")/.." && pwd)
if [[ ${MSYSTEM:-} != UCRT64 ]]; then
    echo 'Open the MSYS2 UCRT64 terminal first. See README.md.' >&2
    exit 1
fi
cmake -S "$project_dir" -B "$project_dir/build/windows" -G Ninja -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build "$project_dir/build/windows" --parallel 2
ctest --test-dir "$project_dir/build/windows" --output-on-failure
cmake --install "$project_dir/build/windows" --prefix "$project_dir/build/package-windows"
echo "Ready: $project_dir/build/package-windows/StartEditor.bat (IDE), StartGame.bat (game)"
