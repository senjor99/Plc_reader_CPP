#!/usr/bin/env bash
set -euo pipefail

clear
echo "Hello from my script!"

BUILD_DIR=build/linux
BIN_DIR=bin/linux

cmake -S . -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_GUI=ON -DWITH_SNAP7=ON -WITH_TAO=ON

cmake --build "$BUILD_DIR" -j

$BIN_DIR/plc_reader

