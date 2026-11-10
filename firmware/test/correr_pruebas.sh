#!/usr/bin/env bash
# Compila y ejecuta en la PC las pruebas unitarias de la logica pura del firmware.
# Requiere g++ (Linux/macOS; en Windows usar WSL o MSYS2).
set -euo pipefail
cd "$(dirname "$0")"
SRC=../banda_clasificadora
mkdir -p build
g++ -std=c++17 -Wall -Wextra -Werror -O0 -g \
    test_logica.cpp "$SRC/protocolo.cpp" "$SRC/tabla.cpp" "$SRC/pendientes.cpp" "$SRC/deteccion.cpp" \
    -o build/test_logica
./build/test_logica
