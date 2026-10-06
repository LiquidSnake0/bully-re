# Ce qui vient après

Liste tenue à jour à chaque séance : le premier point ouvert est le prochain
chantier. Un point fini passe en bas, avec son commit.

## Ouvert

1. **Les scènes d'événements à deux piétons.** `Scripts/EventFunc.lur` relie
   des fonctions d'événement à des scènes d'`Ambient.cat` jouées à deux :
   `F_HeldAgainstWall` → `Wall_Hold`, `F_LockerStuff` → `LockerStuff`,
   `F_Swirlie` → `Swirlie`, `F_TeacherHarassingKids` → `Tell_Off`,
   `F_CoupleCuddling` → `Cuddle`, `F_CoupleKissing` → `Kiss_Me_Baby`.
   `Wall_Hold` est fait (voir « Fait »). Restent `LockerStuff`, `Swirlie`,
   `Tell_Off`, `Cuddle` et `Kiss_Me_Baby` : même démarche, lire l'arbre
   (`tools/cat.py Ambient.cat --pistes`), mesurer les bassins des deux
   animations (`build/outils/sonde_poi <groupe>`), poser la paire dans
   `outils/visite.cpp` (variable `sceneDuo`) et vérifier à l'image.
2. **TargetSync** (décision à prendre) : passer au modèle « chacun sur sa
   flèche + TargetSync » pour toutes les paires (prises, bagarres, couples)
   au lieu de l'origine commune et des reculs ajustés à la main. Gain : le
   vol d'arme et les prises sans flèche se placent juste. Risque : régression
   du placement actuel, qui tient à l'image. Voir `docs/cat.md`, TargetSync.
3. **Le format `.lip`** (493 fichiers de World.img, synchronisation labiale).
   Relevé du 06.10.2026, à confirmer dans le code (`FUN_0052e990`, qui forme
   le nom `lipfile_%03d.lip`) :
   - une table d'entrées de 24 octets, sans en-tête : hachage sur 8 octets
     (16 bits du haut à zéro), durée en secondes (`float`), nombre d'images
     `n` (`uint16`), départ dans les données (`uint16`, en octets), puis
     décalage et taille du son dans sa banque (`uint32`, multiples de 0x800) ;
   - puis les données : `ceil(n / 4)` octets par réplique, contigus, soit
     2 bits par image, environ 33,5 images par seconde ;
   - 468 fichiers non vides, 40 882 répliques ; valeurs 0 (2 039 257),
     1 (297 951), 2 (528 108), 3 (436 438) ;
   - **non tranché** : l'ordre des bits dans l'octet (poids faibles ou forts
     d'abord ; les octets de fin ne sont pas remplis de zéros, ce test ne
     décide pas) et ce que valent 0-3 (ouverture de la bouche ou visème).
4. **Les autres encodages des `.agr`** (objets animés : un sur cinq des
   groupes), `docs/agr.md`.
5. **Lua 5.0** : embarquer la VM et brancher l'API de `docs/api-lua.txt`
   (914 noms), pour que les scripts de mission tournent.
6. **Outillage : REA** (github.com/morluto/rea, MCP, installé le 06.10.2026 : rea-agents 4.0.1, Ghidra 12.1.4 et JDK 21 dans ~/opt) pour interroger Ghidra
   directement au lieu des exports headless. Prérequis : Ghidra 12.1.4 et
   JDK 21 en local (aujourd'hui Ghidra tourne en Docker). Garder l'index
   `export/index.sqlite` comme première source.

## Fait

- Transitions attente / marche, fondus os par os : c2064a9.
- Trajets de `DAT/Trigger.img` et patrouilles : 23746ad, 4c129a1.
- Scènes d'événements à un piéton : bf0ba10.
- Scène à deux `Wall_Hold` (`F_HeldAgainstWall`) : voir le commit « Visite : plaqué au mur ».
