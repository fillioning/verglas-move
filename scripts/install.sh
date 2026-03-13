#!/bin/bash
# Install Verglas module to Ableton Move via SSH
set -e

MOVE_IP="${1:-move.local}"
MODULE_DIR="$(cd "$(dirname "$0")/.." && pwd)/modules/verglas"

if [ ! -f "$MODULE_DIR/verglas.so" ]; then
    echo "Error: verglas.so not found. Run build.sh first."
    exit 1
fi

echo "Installing Verglas to $MOVE_IP..."
DEST="/data/UserData/move-anything/modules/audio_fx/verglas"
ssh root@$MOVE_IP "mkdir -p $DEST"
scp "$MODULE_DIR/verglas.so" "$MODULE_DIR/module.json" "$MODULE_DIR/ui_chain.js" root@$MOVE_IP:$DEST/
if [ -f "$MODULE_DIR/help.json" ]; then
    scp "$MODULE_DIR/help.json" root@$MOVE_IP:$DEST/
fi
ssh root@$MOVE_IP "chown -R ableton:users $DEST"

echo "Done. Restart Move-Anything to load Verglas."
