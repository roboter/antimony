#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BUNDLE=false
CREATE_DMG=false
CLEAN=false
SKIP_DEPS=false

for arg in "$@"; do
    case "$arg" in
        --bundle)
            BUNDLE=true
            ;;
        --dmg)
            BUNDLE=true
            CREATE_DMG=true
            ;;
        --clean)
            CLEAN=true
            ;;
        --skip-deps)
            SKIP_DEPS=true
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --bundle      Bundle frameworks & libraries for standalone distribution"
            echo "  --dmg         Bundle and create antimony-mac.dmg"
            echo "  --clean       Remove build directory before compiling"
            echo "  --skip-deps   Skip dependency check/installation"
            echo "  -h, --help    Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $arg"
            echo "Use -h or --help for usage."
            exit 1
            ;;
    esac
done

cd "$REPO_ROOT"

if [ "$SKIP_DEPS" = false ]; then
    if ! command -v brew &>/dev/null; then
        echo "Error: Homebrew is not installed. Please install Homebrew from https://brew.sh/"
        exit 1
    fi

    echo "==> Checking dependencies with Homebrew..."
    REQUIRED_DEPS=(libpng boost-python3 qt@5 lemon flex ninja cmake dylibbundler)
    MISSING_DEPS=()

    for dep in "${REQUIRED_DEPS[@]}"; do
        if ! brew list --formula "$dep" &>/dev/null; then
            MISSING_DEPS+=("$dep")
        fi
    done

    if [ ${#MISSING_DEPS[@]} -gt 0 ]; then
        echo "==> Installing missing dependencies: ${MISSING_DEPS[*]}..."
        brew install "${MISSING_DEPS[@]}"
    else
        echo "==> All required Homebrew dependencies are installed."
    fi
fi

QT5_PREFIX="$(brew --prefix qt@5 2>/dev/null || echo '/opt/homebrew/opt/qt@5')"

if [ "$CLEAN" = true ]; then
    echo "==> Cleaning build directory..."
    rm -rf "$REPO_ROOT/build"
fi

mkdir -p "$REPO_ROOT/build"
cd "$REPO_ROOT/build"

echo "==> Configuring with CMake..."
cmake -DCMAKE_PREFIX_PATH="$QT5_PREFIX" -GNinja "$REPO_ROOT"

echo "==> Building with Ninja..."
ninja

echo "==> Build successful! Application located at build/app/Antimony.app"

if [ "$BUNDLE" = true ]; then
    echo "==> Bundling application dependencies..."
    chmod +x "$SCRIPT_DIR/bundle-mac.sh"
    "$SCRIPT_DIR/bundle-mac.sh" "$REPO_ROOT/build/app/Antimony.app"
fi

if [ "$CREATE_DMG" = true ]; then
    echo "==> Creating DMG..."
    cd "$REPO_ROOT"
    rm -rf dmg_root antimony-mac.dmg
    mkdir -p dmg_root
    cp -R build/app/Antimony.app dmg_root/
    ln -s /Applications dmg_root/Applications
    hdiutil create -volname "Antimony" -srcfolder dmg_root -ov -format UDZO antimony-mac.dmg
    rm -rf dmg_root
    echo "==> Created antimony-mac.dmg successfully!"
fi

echo ""
echo "Done! You can launch Antimony with:"
echo "  open $REPO_ROOT/build/app/Antimony.app"
