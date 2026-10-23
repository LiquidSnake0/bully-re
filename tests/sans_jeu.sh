#!/bin/sh
# Lance les tests qui ne lisent aucun fichier du jeu : ceux que la CI peut jouer.
# Les autres demandent BULLY_DATA et une installation du jeu (README).
set -eu
cd "$(dirname "$0")/.."
ko=0
for t in test_camera test_luaapi test_marche test_scriptpeds test_speech test_world test_zones; do
	if "build/tests/$t" >/dev/null 2>&1; then echo "ok  $t"; else echo "KO  $t"; ko=$((ko+1)); fi
done
echo "tests sans le jeu : $ko en erreur"
[ "$ko" -eq 0 ]
