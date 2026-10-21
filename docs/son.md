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
moteur, une ligne jouée dure 2,5 s (la durée réelle, dans les banques de sons, n'est pas lue),
et `SoundSpeechPlaying` répond selon le locuteur, l'événement et la ligne. Sur `1_01`, Jimmy (le piéton 0)
dit `M_1_01` ligne 101 à la secrétaire avant la réussite ; le script attend la fin, et la mission se
termine à 12,1 s au lieu de 9,7.

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

## Reste à faire

- Les banques de sons (`LoadCutsceneSound`, `SoundPlay…`) et la durée d'une ligne.
