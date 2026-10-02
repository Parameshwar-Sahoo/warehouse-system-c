#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$SCRIPT_DIR/.."

echo "=========================================================="
echo "  WAREHOUSE SYSTEM (PURE C) VERIFICATION PIPELINE         "
echo "=========================================================="

echo "[1/4] Building Linux Character Device Driver..."
cd "$ROOT_DIR/driver"
make

echo "[2/4] Building C Warehouse System & Tests..."
cd "$ROOT_DIR"
make app

echo "[3/4] Running Comprehensive Test Suites..."
make test

echo "[4/4] Executing Automated End-to-End System Demonstration..."
make demo

echo "=========================================================="
echo "  STAGE DEMONSTRATION COMPLETED SUCCESSFULLY (PURE C)     "
echo "=========================================================="
