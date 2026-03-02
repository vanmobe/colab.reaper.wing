#!/bin/bash

# Wing Connector for REAPER - macOS Installer
# Version 1.0.0

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
PLUGIN_NAME="reaper_wingconnector.dylib"
CONFIG_FILE="config.json"
REAPER_PLUGINS_DIR="$HOME/Library/Application Support/REAPER/UserPlugins"
BUILD_DIR="build"

# Functions
print_header() {
    echo ""
    echo -e "${BLUE}=========================================${NC}"
    echo -e "${BLUE}  Wing Connector for REAPER - Installer${NC}"
    echo -e "${BLUE}  Version 1.0.0${NC}"
    echo -e "${BLUE}=========================================${NC}"
    echo ""
}

print_success() {
    echo -e "${GREEN}✓${NC} $1"
}

print_error() {
    echo -e "${RED}✗${NC} $1"
}

print_info() {
    echo -e "${BLUE}ℹ${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}⚠${NC} $1"
}

check_reaper() {
    if [ -d "/Applications/REAPER.app" ] || [ -d "/Applications/REAPER64.app" ]; then
        print_success "REAPER installation found"
        return 0
    else
        print_warning "REAPER installation not found in /Applications"
        print_info "Plugin will still be installed to: $REAPER_PLUGINS_DIR"
        return 1
    fi
}

check_build() {
    if [ -f "$BUILD_DIR/$PLUGIN_NAME" ]; then
        print_success "Plugin build found"
        return 0
    else
        print_warning "Plugin not built yet"
        return 1
    fi
}

build_plugin() {
    print_info "Building Wing Connector plugin..."
    
    # Check for CMake
    if ! command -v cmake &> /dev/null; then
        print_error "CMake not found"
        echo "  Please install CMake: brew install cmake"
        exit 1
    fi
    
    # Create build directory
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # Configure and build
    echo "  Configuring..."
    cmake -DCMAKE_BUILD_TYPE=Release .. > /dev/null
    
    echo "  Compiling..."
    make -j4
    
    cd ..
    
    if [ -f "$BUILD_DIR/$PLUGIN_NAME" ]; then
        print_success "Plugin built successfully"
    else
        print_error "Build failed"
        exit 1
    fi
}

install_plugin() {
    print_info "Installing Wing Connector..."
    
    # Create plugins directory if it doesn't exist
    mkdir -p "$REAPER_PLUGINS_DIR"
    
    # Copy plugin
    cp "$BUILD_DIR/$PLUGIN_NAME" "$REAPER_PLUGINS_DIR/"
    print_success "Plugin installed to: $REAPER_PLUGINS_DIR"
    
    # Copy config file
    if [ -f "$CONFIG_FILE" ]; then
        cp "$CONFIG_FILE" "$REAPER_PLUGINS_DIR/"
        print_success "Config file installed"
    else
        print_warning "Config file not found - you'll need to configure manually"
    fi
}

restart_reaper() {
    echo ""
    read -p "$(echo -e ${BLUE}Restart REAPER now? [y/N]:${NC} )" -n 1 -r
    echo ""
    
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        print_info "Stopping REAPER..."
        killall REAPER 2>/dev/null || killall REAPER64 2>/dev/null || true
        sleep 2
        
        print_info "Starting REAPER..."
        if [ -d "/Applications/REAPER.app" ]; then
            open -a REAPER
        elif [ -d "/Applications/REAPER64.app" ]; then
            open -a REAPER64
        else
            open -a REAPER 2>/dev/null || true
        fi
        
        print_success "REAPER restarted"
    fi
}

print_post_install() {
    echo ""
    echo -e "${GREEN}=========================================${NC}"
    echo -e "${GREEN}  Installation Complete!${NC}"
    echo -e "${GREEN}=========================================${NC}"
    echo ""
    echo "Next steps:"
    echo "  1. Open REAPER"
    echo "  2. Go to Extensions → Wing Connector"
    echo "  3. Configure your Behringer Wing IP address"
    echo "  4. Click 'Connect to Wing'"
    echo ""
    echo "Config file location:"
    echo "  $REAPER_PLUGINS_DIR/$CONFIG_FILE"
    echo ""
    echo "Documentation:"
    echo "  README.md - Quick start guide"
    echo "  QUICKSTART.md - Step-by-step instructions"
    echo ""
}

# Main installation flow
main() {
    # Change to script directory
    cd "$(dirname "$0")"
    
    print_header
    
    # Check REAPER installation
    check_reaper || true
    echo ""
    
    # Check if plugin is built
    if ! check_build; then
        echo ""
        read -p "$(echo -e ${BLUE}Build the plugin now? [Y/n]:${NC} )" -n 1 -r
        echo ""
        
        if [[ ! $REPLY =~ ^[Nn]$ ]]; then
            build_plugin
        else
            print_error "Cannot install without building first"
            exit 1
        fi
    fi
    
    echo ""
    
    # Install plugin
    install_plugin
    
    # Offer to restart REAPER
    restart_reaper
    
    # Print post-install instructions
    print_post_install
}

# Run installer
main
