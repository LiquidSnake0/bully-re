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
g++ $FLAGS tests/test_ide.cpp src/core/IdeBinary.cpp src/core/CdStream.cpp tests/hote/FileMgr.cpp tests/hote/Chemins.cpp tests/hote/Alloc.cpp -o build/tests/test_ide
g++ $FLAGS tests/test_col.cpp src/collision/ColModel.cpp src/core/CdStream.cpp tests/hote/FileMgr.cpp tests/hote/Chemins.cpp tests/hote/Alloc.cpp -o build/tests/test_col
g++ $FLAGS tests/test_ipl.cpp src/core/IplFile.cpp src/core/CdStream.cpp tests/hote/FileMgr.cpp tests/hote/Chemins.cpp tests/hote/Alloc.cpp -o build/tests/test_ipl
g++ $FLAGS tests/test_nif.cpp src/gamebryo/NifFile.cpp src/core/CdStream.cpp tests/hote/FileMgr.cpp tests/hote/Chemins.cpp tests/hote/Alloc.cpp -o build/tests/test_nif
g++ $FLAGS tests/test_nft.cpp src/gamebryo/NifFile.cpp src/core/CdStream.cpp tests/hote/FileMgr.cpp tests/hote/Chemins.cpp tests/hote/Alloc.cpp -o build/tests/test_nft
# test_world ne lit aucun fichier du jeu : il vérifie l'arithmétique de la grille et les listes.
g++ $FLAGS tests/test_world.cpp src/core/World.cpp src/core/Lists.cpp src/entities/Physical.cpp src/entities/Entity.cpp tests/hote/Entite.cpp -o build/tests/test_world
echo "tests prêts : test_surface, test_pedstats, test_carcols, test_handling, test_objectdata, test_cdstream, test_ide, test_col, test_ipl, test_nif, test_nft, test_world"
