#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DRIVER_DIR="$SCRIPT_DIR/../driver"

echo "=== Loading Linux Character Device Driver (C) ==="
cd "$DRIVER_DIR"
if [ ! -f wms_driver.ko ]; then
    make
fi

sudo insmod wms_driver.ko
echo "Module inserted successfully."

if [ -e /dev/wms_driver ]; then
    sudo chmod 666 /dev/wms_driver
    echo "Device /dev/wms_driver permissions set to 666."
fi

dmesg | tail -n 5
