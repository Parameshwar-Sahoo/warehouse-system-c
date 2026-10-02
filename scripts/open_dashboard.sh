#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DASHBOARD_FILE="$SCRIPT_DIR/../web/index.html"

echo "=== Opening Warehouse System Interactive Dashboard ==="

if command -v wslview >/dev/null 2>&1; then
    wslview "$DASHBOARD_FILE"
elif command -v xdg-open >/dev/null 2>&1; then
    xdg-open "$DASHBOARD_FILE"
elif [ -f "/mnt/c/Windows/explorer.exe" ]; then
    /mnt/c/Windows/explorer.exe "D:\\warehouse-system\\web\\index.html"
else
    echo "Please open in browser: $DASHBOARD_FILE"
fi
