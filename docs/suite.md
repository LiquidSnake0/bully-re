# Ce qui vient après

Liste tenue à jour à chaque séance : le premier point ouvert est le prochain
chantier. Un point fini passe en bas, avec son commit.

## Ouvert

1. **Les autres encodages des `.agr`** (objets animés : un sur cinq des
   groupes), `docs/agr.md`.
2. **Lua 5.0** : embarquer la VM et brancher l'API de `docs/api-lua.txt`
   (914 noms), pour que les scripts de mission tournent.
3. **Outillage : REA** (fork github.com/LiquidSnake0/rea, branche `delai-ghidra-reglable`, rebasée sur rea-agents 4.1.0 le 07.10 ; **à chaque séance, regarder les commits amont** : `git fetch upstream --tags` : délai Ghidra réglable, 15 min ; Ghidra 12.1.4 et JDK 21 dans ~/opt ; l’analyse de Bully.exe prend environ 6 min 30, à sauver en snapshot à la première ouverture ; **attention** : le snapshot de `close_binary` ne garde que les résultats de requêtes déjà faites (2,8 Ko, 0 entrée le 07.10), pas l'analyse Ghidra : chaque ouverture refait les 6 min 30, donc garder la session ouverte tant qu'on s'en sert) pour interroger Ghidra
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
- TargetSync : logique du moteur trouvée (`FUN_0060f350`, confirmée par REA) et mesurée contre les flèches : les paires à flèche (prises, bagarres, Wall_Hold, LockerStuff, Swirlie) étaient déjà justes ; les paires sans flèche (sermon, harcèlement, Cuddle) l'appliquent ; le baiser garde son réglage à l'image. Commit « TargetSync : les paires mesurées ».
- Les scènes d'événements à deux piétons, toutes posées (`F_CoupleKissing` n'a aucun point dans Trigger.img). Dernière : `Swirlie` (`F_Swirlie`), qui exécute l'empoignade de LockerStuff, sans l'objet toilettes ; commit « Visite : la tête dans les toilettes ».
- Scène à deux `Tell_Off` (`F_TeacherHarassingKids`) : les boucles 1001 se lisaient déjà ; le vrai blocage était HOLD_IDLE un cran plus bas et le placement (TargetSync 0,9 m, face à face) ; commit « Visite : le sermon ».
- Scène à deux `Cuddle` (`F_CoupleCuddling`), placement côte à côte, et condition IsScriptedAmbient vraie pour les événements scriptés : commit « Visite : bras dessus, bras dessous ».
