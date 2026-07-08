#!/usr/bin/env sh
set -e
ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
LOG_FILE="replace.txt"
TARGET="$ROOT_DIR/node_modules/react-native-reanimated/android/src/main/java/com/swmansion/reanimated/ReanimatedPackage.java"
BACKUP_SUFFIX=".bak.$(date +%Y%m%d%H%M%S)"

if [ ! -f "$LOG_FILE" ]; then
  echo "ERROR: replace.txt not found at $LOG_FILE"
  exit 1
fi

if [ ! -d "$(dirname "$TARGET")" ]; then
  echo "Target directory does not exist; creating: $(dirname "$TARGET")"
  mkdir -p "$(dirname "$TARGET")"
fi

if [ -f "$TARGET" ]; then
  echo "Backing up existing file to ${TARGET}${BACKUP_SUFFIX}"
  cp "$TARGET" "${TARGET}${BACKUP_SUFFIX}"
fi

cp "$LOG_FILE" "$TARGET"
chmod 644 "$TARGET" || true

echo "Patched: $TARGET (from $LOG_FILE)"

echo "Done. If you want to revert, restore the backup: mv ${TARGET}${BACKUP_SUFFIX} ${TARGET}"
