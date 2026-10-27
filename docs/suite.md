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
   `TimeCond`, `ConditionFalse` sont lues : `1_02` s'enchaîne après `1_01`. Les portes `DT_` passent (`--porte`). Le contrôleur descend dans l'arbre de `PedSetActionTree` (docs/cat.md) et la leçon de parade de `1_02` passe avec `--jouer`.
   Les conditions des nœuds sont jugées (docs/cat.md : `ConditionGroup` par la seconde table de renvois, `WeaponEquiped`,
   `ActionRequest`, `ActionNodePlayTime`, `TargetPlaying`, `LuaCond`) et le contrôleur redescend quand le nœud joué finit ;
   dans `1_02`, la parade, la séquence X, la prise et sa charge passent (`--jouer`, `--demander`, `--sequence`).
   Les passages viennent des pistes (docs/cat.md : `Opportunity`, loquet, `Sequence`, `PlayOnTarget`) ; la leçon de
   prise se termine (le bully joue `PushFwd/RCV`). Les coups portent (docs/cat.md, « Les coups ») : pistes `Collision`,
   réaction par `HitTree` (`ConditionFlag`, `MePlaying`), `Sequence` hors de l'arbre posé, fin des animations
   (`MAINPED.HXD`) ; la leçon des coups au sol passe (`GroundHitHeavy`) et `1_02` affiche `1_02_FightMsg`.
   Les coups blessent (docs/cat.md, « Les dégâts, la santé et l'humiliation ») : un `Execute` ajoute ses pistes au
   contrôleur (pas de contrôleur secondaire), `DamageHandle`, `SetHealth`, `SetPedFlags`, `SetTargetAsGrappleTarget`,
   les piétons ont leur entrée de `pedstats.dat` ; dans `1_02`, le bully se relève, onze jabs le mènent à 5 points et
   la mission affiche `1_02_HumiliateHim` (82,1 s).
   L'humiliation se joue (docs/cat.md, « L'humiliation ») : le groupe d'humiliation (+0xd44), les conditions propres des
   `Opportunity`, les groupes d'animations, la parole retenue, `TargetRelativePosition`, `FaceTarget`,
   `SeekEntityPosition`, `PedFlags` jugée : **`1_02` se termine réussie à 119,13 s** (la vraie `5-8_NoogieSpit`, puis
   `F_SetupRussellFight`).
   L'arbre de l'IA est joué (docs/cat.md, « Les arbres de l'IA ») : `PedSetAITree`, la file des objectifs, dix-huit
   conditions, `ManageTargets`, `CombatLoco`, `ActionNodeRequest`, `spawn`, `ClearCurrentObjective`, `FleeObjective`, la
   règle de l'octet +8 ; `1_02` reste réussie (119,03 s).
   Les références montées et la pile du contexte sont lues (docs/cat.md, « Les références montées ») ; `LuaCond` appelle dans
   l'espace de noms de la mission : le bully attaque pendant la leçon de parade. `1_02` reste réussie (119,03 s).
   Les conditions de la parade, des armes, du social, des caméras, du HUD et des manettes sont lues (docs/cat.md, « Les
   conditions lues le 27.10.2026 ») ; la vitesse de `CombatLoco` vient du pas (`FUN_00625970`). `1_02` reste réussie (119,03 s).
   La course de `RunStart` est établie (il n'y en a pas : le pas de `CombatLoco` est le mode 10 sous le petit rayon) ; les entrées
   vidées (`FUN_005f38e0`), la vitesse de la navigation (`FUN_005e4a60`), les quatre sous-états de `CombatLoco` et l'arrêt au petit
   rayon sont recréés ; le bully pare (le bouton 12 de son IA, `Defence/Block`, `HitTree/Standing/BlockHits`), une `Opportunity` prise
   fait finir son nœud même sans nœud visé ; `ActionRequest` lit le bouton brut (`FUN_005fd570`) ; huit conditions de plus (armes,
   conversation, missions, tenues, `AttackerConditionGroup`). `1_02` est réussie à 121,94 s (quinze jabs, docs/missions.md).
   Suite, dans l'ordre :
   - ce qui reste supposé dans `CombatLoco` : le point valable (`FUN_006254c0`), la recherche de chemin ;
   - le choix d'une cible (`FUN_004992a0`, `FUN_004988b0`, `FUN_00496b60`), les sphères des os des coups (au lieu du demi-espace
     avant) ;
   - les `SpeechTrack` sans tenue ;
   - `1_03` (docs/missions.md, « 1_03 ») : `PedFollowPath` pose l'objectif et les règles du suivi sont recréées ; reste la piste
     `FollowPath` elle-même, bloquée sur le rayon d'arrivée par défaut (+0x51c du piéton, introuvable à l'assembleur hors de
     `PedPathNodeReachedDistance`) : le chercher par REA (les références à +0x51c, une copie de structure) ou sur le jeu en marche.

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

- La vitesse de l'IA et la parade (27.10, soir ; 5e778b1, 9d2108e, 85a3bd9, 0d5f9fc, 70ebb21, 7c260a7, 36fca42, 379089e, 004063b) :
  les entrées vidées (`FUN_005f38e0`, `IDLE`, la garde `FUN_00484490`), la vitesse de la navigation (`FUN_005e4a60`), l'arrêt de
  `CombatLoco` au petit rayon et ses quatre sous-états (`FUN_0062e120`), la course de `RunStart` établie (il n'y en a pas), les boutons
  des piétons de l'IA (`ActionRequest` = `FUN_005fd570`, la piste `FUN_006260d0`), `SetWeaponInfo` et `WeaponNeedInit`,
  `ConversationActive`, `OnMission`, `MissionCond`, la garde-robe (`PedIsInOutfit`), `AttackerConditionGroup`, la fin demandée par une
  `Opportunity` prise : **le bully pare, `1_02` reste réussie à 121,94 s** en quinze jabs.

- Les conditions du 27.10 (1a93702, c2d4361, 435c68a, 49ca373, 786e4c3, 79bece1, 30ac742, 0678b57, 2e9d430) : le HUD (48
  composants, `HudComponentVisible`), `CollisionStart` (+0x4e5, jamais remis à 0) et `AttackPhase` (la phase +0x71c et son
  échéance, `FUN_005f9480`, `FUN_005f9e70`, `FUN_005f8240`), les manettes et `Controller`, quatre conditions d'armes, l'état social
  (`IsSocialStateWait`, `IsSocializing`), la vitesse de `CombatLoco` par le pas (`FUN_00625970`), le gestionnaire de caméras
  (`CameraActive`, `CameraInWidescreen`), `AttackTimer` et `Attacked`, `FightClass` et `RangeToFocus`. **`1_02` reste réussie à
  119,03 s**, le bully gagne sa zone en `Sprint`.

- Les références montées (26.10 ; 1c47e48, 69a6f31, d514cc1, d8c1143, 48dc774, f35ee03, 5a1c5c7, dae1733, 00ee22f, de1f206,
  e036e1a, 53b5636) : `FUN_005f6960` et la table des parents (`FUN_005f6800`, `FUN_005f6290`), `DAT_01b649dc` = `Global`, la
  clé = le nom de l'arbre de tête (`FUN_005f4e40`) ; la pile du contexte (`FUN_005f36d0`, `FUN_005f3700`) ; `LuaCond` par
  `NSCall` dans l'espace de noms ; le type et le sexe des piétons (0x90e7d0, +0x1310) ; l'intention de mouvement (+0, +0x1c ;
  `FUN_005c6180`) ; le chapitre et la saison ; seize conditions. **`1_02` reste réussie à 119,03 s**, le bully frappe pendant
  la leçon de parade (31,12 à 42,60 s).

- Les arbres de l'IA (23.10, nuit ; aa2b85d, cf1c182, fccc685, b5f3ef3, 4d2d23f, e9205d8, 495f738, a287497, fc9349b) :
  `LuaCond` lue par `lua_tonumber` ; la file des objectifs et `AttackPedObjective` (`PedAttack`) ; le contrôleur de l'IA
  (`PedSetAITree`, `FUN_00484290`, `FUN_00484490`) et dix-huit conditions ; les pistes `ManageTargets` (mise à jour
  `FUN_00627420`, qui donne le groupe d'humiliation), `CombatLoco`, `ActionNodeRequest` (l'intention +0x254), `spawn` (un
  contrôleur propre), `ClearCurrentObjective`, `FleeObjective` ; les zones de combat (`PedJoinCombatZone`) ; l'octet +8
  qui retient un nœud (`FUN_005f4230`) ; les contrôleurs d'un piéton supprimé ; les coups de face. **`1_02` reste réussie,
  à 119,03 s**, avec le vrai comportement de l'IA à la place du geste `PedAttack`.
- L'humiliation de `1_02` (23.10, soir ; 9985800) : le groupe d'humiliation (+0x200 du composant social en +0xb44,
  `FUN_0048e790`, `FUN_0048f5d0`, `FUN_004925d0`, `FUN_00492950`, `ManageTargets` `FUN_00627420`), `HumiliationGroupIs` ;
  les conditions propres des `Opportunity` (le champ 60 par la seconde table de renvois), `ActionNodePlayTimePast` ;
  `CAnimGroups` et `AnimationGroupLoaded` ; `SpeechTrack` / `SpeechStartTrack` / `IsSpeechPreloaded` ;
  `TargetRelativePosition`, `FaceTarget`, `SeekEntityPosition`, `spawn` ; un `Execute` ne retient pas son nœud,
  `PedFlags` jugée (9128f8f) ; l'objet de porte (+0x394) posé par `ActivateDoor`, `PropAttemptDoorTransition` et
  `PropAnimation` `OPEN` (docs/zones.md). **`1_02` se termine réussie à 119,13 s.**
- Les coups blessent (23.10) : `Execute` dans le contrôleur (`FUN_005f7070`, `FUN_005f3e20`), le relevé du coup
  (+0x408) et `FUN_0047b810`, `DamageHandle` (`FUN_0060fc70`), `SetHealth` (`FUN_00627500`), `SetPedFlags`
  (`FUN_0060f8e0`), `SetTargetAsGrappleTarget` (`FUN_0062a580`), `HitAnimation`, l'arrêt des `Opportunity`
  (`FUN_005f7440`) ; pedstats.dat dans les piétons (`FUN_00478e30`, `FUN_00472ff0`) ; onze conditions (`HitTime`,
  `stat`, `Health`, `IsHealthAtHumiliationLevel`, `CharacterSize`…) ; `1_02` jusqu'à `1_02_HumiliateHim`.
- Les passages par les pistes (22.10, troisième séance) : chemins relatifs (`CActionBank::Relatif`), `Opportunity`
  (`FUN_005f7d70`, `FUN_005f7440`, `FUN_005f3bc0`, tirage `FUN_00465910`), `OpportunityRandomLatch` (`FUN_005f7510`,
  `RandCrt`), `Sequence` (`FUN_005f7030`), `PlayOnTarget` (`FUN_006615f0`) ; comparateurs `FUN_005fb750` et état des
  boutons `FUN_005fd650` ; la fin de la leçon de prise de `1_02`.
- Les conditions des nœuds d'action et le contrôleur (22.10, seconde séance) : `ConditionGroup` (seconde table de
  renvois), données des .cat gardées, inclusions `i`, `EvaluerListe` (`FUN_005fb9b0`), `CActionController`,
  `ButtonHistory` (rappels), `lancer_lua --demander` / `--sequence`.
- Le contrôleur d'actions (`FUN_005dc040`, `FUN_005f5490`, `FUN_005f5500`) : `CActionBank::Choisir`, `PedSetActionTree` ;
  les 68 arbres globaux (`FUN_005f6f20`) ; `PedMePlaying` (`FUN_005f46a0`) ; `lancer_lua --jouer` ; les fondus
  d'`AreaTransitionButes` (0,65 s / 0,5 s) et `TimeCycleButes` lues dans `Globals.cat` (`FUN_0072ad00`) ; le recul des
  portes (`FUN_00416750`, 2,5 m, π) ; `PlayerIsInAreaObject`, `PedLockTarget` ; `tests/test_actionbank`,
  `tests/test_portes`, `tests/test_scriptpeds`.
- `DayCond` (`FUN_006a8b30`, `TimeCycleButes` = 5) et `CurrentMission` / `CurrentClassMission` (`FUN_006aa630`,
  `FUN_006a9ed0`) ; `GetCurrentDay` réel ; `tests/test_missions` ; docs/missions.md.
- `PedIsPlaying` (`FUN_005f3bf0`, `FUN_005f4f80`) : `CActionBank::Joue`, `tests/test_actionbank` ; docs/cat.md.
- Les coups et l'invulnérabilité (`FUN_0047b6b0`, `FUN_004782a0` : +0x138c, +0x1364, +0x1ca8) :
  `CScriptPedPool::Blesser`, `tests/test_scriptpeds` ; `1_02` va jusqu'au combat de Russell.
- La liste de conditions d'un nœud (`FUN_005fb9b0` : NOT, OR, genres 1 et 2) et le champ 44 (rejouable) de
  `MissionData` ; `CMissionBank::Activable`, `tests/test_missions` ; docs/missions.md.
- Les portes `DT_` : le déclencheur de l'objet de porte en cours (+0x394, `FUN_00415160`), le retour gardé de
  neuf zones (`FUN_0042e570`, `FUN_00416750`) ; `CAreaManager::PousserPorte`, `tests/test_portes`,
  `lancer_lua --porte` ; docs/zones.md.
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
