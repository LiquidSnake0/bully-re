# Ce qui vient après

Liste tenue à jour à chaque séance : le premier point ouvert est le prochain
chantier. Un point fini passe en bas, avec son commit.

## Ouvert

1. **Les scènes d'événements à deux piétons.** `Scripts/EventFunc.lur` relie
   des fonctions d'événement à des scènes d'`Ambient.cat` jouées à deux :
   `F_HeldAgainstWall` → `Wall_Hold`, `F_LockerStuff` → `LockerStuff`,
   `F_Swirlie` → `Swirlie`, `F_TeacherHarassingKids` → `Tell_Off`,
   `F_CoupleCuddling` → `Cuddle`, `F_CoupleKissing` → `Kiss_Me_Baby`.
   `Wall_Hold` et `LockerStuff` sont faits (voir « Fait »). Restent `Swirlie`,
   `Tell_Off` et `Cuddle` (`F_CoupleKissing` n'a aucun point dans Trigger.img) : même démarche, lire l'arbre
   (`tools/cat.py Ambient.cat --pistes`), mesurer les bassins des deux
   animations (`build/outils/sonde_poi <groupe>`), poser la paire dans
   `outils/visite.cpp` (variable `sceneDuo`) et vérifier à l'image.
2. **TargetSync** (décision à prendre) : passer au modèle « chacun sur sa
   flèche + TargetSync » pour toutes les paires (prises, bagarres, couples)
   au lieu de l'origine commune et des reculs ajustés à la main. Gain : le
   vol d'arme et les prises sans flèche se placent juste. Risque : régression
   du placement actuel, qui tient à l'image. Voir `docs/cat.md`, TargetSync.
3. **Les autres encodages des `.agr`** (objets animés : un sur cinq des
   groupes), `docs/agr.md`.
4. **Lua 5.0** : embarquer la VM et brancher l'API de `docs/api-lua.txt`
   (914 noms), pour que les scripts de mission tournent.
5. **Outillage : REA** (github.com/morluto/rea, MCP, installé le 06.10.2026 : rea-agents 4.0.1, Ghidra 12.1.4 et JDK 21 dans ~/opt) pour interroger Ghidra
   directement au lieu des exports headless. Prérequis : Ghidra 12.1.4 et
   JDK 21 en local (aujourd'hui Ghidra tourne en Docker). Garder l'index
   `export/index.sqlite` comme première source.

## Fait

- Transitions attente / marche, fondus os par os : c2064a9.
- Trajets de `DAT/Trigger.img` et patrouilles : 23746ad, 4c129a1.
- Scènes d'événements à un piéton : bf0ba10.
- Scène à deux `Wall_Hold` (`F_HeldAgainstWall`) : voir le commit « Visite : plaqué au mur ».
- Le format `.lip` (synchronisation labiale) : `src/anim/Lip`, `tests/test_lip`, `docs/lip.md` ; confirmé dans `bully.exe` par l'index et REA (chargeur `FUN_0068df50`, lecture `FUN_0068dd30`).
- Scène à deux `LockerStuff` (`F_LockerStuff`) : la prise debout de StuffGrap, sans le casier ; commit « Visite : fourré au casier ».
