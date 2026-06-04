#!/bin/sh
# CifraSync Launcher - "wake up cifra"
# Builds (if needed) and launches CifraSync in interactive mode.

cd "$(dirname "$0")" || exit 1

if [ ! -f "bin/cifrasync" ]; then
    echo ""
    echo "  [CifraSync] Building project..."
    make release || { echo "  [ERROR] Build failed."; exit 1; }
    echo "  [CifraSync] Build complete."
fi

echo ""
echo "  +-----------------------------------------+"
echo "  |        C I F R A S Y N C                |"
echo "  |    Encrypted Incremental Backup & Sync  |"
echo "  +-----------------------------------------+"
echo ""
echo "  Waking up CifraSync..."
echo ""
exec ./bin/cifrasync
