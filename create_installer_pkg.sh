#!/bin/bash

# Create macOS .pkg installer for Wing Connector
# This script creates a distributable installer package

set -e

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

# Configuration
VERSION="1.0.0"
PLUGIN_NAME="reaper_wingconnector.dylib"
CONFIG_FILE="config.json"
PKG_NAME="WingConnector-${VERSION}.pkg"
PKG_IDENTIFIER="com.colab.reaper.wingconnector"

# Directories
BUILD_DIR="build"
PKG_ROOT="pkg_root"
PKG_SCRIPTS="pkg_scripts"
INSTALL_TARGET="/Library/Application Support/REAPER/UserPlugins"

echo -e "${BLUE}=========================================${NC}"
echo -e "${BLUE}  Wing Connector .pkg Installer Builder${NC}"
echo -e "${BLUE}=========================================${NC}"
echo ""

# Check if plugin is built
if [ ! -f "$BUILD_DIR/$PLUGIN_NAME" ]; then
    echo -e "${RED}✗${NC} Plugin not built"
    echo "  Run ./build.sh first"
    exit 1
fi

echo -e "${GREEN}✓${NC} Found plugin build"

# Check for pkgbuild
if ! command -v pkgbuild &> /dev/null; then
    echo -e "${RED}✗${NC} pkgbuild not found (requires macOS)"
    exit 1
fi

# Clean up old package files
rm -rf "$PKG_ROOT" "$PKG_SCRIPTS" "$PKG_NAME"

# Create package root structure
echo ""
echo -e "${BLUE}ℹ${NC} Creating package structure..."
mkdir -p "$PKG_ROOT$INSTALL_TARGET"

# Copy files to package root
cp "$BUILD_DIR/$PLUGIN_NAME" "$PKG_ROOT$INSTALL_TARGET/"
cp "$CONFIG_FILE" "$PKG_ROOT$INSTALL_TARGET/"

# Create scripts directory
mkdir -p "$PKG_SCRIPTS"

# Create postinstall script
cat > "$PKG_SCRIPTS/postinstall" << 'POSTINSTALL'
#!/bin/bash

# Copy plugin to user's REAPER directory if it exists
USER_PLUGINS_DIR="$HOME/Library/Application Support/REAPER/UserPlugins"

if [ -d "$HOME/Library/Application Support/REAPER" ]; then
    mkdir -p "$USER_PLUGINS_DIR"
    cp -f "/Library/Application Support/REAPER/UserPlugins/reaper_wingconnector.dylib" "$USER_PLUGINS_DIR/"
    cp -f "/Library/Application Support/REAPER/UserPlugins/config.json" "$USER_PLUGINS_DIR/" 2>/dev/null || true
    
    # Set permissions
    chmod 755 "$USER_PLUGINS_DIR/reaper_wingconnector.dylib"
    
    echo "Wing Connector installed to: $USER_PLUGINS_DIR"
fi

exit 0
POSTINSTALL

chmod +x "$PKG_SCRIPTS/postinstall"

# Build the package
echo -e "${BLUE}ℹ${NC} Building installer package..."
pkgbuild \
    --root "$PKG_ROOT" \
    --scripts "$PKG_SCRIPTS" \
    --identifier "$PKG_IDENTIFIER" \
    --version "$VERSION" \
    --install-location "/" \
    "$PKG_NAME"

# Clean up temporary files
rm -rf "$PKG_ROOT" "$PKG_SCRIPTS"

# Success
echo ""
echo -e "${GREEN}=========================================${NC}"
echo -e "${GREEN}  Package Created Successfully!${NC}"
echo -e "${GREEN}=========================================${NC}"
echo ""
echo "Installer package: $PKG_NAME"
echo "Size: $(du -h "$PKG_NAME" | cut -f1)"
echo ""
echo "To install:"
echo "  Double-click $PKG_NAME"
echo ""
echo "Or distribute this file to users for easy installation."
echo ""
