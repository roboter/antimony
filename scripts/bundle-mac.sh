#!/bin/bash
set -euo pipefail

APP_PATH="${1:-build/app/Antimony.app}"
EXECUTABLE="$APP_PATH/Contents/MacOS/Antimony"

if [ ! -d "$APP_PATH" ] || [ ! -f "$EXECUTABLE" ]; then
    echo "Error: Application bundle or executable not found at $APP_PATH"
    exit 1
fi

echo "==> Deploying Qt frameworks and plugins..."
QT5_PREFIX="$(brew --prefix qt@5 2>/dev/null || echo '/opt/homebrew/opt/qt@5')"
MACDEPLOYQT="$QT5_PREFIX/bin/macdeployqt"

if [ ! -x "$MACDEPLOYQT" ]; then
    echo "Error: macdeployqt not found at $MACDEPLOYQT"
    exit 1
fi

"$MACDEPLOYQT" "$APP_PATH"

echo "==> Bundling dynamic library (.dylib) dependencies..."
# Use dylibbundler to bundle Boost, libpng, and any other non-system dylibs
dylibbundler -b \
    -x "$EXECUTABLE" \
    -d "$APP_PATH/Contents/Frameworks/" \
    -p "@executable_path/../Frameworks/" \
    -cd -of

echo "==> Bundling Python framework..."
PY_PATH=$(otool -L "$EXECUTABLE" | awk '/Python\.framework/ {print $1}' | head -n 1 || true)

if [ -n "$PY_PATH" ]; then
    PY_FRAMEWORK=$(echo "$PY_PATH" | sed -E 's:(.*Python\.framework)/.*:\1:')
    PY_VER=$(echo "$PY_PATH" | sed -E 's:.*Python\.framework/Versions/([^/]+)/.*:\1:')
    
    echo "Found Python dependency: $PY_PATH"
    echo "Framework directory: $PY_FRAMEWORK (version: $PY_VER)"
    
    if [ -d "$PY_FRAMEWORK" ]; then
        DEST_PY="$APP_PATH/Contents/Frameworks/Python.framework"
        rm -rf "$DEST_PY"
        cp -R "$PY_FRAMEWORK" "$DEST_PY"
        
        # Remove unnecessary test files and documentation to reduce DMG size
        rm -rf "$DEST_PY/Versions/$PY_VER/lib/python$PY_VER/test" 2>/dev/null || true
        rm -rf "$DEST_PY/Versions/$PY_VER/share" 2>/dev/null || true
        find "$DEST_PY" -name "__pycache__" -exec rm -rf {} + 2>/dev/null || true
        
        # Rewire Antimony executable to the bundled Python framework
        NEW_PY_PATH="@executable_path/../Frameworks/Python.framework/Versions/$PY_VER/Python"
        install_name_tool -change "$PY_PATH" "$NEW_PY_PATH" "$EXECUTABLE"
        
        # Rewire any bundled libraries (such as libboost_python) that link against Python.framework
        for lib in "$APP_PATH/Contents/Frameworks"/*.dylib; do
            if [ -f "$lib" ]; then
                chmod +w "$lib"
                install_name_tool -change "$PY_PATH" "$NEW_PY_PATH" "$lib" 2>/dev/null || true
            fi
        done
        
        # Ensure bundled Python framework library ID is relative
        if [ -f "$DEST_PY/Versions/$PY_VER/Python" ]; then
            chmod +w "$DEST_PY/Versions/$PY_VER/Python"
            install_name_tool -id "$NEW_PY_PATH" "$DEST_PY/Versions/$PY_VER/Python" 2>/dev/null || true
        fi
    fi
fi

# Ensure @executable_path/../Frameworks is in rpath
install_name_tool -add_rpath "@executable_path/../Frameworks" "$EXECUTABLE" 2>/dev/null || true

echo "==> Re-signing application bundle..."
codesign --force --deep -s - "$APP_PATH"

echo "==> Verifying binary dependencies:"
otool -L "$EXECUTABLE"

if otool -L "$EXECUTABLE" | grep -E '(/opt/homebrew|/usr/local)'; then
    echo "ERROR: Unbundled Homebrew libraries still referenced in $EXECUTABLE"
    exit 1
fi

echo "==> Deployment completed successfully for $APP_PATH"
