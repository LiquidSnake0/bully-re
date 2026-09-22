#!/bin/sh
# Compile les tests hôtes : sources recréées + implémentation hôte minimale.
# Lancer ensuite avec BULLY_DATA=<racine du jeu>.
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/tests
FLAGS="-std=c++17 -Wall -Wextra -Wno-unused-parameter -fno-rtti -Isrc"
g++ $FLAGS tests/test_surface.cpp src/core/SurfaceTable.cpp tests/hote/FileMgr.cpp -o build/tests/test_surface
g++ $FLAGS tests/test_pedstats.cpp src/peds/PedStats.cpp tests/hote/Lookups.cpp -o build/tests/test_pedstats
echo "build/tests/test_surface et build/tests/test_pedstats prêts"
