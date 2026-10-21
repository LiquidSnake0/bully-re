# Ce qui vient après

Liste tenue à jour à chaque séance : le premier point ouvert est le prochain
chantier. Un point fini passe en bas, avec son commit.

## Ouvert

1. **Lua** (`docs/lua.md`) : la VM 5.0.2 est embarquée, les 515 scripts se
   chargent, et `build/outils/lancer_lua` fait tourner `main.lua` et `gamemain` avec
   leurs threads, image par image, sans erreur. La liste des missions est un
   arbre d'actions (`docs/missions.md`, `tools/missions.py`) et la primitive de
   lancement est `ForceStartMission(nom)`. `ForceStartMission` est branché dans
   `lancer_lua` (`--mission 1_01`) : `1_01` tourne jusqu'à la création des
   personnages (`PedCreatePoint`, docs/missions.md). `POINTLIST`, `PATH` et `TRIGGER` sont réelles depuis `DATLoad`. Les piétons sont un pool
   (`PedCreatePoint` & co réels) ; `1_01` attend le joueur dans `_1_01_BULLYTRIG`. `--entrer`,
   `--aller` et `--zone` jouent le joueur, et les globales de script valent nil comme dans
   le jeu (`src/core/LuaApi`) : **`1_01` se termine réussie**. Les piétons marchent, courent et se battent
   (`CScriptPedPool::Avancer`). `MODELENUM` est lue. Suite : les cinématiques et les attentes, qui démarre `1_01` à la nouvelle partie, puis brancher les fonctions que
   `gamemain` appelle (zones, effets, joueur) en remplaçant les bouchons. Les zones sont faites
   (`src/core/AreaManager`, docs/zones.md) : le changement de zone est joué comme `FUN_00416e30`,
   les scripts de zone (`MainMap.lua`, `SchoolHallways.lua`) sont chargés pour de vrai. Les fichiers
   des cinématiques sont lus et `lancer_lua` les joue (docs/cinematiques.md : `1-1-1` dure 61,7 s
   dans `MissionSetup` de `1_01`, `--sans-cinematiques` pour les sauter). Les conditions `MissionCond`,
   `TimeCond`, `ConditionFalse` sont lues : `1_02` s'enchaîne après `1_01`. Suite : les portes `DT_` (l'objet de porte, +0x394) ; jouer les nœuds d'action des piétons.

3. **Outillage : REA** (fork github.com/LiquidSnake0/rea, branche `delai-ghidra-reglable`, rebasée sur rea-agents 4.1.0 le 07.10 ; **à chaque séance, regarder les commits amont** : `git fetch upstream --tags` : délai Ghidra réglable, 15 min ; Ghidra 12.1.4 et JDK 21 dans ~/opt ; l’analyse de Bully.exe prend environ 6 min 30, à sauver en snapshot à la première ouverture ; **attention** : le snapshot de `close_binary` ne garde que les résultats de requêtes déjà faites (2,8 Ko, 0 entrée le 07.10), pas l'analyse Ghidra : chaque ouverture refait les 6 min 30, donc garder la session ouverte tant qu'on s'en sert) pour interroger Ghidra
   directement au lieu des exports headless. Prérequis : Ghidra 12.1.4 et
   JDK 21 en local (aujourd'hui Ghidra tourne en Docker). Garder l'index
   `export/index.sqlite` comme première source.
   - **État au 08.10.2026** : l'amont est passé à rea-agents 6.0.0 ; la branche se rebase
     toujours sans conflit dessus. La PR [morluto/rea#974](https://github.com/morluto/rea/pull/974)
     est ouverte ; un autre utilisateur confirme la même coupure à 330 s sur un PE de 64 Mo.
   - **Pourquoi chaque ouverture refait l'analyse** : le lanceur passe toujours `-deleteProject`
     à `analyzeHeadless` (`src/ghidra/GhidraLauncher.ts`), le projet Ghidra analysé est donc
     effacé à la fermeture. Piste proposée sur la PR le 08.10 : garder ce projet, repéré par
     l'empreinte du binaire, la version de Ghidra et les options d'analyse. Si morluto est
     d'accord, ouvrir une issue puis une PR séparée ; c'est l'autre voie que le point 4.

4. **Outillage : d'autres serveurs MCP à essayer** (repérés le 07.10.2026) :
   - **Ghidra avec projet persistant**, pour ne plus refaire les 6 min 30 d'analyse
     à chaque ouverture comme REA : [pyghidra-mcp](https://github.com/clearbluejar/pyghidra-mcp),
     [ghidra-headless-mcp](https://github.com/mrphrazer/ghidra-headless-mcp),
     [bethington/ghidra-mcp](https://github.com/bethington/ghidra-mcp) (200+ outils),
     [GhidraMCP](https://github.com/LaurieWired/GhidraMCP) (passe par l'interface).
     Test : rouvrir le projet de `~/Documents/bully-test` en quelques secondes,
     comparer avec REA sur `FUN_0060f350`.
   - **Le jeu en marche** : [x64dbg-mcp-server](https://github.com/duty1g/x64dbg-mcp-server)
     sur la tour Windows, point d'arrêt dans `FUN_0060f350` pour lire les vraies
     valeurs de TargetSync pendant une scène.
   - Écartés : IDA Pro et Binary Ninja (payants) ; agent-reach (scraping avec les
     cookies du compte, rien à voir avec la rétro-ingénierie).

## Fait

- La durée réelle de la parole : `SpeechDB.bin` (voix, ligne, échantillon) et la table de `Speech.bin` (clé
  `FUN_005a7c10`, banques XACT `SDBK`/`WBND`) ; `CSpeechBank`, `tests/test_speechbank` ; docs/son.md.
- Le préchargement d'une ligne de parole (`SoundPreloadSpeech`, le bloc `DAT_00cfb3b4`, le rappel `FUN_005d49d0`) :
  `CSpeechPreload`, `tests/test_speech` ; la narration de `1_02` passe ; docs/son.md.
- Les conditions des nœuds de mission (`FUN_0061a4f0` construit sur l'enregistrement, à +4) : `MissionCond`
  (mot +2 de l'entrée = réussites, incrémenté par `FUN_006ac910`), `TimeCond` (`FUN_00425cf0`), `ConditionFalse` ;
  `CMissionBank::Evaluer` / `Activable`, `tests/test_missions` ; `1_02` démarre seule après `1_01` ; docs/missions.md.
- Le pavé fermé de `FUN_005c2910` (`FUN_0046d960` en trois dimensions, `FUN_0046d880` en deux) : `PaveContient`,
  derrière `PlayerIsInAreaXYZ`, `PedIsInAreaXYZ` (réelle) et le corona ; `tests/test_zones`.
- `DATInit` lu : `FUN_00416000` refait la table des portes du gestionnaire de zones (listes `SV_` et `DT_`
  appariées au déclencheur du même nom ; `SV_` passe en entrant, `DT_` par son objet), puis `FUN_004154e0`
  crée les objets à ramasser de la zone visible ; `CAreaManager::ReconstruirePortes` / `PasserUnePorte`,
  `tests/test_portes` ; `DATInit` réel dans `lancer_lua` ; docs/zones.md.
- Le corona d'une mission : `FUN_006a8650` lit la piste `MissionButtonTrigger` par ses décalages, le champ 28 est le
  demi-côté du pavé (2 m pour `1_01`, 1 m ailleurs), pas un mode ; `CMissionData` lit demi-côté, marqueur, taille,
  invite ; `tests/test_missions` ; le gestionnaire de `lancer_lua` teste le pavé ; docs/missions.md.
- La parole des scripts : la table des 493 événements de `bully.exe` (0x00ae74e8), la recherche sans la
  casse de `SoundPlayScriptedSpeechEvent`, la réponse de `SoundSpeechPlaying` (locuteur, événement sur
  9 bits, ligne sur 11) ; `src/audio/SpeechEvents`, `tests/test_speech`, réelles dans `lancer_lua` ; docs/son.md.
- L'arbre d'actions global et le chemin d'un nœud (`FUN_005f6c50`, `FUN_005f59b0`, `FUN_004849a0`) :
  `src/core/ActionBank`, `tests/test_actionbank` ; `PedSetActionNode` et `PlayerSetControl` réels dans
  `lancer_lua` ; docs/cat.md.
- `POI`, la cinquième énumération : `FUN_00448e30` lu (groupes du dernier au premier, le nom d'un groupe
  rend son premier point, sinon un point de ce nom), `CDatPools::Poi`, `tests/test_datpools`, réel dans
  `lancer_lua` ; docs/trigger.md.
- Les cinématiques : ce que les scripts appellent (`PlayCutsceneWithLoad` → treize fonctions du moteur), les
  fichiers de `Cuts/Cuts.img` (`.INF`, `.SUB`, piste caméra `.DAT`), `src/cutscene/CutsceneFiles`,
  `tests/test_cutscene` ; docs/cinematiques.md.
- Les zones : `CAreaManager` (table des scripts de zone, zone visible, demande et machine à états de
  `FUN_00416e30`), `tests/test_zones` ; `lancer_lua` charge les scripts de zone avec leur `main`,
  `AreaIsLoading`, `AreaRegisterAreaScript`, `AreaSignalAreaTransitionReadyToLoad`, `CameraFade` réels ;
  `AreaTransitionXYZ` était une version C morte (SGlFunc.lua la définit en Lua) ; docs/zones.md.
- L'octet +0xc de l'entrée de liste de points = l'AREACODE (`FUN_006d59a0`) ; tous les champs d'un
  bloc POINT lus (`CTriggerPointPoint`, `MasqueMeteo` = `FUN_00593f20`) ; docs/trigger.md.

- `MODELENUM` : `src/core/ModelEnum` reconstruit la table des modèles par nom depuis les `.idb` (4 251 noms, sans la casse, -1 sinon, comme `FUN_0051c1e0`) ; `lancer_lua` pose `MODELENUM` et `POI` (celle-ci à -1, pas encore lue) ; `tests/test_modelenum` ; docs/lua.md.

- Les gestes des piétons : `CScriptPedPool::Avancer` joue image par image la marche, la course et le sprint (`PedMoveToPoint`, `PedMoveToXYZ`, `PedStop`), la bagarre (`PedAttack` : poursuite, coups, mort) et `PedMakeAmbient` ; `lancer_lua` journalise arrivées, coups et morts ; `tests/test_scriptpeds` étendu.

- La frontière moteur / scripts : `src/core/LuaApi` lit `docs/api-lua-tables.tsv`, `lancer_lua` ne bouchonne que les 1 508 fonctions du moteur, toute autre globale vaut nil comme dans le jeu ; c'est ce qui bloquait `1_01` (`if gMissionSuccess` vrai sur un bouchon) : **`1_01` se termine réussie à 9,1 s** avec `--entrer _1_01_BULLYTRIG@4 --aller _1_01_ENDPOINT@8 --zone 2@8.1` ; `tests/test_luaapi`.

- Le joueur joué par l'outil : `PlayerIsInTrigger`, `PedIsInTrigger`, `PlayerSetPosPointArea`, `PlayerIsInAreaXYZ`, `AreaGetVisible` réels ; options `--entrer`, `--aller`, `--zone` de `lancer_lua` ; `1_01` passe la bagarre et l'arrivée au bureau (`tools/lur.py` pour lire la condition d'un objectif).

- (7dacc36) Les piétons des scripts : `src/peds/ScriptPedPool` (24 entrées, joueur en 0), `PedCreatePoint` / `PedCreateXYZ` / `PedDelete` / `PedIsValid` / positions / cap / santé réels dans `lancer_lua`, ramassage à la fin de la mission ; `1_01` crée ses trois premiers personnages et attend le joueur dans `_1_01_BULLYTRIG` ; `tests/test_scriptpeds`.

- (549ad27) Les données de mission : `DATLoad(fichier, emplacement)`, `DATUnload`, `DATInit`, les tables `POINTLIST` / `PATH` / `TRIGGER` et `GetPointFromPointList` lus dans le moteur et recréés (`src/core/DatPools`, blocs POINT dans `TriggerFile`, `tests/test_datpools`) ; `1_01` passe à `PedCreatePoint` de vrais handles.

- (ab7fba6) La banque des missions dans `src/core/MissionBank.cpp` (`CMissionBank`, suit les références de `Missions.cat`, `Doors.cat` compris) : 499 entrées, 360 avec un script ; l'écart 360 / 473 venait de `missions.py` (Doors ignoré, pistes perdues), corrigé ; `tests/test_missions`.

- `ForceStartMission` dans `lancer_lua` : table des missions lue dans la banque, machine à états du moteur (`FUN_006ad3e0` : MissionSetup, main, MissionCleanup), `RequestModel` et variantes ; `1_01` va jusqu'à `PedCreatePoint` ; champ 36 de `MissionData` = afficher le titre.

- La banque des missions : `Missions.cat` chargée par `CGame::Initialise` (`FUN_006ac5d0`), forme d'une mission, champs de `MissionData` (24 titre, 28 script, 32 clé de titre, 40 type), `ForceStartMission` comme primitive, `1_01` seule mission du chapitre 1 sans condition ; hypothèse « champ 36 = lancer au début » réfutée (282 missions) ; `docs/missions.md`, `tools/missions.py`.

- Les six encodages des `.agr` (999 à 1004) : déjà décodés depuis le commit e0dc3ee
  (« Les six encodages d'animation »), l'ancien point 1 était resté par erreur.

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
