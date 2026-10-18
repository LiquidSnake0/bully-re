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

## Reste à faire

- Les banques de sons (`LoadCutsceneSound`, `SoundPlay…`) et la durée d'une ligne.
