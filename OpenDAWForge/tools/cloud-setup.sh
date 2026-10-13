#!/usr/bin/env bash
set -euo pipefail
# Cloud tasks are already isolated: use this checkout, never create a worktree.
forge_repo=/workspace/LS-Factory/OpenDAWForge
forge_tools=/workspace/.tools
mkdir -p "$forge_tools"/apt/{lists/partial,cache/archives/partial,conf} "$forge_tools/sysroot" "$forge_tools/cache" "$forge_tools/config"
if ! LD_LIBRARY_PATH="$forge_tools/sysroot/usr/lib/x86_64-linux-gnu" "$forge_tools/sysroot/usr/bin/cmake" --version >/dev/null 2>&1 || ! test -f "$forge_tools/sysroot/usr/include/X11/Xlib.h"; then
    cat > "$forge_tools/apt/sources.list" <<'SOURCES'
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian trixie main
SOURCES
    forge_apt=(/usr/bin/apt-get -o "Dir::Etc::parts=$forge_tools/apt/conf" -o "Dir::Etc::sourcelist=$forge_tools/apt/sources.list" -o Dir::Etc::sourceparts=- -o "Dir::State::lists=$forge_tools/apt/lists" -o "Dir::Cache=$forge_tools/apt/cache")
    "${forge_apt[@]}" update
    forge_download=$(mktemp -d "$forge_tools/packages.XXXXXX")
    trap 'rm -rf "$forge_download"' EXIT
    (
        cd "$forge_download"
        "${forge_apt[@]}" download clang-19 libllvm19 libclang-cpp19 libclang-common-19-dev libclang1-19 llvm-19-linker-tools cmake cmake-data ninja-build libjsoncpp26 librhash1 libuv1t64 libasound2-dev libasound2t64 libfreetype-dev libfreetype6 libfontconfig-dev libfontconfig1 libx11-dev libx11-6 libxext-dev libxext6 libxinerama-dev libxinerama1 libxrandr-dev libxrandr2 libxcursor-dev libxcursor1 libxrender-dev libxrender1 libxfixes-dev libxfixes3 libgl-dev libgl1 libglx-dev libglx0 xvfb libxfont2 x11proto-dev xtrans-dev libpng-dev zlib1g-dev libbrotli-dev libharfbuzz-dev libharfbuzz-gobject0 libharfbuzz-subset0 libgraphite2-dev libbz2-dev clang-format-19
        for forge_package in ./*.deb; do dpkg-deb -x "$forge_package" "$forge_tools/sysroot"; done
    )
fi
cat > "$forge_tools/activate.sh" <<'ACTIVATE'
export PATH=/workspace/.tools/sysroot/usr/bin:/workspace/.tools/sysroot/usr/lib/llvm-19/bin:$PATH
export LD_LIBRARY_PATH=/workspace/.tools/sysroot/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
export PKG_CONFIG_PATH=/workspace/.tools/sysroot/usr/lib/x86_64-linux-gnu/pkgconfig:/workspace/.tools/sysroot/usr/share/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=/workspace/.tools/sysroot
export CPLUS_INCLUDE_PATH=/workspace/.tools/sysroot/usr/include:/workspace/.tools/sysroot/usr/include/x86_64-linux-gnu${CPLUS_INCLUDE_PATH:+:$CPLUS_INCLUDE_PATH}
export C_INCLUDE_PATH=/workspace/.tools/sysroot/usr/include:/workspace/.tools/sysroot/usr/include/x86_64-linux-gnu${C_INCLUDE_PATH:+:$C_INCLUDE_PATH}
export LIBRARY_PATH=/workspace/.tools/sysroot/usr/lib/x86_64-linux-gnu${LIBRARY_PATH:+:$LIBRARY_PATH}
export CC=clang-19
export CXX=clang++-19
export CMAKE_BUILD_PARALLEL_LEVEL=3
export XDG_CACHE_HOME=/workspace/.tools/cache
export XDG_CONFIG_HOME=/workspace/.tools/config
ACTIVATE
source "$forge_tools/activate.sh"
clang++-19 --version
cmake --version
if ! test -f "$forge_tools/JUCE/CMakeLists.txt"; then git clone --depth 1 --branch 8.0.15 https://github.com/juce-framework/JUCE.git "$forge_tools/JUCE"; fi
if ! test -f "$forge_tools/Catch2/CMakeLists.txt"; then git clone --depth 1 --branch v3.8.1 https://github.com/catchorg/Catch2.git "$forge_tools/Catch2"; fi
test "$(git -C "$forge_tools/JUCE" rev-parse HEAD)" = 91ad83ae34a81e0833b1a2b0866f54846370ae53
test "$(git -C "$forge_tools/Catch2" rev-parse HEAD)" = 2b60af89e23d28eefc081bc930831ee9d45ea58b
cd "$forge_repo"
cmake --preset debug -DFETCHCONTENT_SOURCE_DIR_JUCE="$forge_tools/JUCE" -DFETCHCONTENT_SOURCE_DIR_CATCH2="$forge_tools/Catch2"
cmake --build --preset debug --parallel 3
ctest --preset debug
