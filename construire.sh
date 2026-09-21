#!/bin/sh
# Compile chaque source recréée en objet, sans édition de liens : on vérifie
# seulement que le C++ est valide et cohérent. Pas de make sur la machine.
set -u
cd "$(dirname "$0")"
CXX=${CXX:-g++}
FLAGS="-std=c++17 -Wall -Wextra -Wno-unused-parameter -fno-rtti -c"
ok=0; ko=0
for src in $(find src -name '*.cpp' -not -path 'src/squelettes/*' | sort); do
	obj="build/${src#src/}"; obj="${obj%.cpp}.o"
	mkdir -p "$(dirname "$obj")"
	if $CXX $FLAGS "$src" -o "$obj"; then ok=$((ok+1)); else ko=$((ko+1)); fi
done
echo "objets : $ok ok, $ko en erreur"
[ "$ko" -eq 0 ]
