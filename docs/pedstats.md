# Config/dat/pedstats.dat

Fichier texte tabulé : 141 lignes dont 75 de données, commentaires `#`.
L'en-tête documente 65 colonnes, A à BM : nom du stat, pickup, probabilités
(n/100), vision, santé, peur, fréquences d'attaque et de blocage,
agressivité, classe de personnage (`Generic`, `Ranged`…), zone et
orientation de combat préférées, vitesses de vélo, dégâts, ténacité,
reversals, arme de nuit, trois modèles de vélo, probabilité d'arme, quatre
emplacements d'arme (type, munitions, poids, mission de déblocage), résistance
à l'étourdissement, renversable, seuil d'humiliation.

## Ce que le binaire en fait

`CPedStats::Initialise` (0x49a0a0) compte les lignes utiles, alloue
`n × 0x11c` octets avec `GameMalloc`, revient au début, puis `CPedStats::ParseLine`
(0x499d80) remplit une entrée par ligne. `0x11c = 284 = 24 + 4 + 64 × 4` :

| Décalage | Taille | Contenu |
|---|---|---|
| +0x00 | 24 | nom (`sscanf "%s"`) |
| +0x18 | 4 | `HashString(nom)` |
| +0x1c | 64 × 4 | les 64 colonnes B à BM, entiers, flottants ou hachés selon la colonne |

`ParseLine` lu dans l'index : chaque colonne est un entier `%d`, sauf

| Colonne | Champ | Conversion |
|---|---|---|
| B (0) | pickup | `"none"` → -1, sinon `GetModelIndexByName` (0x51c1e0) |
| S (0x11) | classe | `GetCharacterClassId` (0x488a00), appelé deux fois |
| AP, AU, AY, BC, BG | armes | `GetWeaponIdByName` (0x51ad50) |
| AQ, AR, AS | vélos | `GetModelIndexByName`, -1 ramené à 0 |
| AX, BB, BF, BJ | mission de déblocage | `"init"` → -1, sinon `GetMissionIdByName` (0x5fa7b0 puis 0x6a9e70) |

`CPedStats::Reload` (0x49a180) relit le fichier dans les entrées existantes.
Recréé dans `src/peds/PedStats.cpp`, vérifié par `tests/test_pedstats` sur le
vrai fichier (75 entrées, valeurs de `STAT_PLAYER` et `STAT_N_EARNEST`).

## Ce qu'un piéton en reçoit (23.10.2026)

`FUN_00478e30` (appelée par le constructeur `FUN_0046dea0` avec l'entrée que le modelinfo du modèle garde en +0x54,
et par `PedSetStatsType`, 0x5caba0) copie les 64 colonnes dans le piéton en +0x1900 (`GameGetPedStat`, `FUN_00476f10`,
les relit), puis : santé maximale +0x1ca4 = colonne F (+0x2c de l'entrée), santé +0x1cb8 et +0x1cbc = ce maximum,
santé minimale +0x1ca8 = 0, régénération +0x1cac = colonne G (+0x30). `GameSetPedStat` (`FUN_00476fa0`) écrit la
colonne et suit 4 (le maximum, la santé ramenée dessous) et 5 (la régénération). À chaque image, `FUN_00472ff0`
(depuis `FUN_004812d0`) ajoute régénération / 100 × 0,02 × `CTimer::ms_fTimeStep` à une santé entre 0 et le maximum ;
le pas valant 1 pour 20 ms (`FUN_0045b810`), c'est régénération / 100 point par seconde. La colonne BM (0x3f), le seuil
d'humiliation en pour cent, sert à `IsHealthAtHumiliationLevel` (`FUN_00601a90`) et BL (0x3e), « peut être renversé », à
la condition `stat` de `HitTree/Standing/Melee` (docs/cat.md).

`lancer_lua` lit le fichier avec `CPedStats::ParseLine` et donne à chaque piéton créé l'entrée de son modèle
(`CScriptPedPool::PoserStats`) : dans `1_02`, le bully (146, `STAT_B_STRIKER_A`) a 95 points de vie, pas de
régénération, un seuil d'humiliation de 15 % ; tests/test_scriptpeds.
