#!/bin/bash
set -euo pipefail

ANTIMONY_DIR="${1:-build/antimony}"
EXECUTABLE="$ANTIMONY_DIR/antimony.exe"

if [ ! -f "$EXECUTABLE" ]; then
    echo "Error: Executable not found at $EXECUTABLE"
    exit 1
fi

echo "==> Deploying Qt runtime and plugins with windeployqt..."
windeployqt-qt5.exe --no-angle --no-opengl-sw --compiler-runtime --no-translations "$EXECUTABLE"

echo "==> Copying non-Qt runtime dependencies..."
cp /ucrt64/bin/libpython3*.dll "$ANTIMONY_DIR/"
cp /ucrt64/bin/libboost_python*.dll "$ANTIMONY_DIR/"
cp /ucrt64/bin/libpng*.dll "$ANTIMONY_DIR/"
cp /ucrt64/bin/zlib1.dll "$ANTIMONY_DIR/"

echo "==> Bundling Python standard library..."
PY_VER=$(python3 -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')")
PY_SRC="/ucrt64/lib/python${PY_VER}"
DEST_PY="$ANTIMONY_DIR/lib/python${PY_VER}"

if [ -d "$PY_SRC" ]; then
    mkdir -p "$ANTIMONY_DIR/lib"
    rm -rf "$DEST_PY"
    cp -R "$PY_SRC" "$DEST_PY"
    # Remove test directory and pycache to save space
    rm -rf "$DEST_PY/test" 2>/dev/null || true
    find "$DEST_PY" -name "__pycache__" -exec rm -rf {} + 2>/dev/null || true
fi

echo "==> Windows bundle completed successfully in $ANTIMONY_DIR"
