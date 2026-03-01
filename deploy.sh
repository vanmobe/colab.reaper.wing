#!/bin/bash
# Deploy script for Wing Connector
# This builds and deploys both the plugin and config file

set -e  # Exit on error

echo "Building Wing Connector..."
cd "$(dirname "$0")/build"
make -j4

echo "Deploying to REAPER..."
PLUGIN_DIR="$HOME/Library/Application Support/REAPER/UserPlugins"

# Copy plugin
cp reaper_wingconnector.dylib "$PLUGIN_DIR/"

# Copy config file
cp ../config.json "$PLUGIN_DIR/"

echo "✓ Plugin deployed to: $PLUGIN_DIR"
echo "✓ Config deployed"

# Optionally restart REAPER
read -p "Restart REAPER now? (y/n) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    killall REAPER 2>/dev/null || true
    sleep 2
    open -a REAPER
    echo "✓ REAPER restarted"
fi
