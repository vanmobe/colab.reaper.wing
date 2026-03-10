#!/usr/bin/env bash
set -euo pipefail

VERSION="${1:-1.1.0}"
STAGE_DIR="${2:-stage}"
OUT_DIR="${3:-releases}"

PLUGIN_NAME="reaper_wingconnector.dylib"
CONFIG_NAME="config.json"
PKG_ID="com.audiolab.wing.reaper.virtualsoundcheck"
INSTALL_TARGET="/Library/Application Support/REAPER/UserPlugins"

if ! command -v pkgbuild >/dev/null 2>&1; then
  echo "pkgbuild is required to create macOS installer packages" >&2
  exit 1
fi

if [[ ! -f "$STAGE_DIR/$PLUGIN_NAME" ]]; then
  echo "Missing $STAGE_DIR/$PLUGIN_NAME" >&2
  exit 1
fi
if [[ ! -f "$STAGE_DIR/$CONFIG_NAME" ]]; then
  echo "Missing $STAGE_DIR/$CONFIG_NAME" >&2
  exit 1
fi

mkdir -p "$OUT_DIR"
TMP_ROOT="$(mktemp -d)"
PKG_ROOT="$TMP_ROOT/pkg_root$INSTALL_TARGET"
PKG_SCRIPTS="$TMP_ROOT/scripts"
mkdir -p "$PKG_ROOT" "$PKG_SCRIPTS"

cp "$STAGE_DIR/$PLUGIN_NAME" "$PKG_ROOT/$PLUGIN_NAME"
cp "$STAGE_DIR/$CONFIG_NAME" "$PKG_ROOT/$CONFIG_NAME"

cat > "$PKG_SCRIPTS/postinstall" << 'POSTINSTALL'
#!/usr/bin/env bash
set -euo pipefail

USER_PLUGINS_DIR="$HOME/Library/Application Support/REAPER/UserPlugins"
SYSTEM_PLUGINS_DIR="/Library/Application Support/REAPER/UserPlugins"

if [[ -d "$HOME/Library/Application Support/REAPER" ]]; then
  mkdir -p "$USER_PLUGINS_DIR"
  cp -f "$SYSTEM_PLUGINS_DIR/reaper_wingconnector.dylib" "$USER_PLUGINS_DIR/"
  cp -f "$SYSTEM_PLUGINS_DIR/config.json" "$USER_PLUGINS_DIR/" || true
  chmod 755 "$USER_PLUGINS_DIR/reaper_wingconnector.dylib"
fi

exit 0
POSTINSTALL
chmod +x "$PKG_SCRIPTS/postinstall"

OUT_FILE="$OUT_DIR/AUDIOLAB-Virtual-Soundcheck-v$VERSION-macOS.pkg"
pkgbuild \
  --root "$TMP_ROOT/pkg_root" \
  --scripts "$PKG_SCRIPTS" \
  --identifier "$PKG_ID" \
  --version "$VERSION" \
  --install-location "/" \
  "$OUT_FILE"

rm -rf "$TMP_ROOT"

echo "Created $OUT_FILE"
