# Le son : la parole des scripts

Établi le 18.10.2026 dans l'index Ghidra et `bully.exe`. Code : `src/audio/SpeechEvents`, test
`tests/test_speech` (sans fichier du jeu).

## Les événements de parole

Un script fait parler un piéton par `SoundPlayScriptedSpeechEvent(ped, "M_1_01", 101)` : un
**nom d'événement** et un **numéro de ligne**. Les noms sont une table de `bully.exe` en
**0x00ae74e8** : 493 pointeurs de chaînes, de `ALLY_ABOUT_TO_LEAVE` à `YEARBOOK`, finis par
`NUM_SPEECH_EVENTS`. 98 sont des missions (`M_1_01`, `M_1_02`, `M_1_02A`…), les autres des
situations (`PRINCIPAL_LECTURING`, `WARNING_TRESPASSING`, `WTF_TV`…). `src/audio/SpeechEvents`
reprend la table dans l'ordre de l'exe (`kSpeechEvents`) ; `SpeechEventIndex` la cherche sans la
casse.

| Fonction Lua | Adresse | Ce qu'elle fait |
|---|---|---|
| `SoundPlayScriptedSpeechEvent(ped, nom, ligne[, banque, b, b])` | 0x5d37b0 | parcourt la table avec `stricmp` jusqu'au nom (ou jusqu'à `NUM_SPEECH_EVENTS` : alors elle sort sans rien rendre) ; `FUN_005a4980(index, ligne, ped, …)` prépare la ligne : l'index sur 9 bits (bits 0 à 8, refusé au-delà de 0x1ec = 492 : « Bad index number »), le numéro de ligne sur 11 bits, le modèle du locuteur dans les bits 9 à 16 ; le script reçoit `DAT_00cf4e00`, l'identifiant, −2 si le modèle n'a pas cet événement (« ATTENTION SCRIPTER … this model does not have this speech event ») |
| `SoundSpeechPlaying([ped[, nom[, ligne]]])` | 0x5d3a50 | `FUN_005a27b0` : sans piéton, quelqu'un parle (deux emplacements de 0x5c octets en `DAT_00cf458c`, alternés par `DAT_00cf4644`) ; avec un piéton, c'est lui qui parle, et de cet événement (`& 0x1ff`) et de cette ligne (`& 0x7ff`) si le script les donne |

Dans `lancer_lua`, les deux sont réelles sur la table : un nom absent est refusé comme dans le
moteur, une ligne jouée dure le temps de son échantillon (« Les fichiers de parole »),
et `SoundSpeechPlaying` répond selon le locuteur, l'événement et la ligne. Sur `1_01`, Jimmy (le piéton 0)
dit `M_1_01` ligne 101 à la secrétaire avant la réussite ; le script attend la fin, et la mission se
termine à 10,5 s (la ligne, `Jimmy_PGREEAF_v6`, dure 0,80 s).

## Le préchargement d'une ligne

Les cinématiques sans film (`1_02` en joue une au début, la narration de Jimmy) préparent leurs
lignes une à une : `SoundPreloadSpeech(0, "NARRATION", 101, "supersize")`, attente de
`SoundIsSpeechPreloaded()`, `SoundPlayPreloadedSpeech()` quand la caméra est en place.

| Fonction Lua | Adresse | Ce qu'elle fait |
|---|---|---|
| `SoundPreloadSpeech(ped, nom, ligne[, taille, b, b])` | 0x5d4a20 | la même recherche du nom que `SoundPlayScriptedSpeechEvent` (absent : rien) ; puis le bloc **`DAT_00cfb3b4`** est remis à zéro : +0 en attente (1), +4 échéance = `DAT_00c1a9b4` + 30 000 ms, +8 jouer (0), +9 préchargée (0), +0xc le locuteur ; `FUN_005a4980` reçoit le bloc et le rappel `FUN_005d49d0` ; le script reçoit l'identifiant |
| `SoundIsSpeechPreloaded()` | 0x5d39e0 | rend l'octet +9 (`DAT_00cfb3bd`) |
| `SoundPlayPreloadedSpeech()` | 0x5d39d0 | pose l'octet +8 (`DAT_00cfb3bc`) |

Le rappel **`FUN_005d49d0(prête, bloc)`** décide : prête, la ligne devient préchargée (+9) ; tant
qu'elle attend et que « jouer » n'est pas posé, il la retient (rend 0), et l'annule une fois
l'échéance passée (`FUN_005a5880(locuteur, 3, 0x19)`) ; sinon il la sort de l'attente et la laisse
jouer (rend 1). `CSpeechPreload` (`src/audio/SpeechEvents`) le refait, `tests/test_speech` le
vérifie. Dans `lancer_lua`, la ligne est prête à l'image suivante (pas de diffusion) et le rappel
passe à chaque image : `1_02` dit ses quatre lignes de `NARRATION` (101 à 104), la secrétaire
`M_1_02` 101 et 123, puis la mission rend la main au joueur et lui donne l'objectif du dortoir
(`1_02B_LOGOBJ04`).

## Les fichiers de parole

`audio\CONFIG\SpeechDB.bin` (chargé par `FUN_005a5ea0` → `FUN_0059f2f0`) donne, pour chacun des 493
événements, ses éléments : un en-tête de 8 octets (+0 le numéro, +2 le nombre d'éléments, +4 des drapeaux,
+6 le dernier élément tiré) puis 4 octets par élément : **+0 la voix, +1 la ligne, +2 le numéro
d'échantillon**. La voix est le modèle du locuteur : `FUN_005a4980` met le mot +0x10e du piéton, son
indice de modèle (`CEntity::SetModelIndexNoCreate`, 0x465cd0), aux bits 9 à 16 de l'identifiant.
`FUN_0059ee10` retient les éléments de la bonne voix (`FUN_0059e790` : la même, ou une des voix
partagées de sa liste, pas lue) et de la bonne ligne (les 11 bits demandés comparés à l'octet : une
ligne au-delà de 255 ne trouve rien), pas encore joués (un bit par échantillon en +0xc8c) ;
`FUN_0059f4d0` en tire un par le rappel de hasard sans reprendre le dernier, et rend son numéro.

`audio\PLAYLIST\Speech.bin` (1,4 Go, ouvert par `FUN_005a6790`) commence par « Hash », le nombre
d'échantillons (40 400) et une table de 12 octets par échantillon (+0 la clé, +4 la position, +8 la
taille), lue par `FUN_005a7b00` et cherchée par `bsearch` (`FUN_005a7c80`) : triée par clé, et **le
numéro d'échantillon est l'indice dans cette table**. La clé est le hachage `FUN_005a7c10` du nom de
fichier (minuscules, `\` lu `/`, `u = (u + c) * 0x401 ; u ^= u >> 6`, puis `(9u >> 11 ^ 9u) * 0x8001`) :
`audio\PLAYLIST\Speech.lst`, que le moteur ne lit pas, donne les 40 400 noms dans l'ordre de la table,
et chacun retombe sur sa clé. Chaque échantillon est une banque de sons XACT (`SDBK`) suivie, à +0x800,
d'une banque d'ondes (`WBND`, version 44) d'une seule onde, en ADPCM, autour de 22 050 Hz : l'entrée de
l'onde porte sa **durée en échantillons** (28 bits) et sa fréquence (18 bits du format). 27 h de parole.

`CSpeechBank` (`src/audio/SpeechBank`) lit les deux fichiers ; `tests/test_speechbank` vérifie les
comptes (493 événements, 40 882 éléments, 40 400 échantillons), l'ordre et les clés, et la narration
de `1_02` : `NARRATION` 101 à 104 par Jimmy (modèle 0) sont `Jimmy_NARR1_01_100` à `NARR1_02_101`
(3,4 à 5,5 s), la secrétaire (modèle 59) dit `M_1_02` 101 en 2,29 s. Dans `lancer_lua`, une ligne dure
son échantillon : le premier candidat dans l'ordre du fichier (le moteur tire au hasard parmi les
prises), 2,5 s s'il n'y en a aucun.

## Reste à faire

- Les banques de sons (`LoadCutsceneSound`, `SoundPlay…`) et la durée d'une ligne.
