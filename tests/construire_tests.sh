#!/bin/sh
# Compile les tests hôtes : sources recréées + implémentation hôte minimale.
# Lancer ensuite avec BULLY_DATA=<racine du jeu>.
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/tests
FLAGS="-std=c++17 -Wall -Wextra -Wno-unused-parameter -fno-rtti -Isrc"
g++ $FLAGS tests/test_surface.cpp src/core/SurfaceTable.cpp tests/hote/FileMgr.cpp tests/hote/Chemins.cpp -o build/tests/test_surface
g++ $FLAGS tests/test_pedstats.cpp src/peds/PedStats.cpp tests/hote/Lookups.cpp tests/hote/Chemins.cpp -o build/tests/test_pedstats
g++ $FLAGS tests/test_carcols.cpp src/vehicles/VehicleColours.cpp tests/hote/Vehicules.cpp tests/hote/Chemins.cpp -o build/tests/test_carcols
g++ $FLAGS tests/test_handling.cpp src/vehicles/HandlingMgr.cpp tests/hote/Handling.cpp tests/hote/FileMgr.cpp tests/hote/Chemins.cpp -o build/tests/test_handling
g++ $FLAGS tests/test_objectdata.cpp src/objects/ObjectData.cpp src/core/Tokenizer.cpp tests/hote/Objets.cpp tests/hote/Vehicules.cpp tests/hote/Chemins.cpp -o build/tests/test_objectdata
g++ $FLAGS tests/test_cdstream.cpp src/core/CdStream.cpp tests/hote/FileMgr.cpp tests/hote/Chemins.cpp tests/hote/Alloc.cpp -o build/tests/test_cdstream
echo "tests prêts : test_surface, test_pedstats, test_carcols, test_handling, test_objectdata, test_cdstream"
