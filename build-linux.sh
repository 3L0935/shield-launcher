#!/usr/bin/env bash
# Native Linux build of the launcher.
#
# Requirements:
#   - a C++20 compiler (gcc or clang)
#   - Qt6 base + widgets + multimedia development packages
#   - premake5 (https://premake.github.io/download, or ./tools/premake5)
#
# Usage: ./build-linux.sh [debug]
set -euo pipefail
cd "$(dirname "$0")"

config="${1:-release}"
if [ "$config" != "debug" ] && [ "$config" != "release" ]; then
    echo "usage: $0 [debug|release]" >&2
    exit 1
fi

if [ -x tools/premake5 ]; then
    premake_bin="tools/premake5"
elif command -v premake5 >/dev/null 2>&1; then
    premake_bin="$(command -v premake5)"
else
    echo "error: premake5 not found." >&2
    echo "       download it from https://premake.github.io/download and place it in tools/premake5" >&2
    exit 1
fi

moc_found=""
for candidate in /usr/lib/qt6/moc /usr/lib64/qt6/moc /usr/lib/qt6/libexec/moc \
                 /usr/lib/x86_64-linux-gnu/qt6/moc /usr/lib/x86_64-linux-gnu/qt6/libexec/moc; do
    if [ -x "$candidate" ]; then moc_found="$candidate"; break; fi
done
if [ -z "$moc_found" ] && ! command -v moc >/dev/null 2>&1; then
    echo "error: Qt6 moc not found, install your distribution's qt6 tools package." >&2
    exit 1
fi

"$premake_bin" gmake
make -C build "config=${config}_x64" -j"$(nproc)"

echo
echo "built: $(pwd)/build/bin/x64/${config^}/Shield_Launcher"
