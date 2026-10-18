#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

VERSION="${1:-0.9.7}"
# Strip leading 'v' if present
VERSION="${VERSION#v}"

echo "==> Building Antimony v${VERSION} for Linux..."
cd "$REPO_ROOT"

# Create build directory
mkdir -p build
cd build

# Configure and build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr ..
ninja

cd "$REPO_ROOT"

# Ensure output directory exists
mkdir -p dist

echo "==> Packaging standalone tarball (antimony-linux-x86_64.tar.gz)..."
TAR_DIR="dist/antimony-linux-${VERSION}"
rm -rf "$TAR_DIR"
mkdir -p "$TAR_DIR/bin" "$TAR_DIR/sb" "$TAR_DIR/share/applications" "$TAR_DIR/share/icons/hicolor/scalable/apps"

cp build/antimony/antimony "$TAR_DIR/bin/antimony"
cp -r build/antimony/sb/nodes "$TAR_DIR/sb/nodes"
cp -r build/antimony/sb/fab "$TAR_DIR/sb/fab"
cp deploy/icon.svg "$TAR_DIR/share/icons/hicolor/scalable/apps/antimony.svg"
cp deploy/icon.svg "$TAR_DIR/icon.svg"

# Create portable launcher
cat << 'EOF' > "$TAR_DIR/antimony.sh"
#!/bin/bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export PATH="$DIR/bin:$PATH"
exec "$DIR/bin/antimony" "$@"
EOF
chmod +x "$TAR_DIR/antimony.sh"

# Create desktop entry
cat << EOF > "$TAR_DIR/share/applications/antimony.desktop"
[Desktop Entry]
Type=Application
Version=1.0
Name=Antimony
Comment=CAD from a parallel universe
GenericName=CAD Application
Exec=antimony %f
Icon=antimony
Terminal=false
Categories=Graphics;Science;Engineering;
MimeType=application/x-extension-sb;application/x-antimony;
StartupWMClass=antimony
EOF

# Create tarball
cd dist
tar -czvf "antimony-linux-x86_64.tar.gz" "antimony-linux-${VERSION}"
cd "$REPO_ROOT"

echo "==> Packaging Debian package (antimony-linux-amd64.deb)..."
DEB_DIR="dist/deb_root"
rm -rf "$DEB_DIR"
mkdir -p "$DEB_DIR/DEBIAN"
mkdir -p "$DEB_DIR/usr/bin"
mkdir -p "$DEB_DIR/usr/share/antimony"
mkdir -p "$DEB_DIR/usr/share/applications"
mkdir -p "$DEB_DIR/usr/share/icons/hicolor/scalable/apps"

cp build/antimony/antimony "$DEB_DIR/usr/bin/antimony"
cp -r build/antimony/sb/nodes "$DEB_DIR/usr/share/antimony/nodes"
cp -r build/antimony/sb/fab "$DEB_DIR/usr/share/antimony/fab"
cp deploy/icon.svg "$DEB_DIR/usr/share/icons/hicolor/scalable/apps/antimony.svg"
cp "$TAR_DIR/share/applications/antimony.desktop" "$DEB_DIR/usr/share/applications/antimony.desktop"

cat << EOF > "$DEB_DIR/DEBIAN/control"
Package: antimony
Version: ${VERSION}
Architecture: amd64
Maintainer: Roboter <roboter@begemotik.ee>
Depends: libqt5widgets5 (>= 5.4), libqt5gui5 (>= 5.4), libqt5opengl5 (>= 5.4), libqt5network5 (>= 5.4), libqt5core5a (>= 5.4), libpng16-16, python3
Section: graphics
Priority: optional
Homepage: https://github.com/roboter/antimony
Description: CAD from a parallel universe
 Antimony is a computer-aided design (CAD) tool from a parallel universe
 in which CAD software evolved from Lisp machines rather than drafting tables.
EOF

dpkg-deb --build "$DEB_DIR" "dist/antimony-linux-amd64.deb"

# Optional AppImage generation
if command -v linuxdeploy &>/dev/null || [ -f "./linuxdeploy-x86_64.AppImage" ]; then
    echo "==> Attempting AppImage creation..."
    LINUXDEPLOY="./linuxdeploy-x86_64.AppImage"
    if command -v linuxdeploy &>/dev/null; then
        LINUXDEPLOY="linuxdeploy"
    else
        chmod +x "$LINUXDEPLOY"
    fi

    if [ -f "./linuxdeploy-plugin-qt-x86_64.AppImage" ]; then
        chmod +x "./linuxdeploy-plugin-qt-x86_64.AppImage"
        ln -sf linuxdeploy-plugin-qt-x86_64.AppImage linuxdeploy-plugin-qt
        export PATH="$PWD:$PATH"
    fi

    APPDIR="dist/AppDir"
    rm -rf "$APPDIR"
    mkdir -p "$APPDIR/usr/share/antimony"
    cp -r build/antimony/sb/nodes "$APPDIR/usr/share/antimony/nodes"
    cp -r build/antimony/sb/fab "$APPDIR/usr/share/antimony/fab"
    
    QMAKE_BIN="$(command -v qmake || command -v qmake-qt5 || echo /usr/lib/qt5/bin/qmake)"
    QMAKE="$QMAKE_BIN" "$LINUXDEPLOY" --appdir "$APPDIR" \
        -e build/antimony/antimony \
        -i deploy/icon.svg \
        -d "$TAR_DIR/share/applications/antimony.desktop" \
        --plugin qt \
        --output appimage || echo "AppImage creation skipped or failed."

    find . -maxdepth 1 -name "*Antimony*.AppImage" -exec mv {} "dist/antimony-linux-${VERSION}-x86_64.AppImage" \; 2>/dev/null || true
fi

echo "==> Linux build and packaging complete. Artifacts in dist/:"
ls -lh dist/
