#!/bin/bash
set -e

echo "=== Unloading Linux Character Device Driver (C) ==="
sudo rmmod wms_driver || {
    echo "Module wms_driver was not loaded."
    exit 0
}
echo "Module unloaded successfully."
dmesg | tail -n 5
