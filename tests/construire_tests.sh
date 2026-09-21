#!/bin/sh
# Compile les tests hôtes : sources recréées + implémentation hôte minimale.
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/tests
g++ -std=c++17 -Wall -Wextra -Wno-unused-parameter -fno-rtti -Isrc \
	tests/test_surface.cpp src/core/SurfaceTable.cpp tests/hote/FileMgr.cpp \
	-o build/tests/test_surface
echo "build/tests/test_surface prêt"
