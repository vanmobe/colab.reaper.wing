#!/usr/bin/env bash
set -euo pipefail

VERSION="${1:-1.1.0}"
STAGE_DIR="${2:-stage}"
OUT_DIR="${3:-releases}"

PLUGIN_NAME="reaper_wingconnector.so"
CONFIG_NAME="config.json"
PACKAGE_NAME="audiolab-virtual-soundcheck"
ARCH="${ARCH_OVERRIDE:-$(dpkg --print-architecture)}"

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
PKG_DIR="$TMP_ROOT/${PACKAGE_NAME}_${VERSION}_${ARCH}"
SHARE_DIR="/usr/share/${PACKAGE_NAME}"
mkdir -p "$PKG_DIR/DEBIAN" "$PKG_DIR${SHARE_DIR}"

cp "$STAGE_DIR/$PLUGIN_NAME" "$PKG_DIR${SHARE_DIR}/$PLUGIN_NAME"
cp "$STAGE_DIR/$CONFIG_NAME" "$PKG_DIR${SHARE_DIR}/$CONFIG_NAME"

cat > "$PKG_DIR/DEBIAN/control" << EOF
Package: ${PACKAGE_NAME}
Version: ${VERSION}
Section: sound
Priority: optional
Architecture: ${ARCH}
Maintainer: CO LAB <noreply@example.com>
Description: AUDIOLAB Virtual Soundcheck for REAPER and Behringer WING
 Installs the REAPER AUDIOLAB Virtual Soundcheck plugin and config file.
EOF

cat > "$PKG_DIR/DEBIAN/postinst" << 'EOF'
#!/usr/bin/env bash
set -euo pipefail

SOURCE_DIR="/usr/share/audiolab-virtual-soundcheck"
for home_dir in /home/*; do
  [[ -d "$home_dir" ]] || continue
  user_name="$(basename "$home_dir")"
  if ! id -u "$user_name" >/dev/null 2>&1; then
    continue
  fi

  target_dir="$home_dir/.config/REAPER/UserPlugins"
  mkdir -p "$target_dir"
  cp -f "$SOURCE_DIR/reaper_wingconnector.so" "$target_dir/reaper_wingconnector.so"
  if [[ ! -f "$target_dir/config.json" ]]; then
    cp -f "$SOURCE_DIR/config.json" "$target_dir/config.json"
  fi
  chown "$user_name:$user_name" "$target_dir/reaper_wingconnector.so" || true
  [[ -f "$target_dir/config.json" ]] && chown "$user_name:$user_name" "$target_dir/config.json" || true
done

exit 0
EOF
chmod 755 "$PKG_DIR/DEBIAN/postinst"

cat > "$PKG_DIR/DEBIAN/postrm" << 'EOF'
#!/usr/bin/env bash
set -euo pipefail

for home_dir in /home/*; do
  [[ -d "$home_dir" ]] || continue
  target_dir="$home_dir/.config/REAPER/UserPlugins"
  rm -f "$target_dir/reaper_wingconnector.so"
done

exit 0
EOF
chmod 755 "$PKG_DIR/DEBIAN/postrm"

OUT_FILE="$OUT_DIR/AUDIOLAB-Virtual-Soundcheck-v${VERSION}-${ARCH}.deb"
dpkg-deb --build "$PKG_DIR" "$OUT_FILE" >/dev/null
rm -rf "$TMP_ROOT"

echo "Created $OUT_FILE"
