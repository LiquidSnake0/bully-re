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
   La mort passe par la piste `Death`, `1_02` n'a plus de condition jugée -1 (`MissionUnlocked` ; `HitTime` vaut -1 sans coup), la vue
   des piétons et `DamageTarget` sont lues ; `1_03` démarre (la piste `FollowPath`, docs/missions.md). `1_02` reste réussie à 121,94 s.
   Le 02.11, `1_03` va jusqu'au garage : la poursuite jouée (`--suivre`, `--viser`), le coup reçu qui prévient les scripts
   (`RegisterPedEventHandler`), `PedSetPosPoint`, `PedFaceObject(Now)`, `PedClearObjectives`, `PedAttackPlayer`, les actions des
   nœuds de trajet, les drapeaux et les stats, les attitudes, le choix d'une cible par l'IA (docs/missions.md, « 1_03 »).
   Le 03.11, le bully 5 de `1_03` frappe par son arbre : les arbres du modèle posés à la création (`FUN_00484d10`), 26 classes de
   `AI.act`, `stat` et son tirage, les dés des contextes, les droits d'attaque, les jetons de combat et les piétons proches
   (`FUN_00486e40`, `FUN_0047d1d0`). `1_02` est réussie à 124,97 s (dix-huit jabs, docs/missions.md).
   Le 04.11, **`1_03` se termine réussie à 148,90 s** (docs/missions.md, « La riposte ») : les événements des déclencheurs
   (`RegisterTriggerEventHandler`, `FUN_006d6f60`), la piste `Script` (`FUN_006851a0`), les arbres du joueur, `PlayerSetControl`,
   la laisse, `PedIgnoreStimuli`, l'état des portes, `IsSocializingWithPlayer` et `TargetedByPlayer`. `1_02` reste réussie à 124,97 s.
   Le 05.11, **`1_04` se termine réussie à 81,05 s** (docs/missions.md, « 1_04 ») : `PedGetOffsetInWorldCoords`, les armes (les types
   ObjectButes de `Global_Weapons.cat`, l'inventaire de `ms_pPool654`, l'arme en main par `Weapons/SelectActions`, le HUD des armes, onze
   conditions), les projectiles (`WeaponFire`, la bille, la brique, les marques), les objets cassables (les entrées spec des `.ipb`,
   `RegisterHashEventHandler`), les arbres des objets diffusés (`pxTree.cat`), les gestionnaires globaux, les ramassages ; `1_03` finit
   aussi aux briques, Davis touché de loin (148,04 s), et `1_02` par son vrai chemin (`PedGetWhoHitMeLast`, `_1_02_RUSSELTEASE`, 132,92 s).
   Le 06.11, **`1_05` va jusqu'à sa troisième étape, le casier d'Algie** (docs/missions.md, « 1_05 ») : les rondes des préfets (`src/peds/Patrouilles`,
   `AreaDisablePatrolPath`), `PedIsHit`, le groupe des alliés (`PedRecruitAlly`, `PedDismissAlly`, huit conditions), la piste `TetherLoco`,
   les alliés qui passent les portes avec le joueur (`FUN_0048b970`), `PedSetMissionCritical`, `PedFindInAreaXYZ`,
   `SoundStopCurrentSpeechEvent`, `PedRestrictToTrigger`, `PedSetInfiniteSprint` : la première étape, l'école, la cabine, l'embuscade,
   la chasse d'eau, le lavabo ; `F_Stage3` à 154,18 s.
   Le 06.11 (séance 2), **`1_05` se termine réussie à 185,20 s** (docs/missions.md, « 1_05 ») : les objets animés des déclencheurs
   (`src/objects/ObjetsAnimes` : leur création pour la zone visible, `FUN_00416540`, leur fiche PropButes, `PAnimExists`,
   `PAnimSetActionNode`, leurs drapeaux, leur contrôleur et les pistes `PropFlags`, `PropSetPedAction`), `TargetProp` et le relevé des
   objets proches, les pistes `Sync` et `PropInteract`, l'écran de fin (`MinigameSetCompletion`, `MinigameIsShowingCompletion` : `1_04`
   finit maintenant à 82,53 s), les objectifs de mission, `PlayOnTarget` qui attend sa cible ; `1_06_01` démarre par le nœud `Run` de sa
   mission (`F_SetStage`) ; les portes animées (`PAnimOpenDoor`, `PAnimCloseDoor`, les portes nommées par leur objet).
   Le 06.11 (séance 3), **`1_06_01` se termine réussie à 61,81 s** (docs/missions.md, « 1_06_01 ») : le gestionnaire des collectibles
   (`src/objects/Collectibles`, colctbls.dat), les objets du joueur (`ItemGetCurrentNum`, `ItemSetCurrentNum`), les ramassages dans src
   (`src/objects/Ramassages`), les collectibles des placements changés en ramassages par la population, la prise au contact
   (`FUN_00445710`, `FUN_00444ac0`), `ObjectRemovePickupsInTrigger`, l'historique des boutons (`src/hud/ButtonHistory` et la piste
   `ActionRegisterButtonHistory`) : le joueur prend le transistor, le donne au clochard et réussit la leçon du coup chargé ; `1_07`
   passe `PAnimRequest`.
   Le 06.11 (séance 4), **`1_07` va jusqu'à la porte du garage** (docs/missions.md, « 1_07 ») : `PAnimCreate` (les objets animés des
   missions : dans `1_01` la grille de l'école, dans `1_03` les portes du garage et les tonneaux de Davis ont maintenant leur objet),
   `PedFindInAreaObject`, `CreatePersistentEntity` et `DeletePersistentEntity`, les marqueurs du radar (`src/hud/Marqueurs` :
   `AddBlipForChar`, `BlipAddPoint`, `BlipAddXYZ`, `BlipRemove`) ; le joueur rejoint Bucky, bat les quatre bullies et ouvre la porte.
   Le 06.11 (séance 5), **`1_07` se termine réussie à 151,47 s** (docs/missions.md, « 1_07 ») : `PropTargetInteractive` (Bucky se
   sert de la caisse à outils), `PickupCreateFromPed`, `PedHasAlly`, `PedIsModel`, `PlayerUnequip`, `PlayerSetPosSimple`,
   `PlayerFaceHeadingNow`, `PAnimDelete`, la cible par genre de `FaceTarget` et `SeekEntityPosition` (`FUN_00477ab0`), la barre de santé
   secondaire (`src/hud/BarreSante.h`), `PedMakeMissionChar` et `PedStopSocializing` ; le joueur entre dans `_1_07_FIGHTTRIGGER`, bat la
   seconde vague, va à `_1_07_PLYRDOOR`, et la mission joue le clochard, le don de la planche et l'écran `M_PASS`.
   Le 06.11 (séance 6), **la suite des armes commence** : la charge de l'arme (`CChargeArme` : `WeaponSetCharge`, `WeaponIncrementCharge`,
   la charge lue par `WeaponFire` et vidée après le tir), les drapeaux des armes (`SetWeaponFlags`, `WeaponFlags`, `PedSetWeaponFlag`), la
   minuterie de mission dans src (`src/hud/MinuterieMission.h` : `MissionTimerStartFixed`, `MissionTimerPause`) et `FaceTarget` des
   contrôleurs de l'IA par `FUN_00477ab0` ; les sept missions restent réussies.
   Le 07.11 (séance 1), avec les six commits de la veille au soir, elle continue : le pas du contrôleur d'actions, la manette qui
   tient un bouton (`--tenir`, la feuille `Charge` jouée pour de vrai dans `1_04`), les réglages et les dégâts du tir, l'objet arme,
   `AttackTimerReset`, `HasRangedWeapon`, `DetectProjectile` et `DetectGrenade` sur les projectiles de l'outil, et le haut du corps du
   joueur en contrôleurs à part (le bouton 8 tenu seul fait partir la première bille de `1_04`) ; les sept missions restent réussies.
   Le 07.11 (séance 2) : `IsPlayerFatigued`, l'avance sur une cible qui bouge (`FUN_0065ecb0`), la table des projectiles (32 places,
   leur vie, `AreaClearAllProjectiles` par modèle) et **les entités qui suivent un trajet** : le PathReference, le gestionnaire
   0xd13730, `PAnimFollowPath`, `PAnimStopFollowPath`, `PAnimSetPathFollowSpeed`, `PAnimGetPathFollowSpeed`, `PAnimFollowPathReset` et
   le rappel des nœuds atteints ; la grille fermée du magasin (`SCStoreGte_Closed`) est posée au départ de son trajet dans `1_01`, `1_02` et `1_05`, et sa vitesse de -1,2 ne la fait pas bouger ; les sept missions restent réussies.
   Le 07.11 (séance 3), **`AreaClearAllPeds` est branchée** : elle détruit les trois joueurs de foot rendus à la population dans
   `1_04`, `1_05`, `1_06_01` et `1_07`, les poignées des piétons créés ensuite baissent de 3, et les commandes de `1_04` et `1_05`
   sont refaites (`1_05` finit à 194,30 s) ; avec elle `PedClearHitRecord`, `PedSetMinEngage` et la condition `EngageDistance`, la
   visée automatique du joueur (`WeaponPlayerFireAutoTarget`) et les décalages de visée de `WeaponFire` (`FUN_00662760`).
   Le 09.11 (séance 1), **la visée du joueur est refaite en entier** (`FUN_00662a40`) : les destinations forcées (`PedSetProjDest`), la
   source de la cible (l'octet +0x31 et l'objet visé +0x438, le joueur et ses quatre conditions, +0x54c, la cible du composant de visée
   et son repli `FUN_004d7630`), le bouton 10 de la manette (`FUN_007386f0`, `FUN_0050c730`, `src/core/Manette.h`), le point devant le
   tireur sans cible (`FUN_0065ea50`), le point et l'imprécision sur une entité qui n'est pas un piéton (`FUN_006626a0`, `FUN_0065eb40`,
   `FUN_004d5cd0`) et `PedTargetPAnim` ; avec elle la vie d'un projectile (`FUN_004d4c40`, `PedSetProjectileLifetime`), son test
   d'eau (`FUN_006d48d0`) et la mort d'une piste spawn au contrôleur vide (`FUN_005f72e0`) ; les sept missions restent réussies, à
   l'identique. Le haut du corps du lance-pierre ne repart pas encore au bouton suivant : la feuille `AimOut/default` ne finit pas.
   Le 11.11 (séance 1), le haut du corps du lance-pierre repart : ce n'était pas une durée manquante (`C_PLAYER\SLNG_2IDLE_T` est dans
   MAINPED.HXD, 0,3333 s) mais le contrôleur d'actions qui ne se vidait pas à la fin d'un nœud sans suite (`FUN_005f49a0`) et la durée
   d'un nœud qui comptait les pistes sans l'octet +8 (`FUN_005f4230`). `1_04` casse ses trois vitres au seul bouton 8 (`e104b.sh` :
   17,56 s, 19,54 s et 21,55 s) et finit à 82,53 s ; `1_05` finit à 180,31 s et `1_07` à 150,88 s (les nœuds de coups reçus finissent
   avec leur animation retenante). Avec lui le choc d'un projectile (`FUN_004d3860`, `FUN_004d37d0`), son contact (`FUN_0046ad90`), les
   explosions (`FUN_004d2b40`, `FUN_004d2aa0`, `FUN_004d2690`), sa fin (`FUN_004d45e0`, `FUN_004d3dc0`) et les marques d'impact
   (`FUN_00560f70`, `FUN_00560f10`).
   Suite, dans l'ordre :
   - la suite des armes : **les deux tests fins de collision, `00469f70` et `0056c770`** (non lus) : c'est tout ce qui reste entre
     le balayage, recréé et branché, et un vrai contact dans l'outil. État au 13.11, séance 1 (voir « Fait ») : le balayage d'une
     liste de secteur est recréé (`FUN_0046c4b0`, `CVolProjectile::BalayerListe`, `src/objects/Projectiles.h`, un gabarit dont
     l'adaptateur fournit `Code`, `Pas`, `Rayon`, `Centre`, `Entite`, `Noeud`, `SansPietons`, `IgnorePieton`, `IgnoreSonGenre2`,
     `IgnoreGenre2Lie`, `BitDuModele`, `Volume`, `Spheres`, `ProjectileIgnore`, `VehiculeIgnore`, `ModeleSerre`, `Avant`, `Tester`,
     `Lier`, `Delier`), avec tout ce qu'il appelle : `FUN_004761a0` (`ProjectileIgnore` : un projectile ne touche jamais son
     tireur), `FUN_00469f10` (`VehiculeIgnore`), le ballon prisonnier (`FUN_006838d0`, `FUN_0067b0f0`, `FUN_00683a10`,
     `FUN_00680b60`, `FUN_0067b0b0`, `FUN_0067b100`), `FUN_004d3360`, le centre (`FUN_00466b50`, `FUN_004660b0`, `FUN_0046d700`), `FUN_0051ae00`
     (`ModeleDeCollision`), `FUN_004ea210` (`Avant`), `FUN_00415e50` et `FUN_004153b0` (le contact local), et **la méthode +0xa4
     d'un `CProjectile`, `FUN_0046a340`** (`TesterCollision`, avec `FUN_00447290`, `EstUnChien`), qui ne fait que choisir entre
     les deux tests non lus. **Le balayage est branché dans l'outil** (`outils/lancer_lua.cpp`, `CBalayageOutil` avant
     `AvancerProjectiles`) : à chaque image et pour chaque projectile, l'outil fait une liste aux nœuds du binaire (les piétons
     vivants, piscine 0 ; les objets cassables qui restent, piscine 2 ; le monde de la zone, une seule entité, piscine 7, absente
     à la première image du vol) et la donne à `BalayerListe` ; le code recréé juge le code de balayage, le filtre d'entrée, le
     volume et les jugements d'un projectile (le tireur n'est plus jamais touché ; l'outil écrit « le projectile N traverse son
     tireur, le piéton P : le balayage l'écarte (FUN_004761a0) » quand son ancien test l'aurait touché) ; `Tester` garde les
     tests de l'outil, supposés. Dans l'ordre :
     1. **lire et recréer les deux tests de `FUN_0046a340`** (lue de 0x46a340 à 0x46a423, à ne pas relire) :
        - le test fin contre un piéton, quand l'entité qui balaie est un projectile (+0xc4 non nul) et que l'entité de la liste
          est un piéton (+0x108 & 7 == 3) au +0xa4 nul, dont le modèle (+0x10e) n'est ni DAT_00bf42dc (« Rat_Ped ») ni un des
          trois chiens de `FUN_00447290` : d'abord `00572680(piéton)` (1 529 octets, **non lue** ; supposé : elle prépare les
          volumes du piéton, que le mot 0xcf3138 désigne ensuite), puis `00469f70` (966 octets, **non lue**), méthode de l'entité
          qui balaie, avec arg1, sa matrice (`FUN_0046dbd0`), son modèle de collision (`FUN_0051ae00`), la matrice du piéton, le
          mot 0xcf3138 et le contact ;
        - sinon le test général `0056c770` (4 610 octets, **non lue**, cdecl à quinze arguments) : arg1, la matrice et le modèle
          de collision de l'entité qui balaie, la matrice de l'entité de la liste deux fois, son modèle de collision, le contact,
          cinq mots nuls, 0xf, 0 et 1. C'est lui qui teste un projectile contre le monde, un objet, un véhicule, le rat et les
          chiens : **commencer par lui** (toutes les billes de `1_04` finissent sur une vitre ou un piéton, mais le sol et les
          murs de l'essai hors mission passent par lui). Il prend deux modèles de collision (sphères, boîtes, triangles : les
          trois comptes +0x1c, +0x1e, +0x1f du volume +0x38, sens supposé) : l'outil a déjà les triangles et les sphères du
          monde (`CMondeCollision`, `src/collision/Marche.h`), pas les modèles des entités ;
     2. **brancher ces tests dans `CBalayageOutil::Tester`** (`outils/lancer_lua.cpp`) à la place des tests de l'outil, qui sont
        tous **supposés** : le segment de l'image contre trois sphères de 0,35 m pour un piéton (h = 0,35, 0,95 et 1,55 m), une
        de 0,5 m pour un objet cassable, `CMondeCollision::Trajet` pour le monde (compté seulement s'il est croisé avant l'entrée
        dans la sphère déjà retenue). Tomberont alors : le rayon nul du projectile (`Rayon`, `FUN_00465cc0`), `Spheres` toujours
        vrai (`FUN_00465bc0`, recréée : `SpheresSeTouchent`), le volume d'essai à un compte, et la règle de la première image du
        vol (`q.sorti`, qui tient au point 4) ;
     3. **composer `0046ca40` d'une pièce** (2 884 octets, de 0x46ca40 à 0x46d588 ; **lue en entier**, recréée par morceaux, pas
        au compte). Ses morceaux, tous dans `CVolProjectile` : le début jusqu'à 0x46cd98 (`Immobile`, `PartagerChute`,
        `CodeDeBalayage`, `BalayerSecteurs`) ; le recul de 0x46cd98 à 0x46ce6b (`Recul`, `Reculer`) ; la glissade de 0x46ce71 à
        0x46cf6a (`Glisser`, `TourDeGlissade` : au plus deux glissades par image, le troisième contact reste au recul) ; la part
        du projectile de 0x46cf6f à 0x46d2f6 (le rebond, la fléchette `Planter`, le frisbee `Devier`) ; la sortie de 0x46d2fc à
        0x46d588 (`PoserApresChute`, `Distance`). La boucle des contacts, lue le 13.11 (0x46cc20 à 0x46cf6f) : le compteur de
        tours est à [esp+0x20], le meilleur contact à [esp+0x68], le contact local à [esp+0xa8], la matrice d'avant le
        déplacement à [esp+0xd0] (c'est l'arg1 passé à `FUN_0046c970`) ; à chaque tour le code de balayage monte, la fraction du
        meilleur repart à FLT_MAX (0x9009b8) et les secteurs de la liste +0x178 sont balayés ; rien touché : la sortie. **Ce qui
        manque pour la composer** : l'angle que `FUN_0046b720(entité, &meilleur, angle)` reçoit pour un projectile contre un
        piéton (`00432830` sur le cap du piéton et `004331c0` sur l'opposé d'un vecteur pointé par ebp, **non identifié** : le
        relire dans le début de la fonction) ; `00443950` et `00469320` de la sortie (non lues) ;
     4. le départ du tir : `FUN_0065f050` (la vitesse de départ) est recréée et branchée (`CViseeTir::VitesseDepart`) ; reste
        `0065e880` (la douille « Muzzle » de l'arme ou l'os de la main, lue au décompilé seulement) : **bloqué**, l'outil n'a ni
        os ni douilles ; il fait partir le projectile du point du tireur, d'où la règle supposée de la première image (`q.sorti`) ;
     5. ce que le contrôle `FUN_004d2290` laisse non suivi : l'autre vol de l'octet +0x15 de l'entrée (la fonction `004d1810`,
        non lue ; le mode de DAT_01b65c14 à la création), les déclencheurs (`006d7220` sur 0x20c7e98), le son du modèle
        (`00598030` sur 0xcf5dc4 puis `0059a710` avec 3 000, ou 8 000 pour « WFtBomb », DAT_00bf4394), l'effet +0xa8 de la fiche
        (lu par l'outil, non joué) ; le redressement de la matrice par `FUN_004d1560` (lue de 0x4d1560 à 0x4d1666) ; la matrice de
        la fléchette plantée (les fonctions `00415320`, `0041a460` avec -π et π, `0046a540`, puis `007014f0` et `00701650` sur la
        position) ; l'ordre du contrôle et du déplacement dans la boucle du monde (supposé : le contrôle d'abord, non lu) ; la
        vitesse angulaire d'un projectile (nulle dans l'outil). Le freinage, lui, est fait (la fiche est lue) ;
     6. les références d'entité dans l'outil : `FUN_00450730` et `FUN_00450780` sont recréées (`src/entities/References.h`), la
        réserve de maillons (le mot 0xc11d40, taille non lue) et le parcours qui remet les champs à nul quand l'entité disparaît
        (supposé, non lu) ne le sont pas ; l'adaptateur de l'outil a des `Lier` et `Delier` vides.
     **Essayé et écarté le 13.11, séance 1** (ne pas refaire) : composer `0046ca40` d'une pièce sans l'angle de
     `FUN_0046b720` (point 3) ; attendre deux liens du meilleur contact dans `BalayerListe` (le second, sur l'entité qu'il vient
     de copier, ne fait rien : `FUN_0045f0a0` sort quand c'est déjà elle ; l'ordre juste pour un contact gardé : le lien du
     contact local, le lien du meilleur par la copie, le délien du local) ; chercher un essai hors mission où le projectile
     retombe sur son tireur (l'outil n'a pas d'option pour tirer vers le haut : `--tirer` vise devant le joueur) ; relire
     `FUN_0046c4b0`, `FUN_0046a340`, `FUN_004761a0`, `FUN_00469f10` et la boucle des contacts de `0046ca40` (tout est lu, les
     plages sont dans le journal du 13.11). **Les tests se rejouent seuls** en 2 min 45 (`tests/construire_tests.sh`, qui compile
     aussi l'outil `lancer_lua`, puis `tests/sans_jeu.sh`) : une erreur de compilation de l'outil sort là, pas dans
     `construire.sh`.
     **Essayé et écarté le 12.11, séance 2** (ne pas refaire) : un `AddImage` de plus pour l'image du monde (la table de
     `CdStream` n'a que huit places, `MAX_CDIMAGES` : à 34,02 s de `1_04` l'arbre `Act/Props/pxTree.act` ne se chargeait plus ;
     l'image déjà ouverte est reprise dans la table par son chemin) ; compter le monde dès la première image du vol (le tir de
     73,36 s de `1_04` touchait à 27 cm le volume « treeclimbbranch » où se tient le joueur, la mission ne finissait plus) ;
     l'essai hors mission avec `--tirer` seul ou `--donner 303` (« le joueur n'a pas d'arme en main » : la bille ne se met pas en
     main ainsi). **L'essai qui marche** : `build/outils/lancer_lua 40 --mission 1_04 --sans-cinematiques --entrer
     _1_04_PARKING@10 --donner 302@15 --tirer @16.6` : la balle 302 part à 16,63 s, touche le mur « nogo_tschoolnop » à 17,06 s
     en (175,47, 9,63, 6,83) à 0,348 m par pas, le sol « fgrd_sc1b18 » à 17,36 s en (175,48, 8,16, 5,48) (« premier contact au
     sol »), et « est au repos » à 17,98 s en (175,48, 7,95, 5,48).
     **Les fiches de freinage** (« DodgeballPhysicsButes » de `Globals.cat`, relevées à l'essai) : par défaut, arrêt sous 0,0015 m
     par pas, × 0,998 dès 0,05 m par pas, × 0,985 en dessous ; BaseballToss (arme 302) et StinkBomb (309) ont ces valeurs ;
     CherryBomb (301, 311), bRocket (308), BouncyBall (325), RegularBall (329), Basketball (381) : 1 et 0,96 ; Wftball (331, 400) :
     0,995 et 0,98 ; soft (346) et RemoteCherryBomb (417) : 0,99 et 0,985 ; de genre 7 sans nom de fiche : 304 (la bille), 312,
     318, 320, 322, 343, 378 ; 346 et 400 nomment une fiche sans être de genre 7. **Les fiches de vitesse** (`FicheVitesse`) :
     toutes ont les champs 104, 108, 112, 116, 120 et 288 ; les bornes sont à −1 sauf un angle de 35° sur une fiche ; la
     dispersion est nulle sauf 0,07 sur la fiche « bRocket ».
     **Le compte** : `tools/avancement.py` prend dans `src` les `FUN_xxxxxxxx` **et** la forme `0x…` du premier octet d'une
     fonction : une fonction non recréée s'écrit sans préfixe (`0046ca40`).
     La table des noms de modèles est en 0xa136e8 (paires nom, adresse de la globale ; `FUN_0043f1e0` la lit, `FUN_0043f220` y
     écrit l'indice) : « dart » = DAT_00bf44dc, « Wfrisbee » = DAT_00bf439c, « WFtBomb » = DAT_00bf4394 ; elle nomme tout
     `DAT_00bf4xxx` comparé à un modèle (+0x10e).
     Puis, comme avant : **le balayage d'une explosion** (la fonction en 0x463b70 qu'appelle `FUN_004d2690`, puis `004629d0`, 2 427
     octets, non lue : qui est touché et avec quels dégâts) ; la marque d'une explosion par la surface du contact (+0x14c de la fiche,
     `FUN_004d2b40`) et la normale du contact dans l'outil (ses marques sont posées au point même) ; les volumes d'eau de
     `FUN_006d48d0` ; le départ du tir (la douille « Muzzle », `FUN_0065e880`) ; le genre d'un coup de projectile (+0x468, le 19e
     argument de `FUN_0047b810`) ; le contrôleur de `Locomotion` du lance-pierre qui va et vient entre `Idle` et son enfant #5ab79a15 à
     chaque image (`1_03`, après 138,04 s) ; ce qui reste supposé de la visée : le point de verrouillage +0x1dc8 que pose
     `FUN_00497d20`, le temps du verrou (+0x80 du composant de visée) et sa liste de candidats (`FUN_004d6dc0`), l'écrivain de +0x54c,
     la caméra de visée (`FUN_00662180`), le point d'une automobile (`FUN_006625f0`, l'effet 2dfx 0x11) ;
   - ce qui reste des entités qui suivent un trajet : leur orientation (`FUN_005eb910` : le cap et le tangage gardés, la rotation
     +0x1c … +0x28 que `FUN_005ec0a0` fait tourner, les angles du segment avec l'octet +0xc, `FUN_005ed570` et `FUN_00433340` ;
     l'outil ne garde que la position de ses CObject), `PAnimFollowPathSoundLoop` (0x5c3e70, la boucle de son +4), l'octet +0xd
     qui arrête l'entité quand sa rotation atteint sa cible, le genre 6 du rappel ;
   - ce qui reste de `1_07` : le dessin de la barre de santé
     et sa mise à jour, `PedSetEmotionTowardsPed` (`FUN_0048f080`, la table 11 × 11 de 0xa349e0, `PedGetEmotionTowardsPed`
     0x48ea70), `PedSetUniqueModelStatus` et `PedGetUniqueModelStatus` (le mot +0x5c du modelinfo : la colonne UNIQUE des .idb à la
     lecture, `FUN_004370d0`, puis le +0x5e recopié par `FUN_00522030` à chaque remise du jeu, `FUN_0042fa40` → `FUN_0051b6b0` ;
     d'où vient +0x5e n'est pas trouvé ; `F_FootballFieldThread` les juge des dizaines de fois par mission), `DoublePedShadowDistance`
     (0x5bb7e0 : l'octet DAT_00c93768), la liste de DAT_00bcd098 qu'épargne `AreaClearAllPeds` (`FUN_00424490`), l'échange social que `FUN_004906e0` arrête, les genres de `FUN_00477ab0` que l'outil ne suit pas (2, 3, 6 à 0xf) ;
   - ce qui reste de `1_06_01` : l'échelle (`/Global/Ladder/Ladder_Actions` et les marqueurs de `_LOWCORONA` et `_HIGHGROUND` : l'outil
     monte au transistor en ligne droite), les marqueurs du radar (`AddBlipForChar`, `BlipAddPoint`, `BlipRemove` : le pool de
     `CRadar`, `FUN_0054a460`, `FUN_0054a2c0`, `FUN_0054a7f0`, `FUN_0054a740`), la diffusion des placements (`CIplStore`, `FUN_00439220`)
     au lieu des entrées collectibles jugées à chaque image, l'objet visible des ramassages (`FUN_004453f0`), les autres genres de
     `FUN_00444ac0` (armes, munitions, santé) et la réapparition, l'historique de la manette hors du mode de l'arbre (`FUN_0053ae10`,
     `FUN_0053b1e0`), le don `Give_Seek` que le joueur quitte avant que le clochard joue `Receive` ;
   - ce qui reste supposé de `1_05` : le composant de visée des piétons de l'IA (le +0xa0 du +0x1c8 que posent `FUN_004d7890` et les
     pistes `SetTarget`, `TargetLock`, `GiveTargetToFocus` : l'outil prend le partenaire de prise pour cible des `PlayOnTarget`), les
     points de synchronisation des modèles (`FUN_00467c30`, FeetSync à la position de l'objet), l'arbre propre d'un objet animé à sa
     création (le départ `RandomLockState` de sa fiche, +0x70) et la descente de son contrôleur (`FUN_005f49a0` depuis sa racine +0x228),
     la vue et les zones interdites du relevé, `PedGetUniqueModelStatus` (0x5cdef0 : le mot +0x5c du modelinfo), les marqueurs
     (`AddBlipForChar`, `BlipRemove`), la barre de santé d'Algie, la ronde du préfet (`FUN_00472320`), le sol (la commande fait courir le
     joueur à 5,9 m de haut aux toilettes et au casier), la tenue de l'écran de fin (0,5 s jusqu'au signal `DAT_020c870b`) ;
   - les objets : les CObject vraiment créés par le chargeur des `.ipb` (son emplacement +0x18), leur modèle de collision au lieu de la
     sphère de 0,5 m, leurs effets et leur remplaçant (`FUN_004d0300`) ; les objets animés hors des déclencheurs des `.dat` (`PAnimCreate`
     0x5c4680, hors de l'index ; la section prop des `.ipb`, `FUN_00438d90`) : que l'arbre de la porte offre lui-même `PedPropActions` au
     joueur, que l'arbre de l'arbre (`pxTree`) le fasse grimper sans `--jouer`, que les portes animées jouent leur arbre (`Door.act`,
     supposé chargé) et leurs événements (`FUN_005b2df0`, `FUN_005b3030`), et `AreaSetDoorPathableToPeds` (`FUN_005eaf00`) ;
   - les ramassages : la fiche (`PickupData/<nom>`, BrickBute), la réapparition, `PickupCreatePoint`, la banque `Weapons/PickUpActions`
     jouée pour de vrai (PropPickup) au lieu de `--ramasser` ;
   - les classes de `AI.act` jugées derrière : `CheckWeather` (`#51223f18`, nommée par sa chaîne), `FactionScan` (`FUN_00600bb0`),
     `TargetFriendsAttacker` (`FUN_00601010`), `SpecialPoints` (`#66349158`, de `/Global/PlayerAI`), `PedVelocity` (`#53408a0c`),
     `AcquirePunishableTarget` (`FUN_00601780`), `InTriggerVolume` (`FUN_005ff8d0`), `TargetProp`
     (`FUN_0065c250`), `HitByPed` (`FUN_005fdae0`), `HasNodeCooldown` (`FUN_006064c0`), `PedSphereHit` (`FUN_00605af0`), `HitFlagClear`
     (`FUN_005fbfa0`), `AcquirePOI` (`FUN_00448a40`), `IsPlayerFatigued`, `WeaponEquipedSocket`, `TargetProjectile` ;
   - le composant de visée du joueur (+0x1c8, `FUN_004d88f0` à chaque image : la cible +0xa0, lâchée quand elle meurt par
     `FUN_004d7c30` → `FUN_004d6710`) et le piéton à terre que l'outil frappe encore (son `KnockOuts` recommence à chaque coup) ;
   - `PedSetHitRecordDamage` (`FUN_005c97c0` : l'emplacement 16 de CPed, le contexte +0x300 du composant, son objet +0x10, +0x38) ;
   - les pistes `EnableFightTokens` / `DisableFightTokens` (`FUN_00486960`, `FUN_004869c0`) et l'octet +0x1e25 du composant de combat ;
   - ce qui reste du choix d'une cible : la riposte (`FUN_00496ee0`), la conversation, les zones interdites (`FUN_0046ee60`) ;
   - le sol du monde (les collisions des `.col` posées par les `.ipb`, déjà lues par `outils/visite`) : les déclencheurs comme
     `_1_03_DAVISREGISTERHIT` sont plus hauts que les nœuds des trajets ;
   - ce qui reste supposé dans `CombatLoco` : le point valable (`FUN_006254c0`), la recherche de chemin ;
   - les sphères des os des coups (`FUN_005f8e20`) au lieu du demi-espace avant ;
   - les `SpeechTrack` sans tenue.

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

- Le balayage d'une liste de secteur, recréé et branché dans l'outil des missions (13.11, séance 1 ; 0bfcdda, c4d4121, b25f6ee, 42d6883, 6b47b2f, 58f30bf, f54307f) : `FUN_0046c4b0`
  (`CVolProjectile::BalayerListe`, `EntiteBalayee`, `VolumeBalaye`, `src/objects/Projectiles.h` ; lue de 0x46c4b0 à 0x46c877, 39
  vérifications dans `test_armes` sur un monde d'essai) ; avant lui, tout ce qu'il appelle : les deux filtres d'un projectile
  (`FUN_004761a0`, `ProjectileIgnore` : le tireur n'est jamais touché ; `FUN_00469f10`, `VehiculeIgnore`), le ballon prisonnier
  (`FUN_006838d0`, `FUN_0067b0f0`, `FUN_00683a10`, `FUN_00680b60`, `FUN_0067b0b0`, `FUN_0067b100`), l'entrée de la table des projectiles
  (`FUN_004d3360`), la méthode +0xa4 d'un `CProjectile` (`FUN_0046a340`, `TesterCollision`, avec `FUN_00447290`), le centre
  (`FUN_00466b50`, `FUN_004660b0`, `FUN_0046d700`), `FUN_0051ae00`, `FUN_004ea210`, `FUN_00415e50` et `FUN_004153b0`.
  Le branchement dans l'outil (`outils/lancer_lua.cpp`, `CBalayageOutil`) : les projectiles de l'outil passent par
  `BalayerListe` au lieu de ses trois boucles (une liste par projectile et par image : les piétons, les objets cassables, le
  monde) ; le tireur n'est plus jamais touché (`FUN_004761a0`), au lieu d'épargné 200 ms ; le test lui-même reste celui de
  l'outil, supposé.
  La glissade de `0046ca40` (0x46ce71 à 0x46cf6a, avec le recul de 0x46cd98 à 0x46ce6b : `Reculer`, `Glisser`, `TourDeGlissade`,
  et `FUN_0041c390`, `PoserPosition`) : la fonction est lue en entier, recréée par morceaux, pas au compte. Les
  références vers une entité (`FUN_00450730`, `FUN_00450780`, `FUN_00465680`, `FUN_00465690` ; `src/entities/References.h`). Les sept
  missions gardent leurs temps.
- Le sol sous les projectiles de l'outil des missions (12.11, séance 2 ; 07037ca, db4c5c1, be1f0c4, d18b45b, fc34c3d, 2506278, 5bfab15) : `CMondeCollision::Trajet`
  (`src/collision/Marche.h`) et la collision du monde dans `AvancerProjectiles` (`outils/lancer_lua.cpp` : les 488 .col de
  `Stream/World.img`, les placements « inst » de la zone ; « le projectile N touche le monde (placement) en (…) », le premier sol,
  le repos ; l'option `--donner ARME@s`) ; la vitesse de départ d'un tir (`FUN_0065f050`, `CViseeTir::VitesseDepart` dans
  `src/peds/Armes.h` : la durée du vol tirée de la distance à plat, vingt essais contre les bornes +0x68 à +0x78 de la fiche, la
  dispersion +0x120 ; `1_03` aux briques finit à 147,97 s au lieu de 148,04 s) ; le freinage dans l'outil (la fiche
  « DodgeballPhysicsButes » de `Globals.cat`, `Freiner` avant `Controler` pour le genre 7) ; l'entourage du balayage du
  déplacement dans `CVolProjectile` (`FUN_0046c970`, `FUN_0046a920`, le début et la sortie de `0046ca40`, `Recul` à la constante
  exacte) ; le test des sphères (`FUN_00465bc0`) ; les trois filtres de piéton du balayage d'une liste (`FUN_004760b0`,
  `FUN_00476160`, `FUN_00476270`) ; la référence d'un contact (`FUN_0045f0a0`) et la règle des deux projectiles de même modèle
  (`MemeSens`, cosinus au-dessus de 0,866). Le balayage d'une liste (`0046c4b0`) est lu en entier, pas recréé : premier point de la suite.
- La physique d'un projectile dans le monde (12.11, séance 1 ; d37cc93, dbacaa5, 4c7b7c4, fb6074b, a181812, d61ab96, 4777fc2, 949c743, 698e156) : `CVolProjectile` dans `src/objects/Projectiles.h`. Le vol
  (`FUN_004494d0`, `FUN_004d1510`, `FUN_00469760`, `FUN_004d2160` : la gravité +0x230 × 0,008 par pas, le déplacement, l'unité de
  +0x124 = le mètre par pas) ; l'air (`FUN_004698c0` : 0,99999 puissance le pas) ; le repos (`FUN_00469a20` : onze images sous
  0,003 fois le pas) ; le rebond (la part du projectile dans `0046ca40` : le recul de 0,005, 0,75 et 0,75 contre un piéton,
  l'élasticité +0x164 sinon) ; la paire du contact (`FUN_0046b720` : ni le tireur du jeu 0xd, ni un projectile bas et lent) ; le
  contrôle (`FUN_004d2290`, `FUN_004d2090` : l'effet près du joueur, le choix du vol, l'échelle de la chute) ; la poussée (`FUN_004d1ec0`) ; le freinage et la mise au repos (`FUN_004cf350`, `FUN_0040df90`, `FUN_004cf190`) ; la fléchette qui se
  plante et le frisbee dévié (`Planter`, `Devier`, `FUN_004150b0` ; la table des noms de modèles 0xa136e8, `FUN_0043f1e0`).
  L'outil ne retire plus un projectile à son contact : `1_04` écrit vingt et un rebonds (les trois vitres à 17,52 s, 19,50 s et
  21,52 s), `1_03` aux briques trois. Les sept missions et leurs temps sont inchangés.
- Le haut du corps du lance-pierre qui repart, le choc, le contact, les explosions, la fin et les marques des projectiles (11.11,
  séance 1 ; 54c60aa, 4fe6762, 3ef9480, 61771cd, 857cb1a, c87c3d2, f5f9973, 74427aa) : le contrôleur d'actions qui se vide à la fin
  d'un nœud sans suite (`FUN_005f49a0`), la durée d'un nœud par ses seules pistes retenantes (`FUN_005f4230`) : `1_04` casse ses trois
  vitres au seul bouton 8 (17,56 s, 19,54 s et 21,55 s), `1_05` finit à 180,31 s et `1_07` à 150,88 s ; le choc d'un projectile
  (`FUN_004d3860`, `FUN_004d37d0`, `CChocProjectile`), son contact (`FUN_0046ad90`, `CContactProjectile`), la table des explosions
  (`FUN_004d2b40`, `FUN_004d2aa0`, `FUN_004d2690`, `FUN_004d92b0`, `CPlacesExplosions`), la fin d'un projectile (`FUN_004d45e0`,
  `FUN_004d3dc0`) et les marques d'impact (`FUN_00560f70`, `FUN_00560f10`, `CPlacesMarques`, les deux marques d'un bâtiment de
  `FUN_0046ad90`) ; les sept missions restent réussies.
- La visée du joueur en entier, la vie et le test d'eau des projectiles, la piste spawn qui meurt (09.11, séance 1 ; 198fc0b, ec11096,
  df3895b, 319d758, b8599fe, 304b947, 5184a36, b6fcc8c, 16aa299) : `PedSetProjDest` (`FUN_0048bd40`, `CDestinationsTir`), la source de
  la cible du tir (`FUN_00662a40`, `CViseeTir::SourceCible`, `FUN_004d7630`), le bouton 10 (`FUN_007386f0`, `FUN_0050c730`,
  `CManette`), le point devant le tireur (`FUN_0065ea50`), l'imprécision (`FUN_006626a0`, `FUN_0065eb40`, `FUN_004d5cd0`), la vie du
  projectile (`FUN_004d4c40`, `PedSetProjectileLifetime`), la piste spawn au contrôleur vide (`FUN_005f72e0`), le test d'eau et le
  fondu (`FUN_006d48d0`, `FUN_004d45e0`), `PedTargetPAnim` (0x5cfe50, `VerrouillerObjet`).

- `AreaClearAllPeds` branchée et la suite de 1_07 et des armes (07.11, séance 3 ; c60757f, 2a54c45, f5a14d7, 68ecb38, 9caccfa) : `AreaClearAllPeds`
  (0x5b7e70, `CScriptPedPool::AmbiantsAVider`, les commandes de `1_04` et `1_05` refaites), `PedClearHitRecord` (0x5cfde0),
  `PedSetMinEngage` (0x5cb790) et `EngageDistance` (`FUN_00602070`), `WeaponPlayerFireAutoTarget` (`FUN_00661ee0`, l'octet +0x1ec0
  du joueur), les décalages de visée de `WeaponFire` (`CViseeTir::DecalageVisee`, `FUN_00662760`).

- Les entités qui suivent un trajet, la visée et la table des projectiles (07.11, séance 2 ; d841781, a9e14ca, d9cc993, c07c7e0, 9944415, d06b095, 2f4706b, 74a7b18, 99dbfab) :
  `IsPlayerFatigued` (`FUN_005fc3a0`), l'avance sur une cible qui bouge (`CViseeTir::AvanceSurCible`, `FUN_0065ecb0`), la table des
  projectiles (`CPlacesProjectiles`, `src/objects/Projectiles.h` : `FUN_004d4590`, `FUN_004d3f60`, `FUN_004d3dc0`) et leur vie
  (`FUN_004d45e0`, `FUN_004d48a0`), le PathReference (`CReferenceTrajet`, `src/entities/SuiviTrajet.h` : `FUN_005ed750`,
  `FUN_005ed620`, `FUN_005ed6f0`, `FUN_005ed400`, `FUN_005ed470`, `FUN_005ec510`, `FUN_005ec760`), le gestionnaire 0xd13730
  (`CSuiveursTrajet` : `FUN_005eb880`, `FUN_005ebfd0`, `FUN_005ebd60`, `FUN_005ebee0`, `FUN_005ebf80`, `FUN_005ebbf0`, `FUN_005ebb50`,
  `FUN_005ebba0`, `FUN_005ec3a0`, `FUN_005ec0a0`), `PAnimFollowPath` (0x5c3b30), `PAnimStopFollowPath` (0x5c3d50),
  `PAnimSetPathFollowSpeed` (0x5c3c60), `PAnimGetPathFollowSpeed` (0x5c3ce0), `PAnimFollowPathReset` (0x5c3c00), le rappel des nœuds
  atteints (`FUN_005d9e70`).

- La suite des armes, deuxième partie (06.11, séance 7, et 07.11, séance 1 ; ca12e67, fa5abcb, 228e02d, 83e971c, 4542548, f6d6093,
  c408a7f, d24e491, 939d868) : le pas du contrôleur d'actions (`CScriptPedPool::PasDuControleur`, `FUN_004812d0`), la manette qui tient
  un bouton (`lancer_lua --tenir`, `FUN_005f3930`, `FUN_005f3720`), les réglages du tir (`CReglagesTir` : `FUN_0065d3f0`, `FUN_0065d430`,
  `FUN_0065d350`, `FUN_0065d160`), les dégâts du tir (`CChargeArme::Degats`, `FUN_00664710`), l'objet arme (`FUN_004da610`,
  `FUN_004d92c0`, `FUN_004da6f0`), `AttackTimerReset` (`FUN_006121d0`), `HasRangedWeapon` (`CInventaireArmes::ArmeADistance` et
  `MunitionsPour` : `FUN_00605a80`, `FUN_00486ae0`, `FUN_00486130`), `DetectProjectile` et `DetectGrenade`
  (`CScriptPedPool::VoitPoint`, `DetecterProjectile`, `DetecterGrenade`, `GrenadeAPortee` : `FUN_0048cf50`, `FUN_0048a300`,
  `FUN_0048a130`, les juges `FUN_00604410` et `FUN_00604380`), le haut du corps du joueur en contrôleurs à part
  (`CActionController::arbreDesLances`, `AppliquerLances`, la référence montée descendue à l'entrée, `FUN_005f5470`).

- La suite des armes, première partie (06.11, séance 6 ; a464efb, 3391c9f, 8fe559d, 671ceda) : la charge de l'arme (`CChargeArme` dans `src/peds/Armes.h` :
  `FUN_0065e7c0`, `FUN_0065e810`, la charge de `FUN_00661f10` et sa remise par `FUN_00664710`), les drapeaux des armes
  (`CInventaireArmes::PoserDrapeau` et `LireDrapeau` : `FUN_00661d40`, `FUN_0065bb20`, `PedSetWeaponFlag` 0x5cf090), la minuterie de
  mission (`src/hud/MinuterieMission.h` : `FUN_0043f600`, `FUN_0043f540`, `FUN_0043f560`, `FUN_0043f5a0`, `MissionTimerStartFixed`
  0x5c1ff0, `MissionTimerPause` 0x5c2090), `FaceTarget` des contrôleurs de l'IA par `FUN_00477ab0`.

- `1_07` se termine réussie à 151,47 s (06.11, séance 5 ; d4c9c00, 5e4adb0, c5a6b07, 03efeb4, 7d694fe, 168133b, e6381ce, c53b94e, f7687d6) : `PropTargetInteractive` (`FUN_00655110`, le relevé et le
  choix de `TargetProp` mis en commun), `PickupCreateFromPed` (0x5d0cc0, `FUN_00443d90` : `CRamassages::CreerDuPieton`), `PedHasAlly`
  (0x5cd730), `PedIsModel` (0x5c92a0), `PlayerUnequip` (`FUN_0055c780`), `PlayerSetPosSimple` (0x5d2bd0), `PlayerFaceHeadingNow`
  (0x5c7360), `PAnimDelete` (0x5c4930), `FUN_00477ab0` (`CScriptPedPool::CibleDeGenre` : `FaceTarget` et `SeekEntityPosition` vers
  l'objet visé et l'allié), la barre de santé secondaire (`src/hud/BarreSante.h` : `PedShowHealthBar` 0x5ca5d0, `PedHideHealthBar`
  0x5c6770, `FUN_00553690`, `FUN_00551380`, `FUN_005513b0`), `PedMakeMissionChar` (0x5ca890, `FUN_004770a0`), `PedStopSocializing`
  (0x5cdbf0, `FUN_00490f50`) ; `AreaClearAllPeds` (0x5b7e70) recréée dans src, pas encore branchée.

- `1_07` va jusqu'à la porte du garage (06.11, séance 4 ; 1054924, bebd02c, de6c457, 6fcb4cb, d6444c1) : `PAnimCreate` (`CObjetsAnimes::CreerParScript`, la matrice et
  `FUN_0044cd20` sans les conditions de `FUN_00416540`), `PedFindInAreaObject` (0x5d0a30), `CreatePersistentEntity` (0x5bd090 : les
  modèles panm en objets animés sans déclencheur, `CObjetsAnimes::CreerPersistant`, les autres en CObject) et `DeletePersistentEntity`
  (0x5bca50), les marqueurs du radar (`src/hud/Marqueurs` : les 300 traces de CRadar, `FUN_0054a460`, `FUN_0054a2c0`, `FUN_0054a7f0`,
  `FUN_0054a740`, `FUN_00548aa0` ; `AddBlipForChar`, `BlipAddPoint`, `BlipAddXYZ`, `BlipRemove`).

- `1_06_01` se termine réussie à 61,81 s (06.11, séance 3 ; ef92311, f492186, dda40f8, 469ff55, e9c2865, d9701dd, 492c182, 1412b5e, 261b63d) : le gestionnaire des collectibles (`src/objects/Collectibles` :
  l'objet 0xbceb50, `FUN_00428bf0`, `FUN_004287a0`, `FUN_00426080`, `FUN_004288c0`, `FUN_00428ba0`, les fonctions Lua `Collectibles*`),
  les objets du joueur (`CObjetsJoueur`, DAT_00cf0060 : `FUN_00541ae0`, `FUN_00541c10`, `FUN_00541cb0`, `ItemGetCurrentNum`,
  `ItemSetCurrentNum`), les ramassages dans src (`src/objects/Ramassages` : `FUN_00443af0`, `FUN_00443e90`, les bits de +0x46), les
  collectibles des placements (`FUN_00438790`, la population `FUN_004a0130`, `FUN_0049f2f0`, `FUN_00445640`), la prise au contact
  (`FUN_00445970`, `FUN_00445710`, `FUN_00444ac0` pour les objets), `ObjectRemovePickupsInTrigger` (`FUN_004448c0`), l'historique des
  boutons (`src/hud/ButtonHistory` : `FUN_0053abb0`, `FUN_0053ad50`, `FUN_0053b2e0`, `FUN_0053b370` dans le mode de l'arbre,
  `FUN_0053c250`, la piste `ActionRegisterButtonHistory` et `FUN_0053b340`), `PAnimRequest`.

- `1_05` se termine réussie à 185,20 s (06.11, séance 2 ; 33048d4, 40d3c33, 6a129cc, 477c762, ab53885, ce870cc, 910cf23, b6527da,
  bae6062) : les objets animés des déclencheurs (`src/objects/ObjetsAnimes` : `FUN_00416540`, `FUN_0044cd20`,
  `FUN_00416490`, la fiche PropButes de `FUN_00520460` et `FUN_00729b70`, `CActionBank::ButesCherche`, `AreaRegisterAnimProps` vide),
  `PAnimExists`, `PAnimSetActionNode` (`FUN_004ef1f0`), `PAnimSetPropFlag`, `PAnimGetPropFlag`, le contrôleur figé de l'objet, les pistes
  `PropFlags`, `PropSetPedAction`, `Sync`, `PropInteract`, les conditions `PropFlags`, `PropPedIsUsingMe`, `PropModelID`,
  `PedConditionGroup` et `TargetProp` (`FUN_0065c250`, le relevé `FUN_004989f0`, `FUN_00497e90`, le choix `FUN_004979b0`), l'écran de
  fin (`src/hud/EcranDeFin.h`), les objectifs de mission (`src/hud/ObjectifsMission.h`), le nœud `Run` des missions et les pistes
  `Script` à un nombre (`FUN_006abf20`, `FUN_006ab510`, `FUN_006851a0`), `PlayOnTarget` qui attend sa cible (`FUN_006615f0`), les
  portes animées (`FUN_004eebc0`, `FUN_004eed70`, `PAnimOpenDoor`, `PAnimCloseDoor`, `PAnimDoorStayOpen`, `PAnimIsOpen`, les portes
  nommées par leur objet) ; `1_06_01` attend le joueur dans `_FIGHTINGAREA`.

- `1_05` va jusqu'à sa troisième étape (06.11 ; fc7617d, e4918b5, cc43ab6, f1538a0, a147a19, 167fc76) : les rondes des préfets
  (`src/peds/Patrouilles` : le gestionnaire `DAT_00c2b034`, `FUN_004720e0`, `AreaDisablePatrolPath` et ses voisines, `DATUnload`),
  `PedIsHit`, le groupe des alliés (le meneur +0x2f4, le suiveur +0x2f8, `FUN_00485fa0`, `FUN_00485880`, `PedRecruitAlly`,
  `PedDismissAlly`, `IsAllyFollower`, `IsAllyLeader`, `IsTargetAlly`, `HasAllyState`, `HasAllyMorale`, `AllyLeaderConditionGroup`,
  `AllyFollowerConditionGroup`, `AllyIsInCombat`), la piste `TetherLoco` (`FUN_00627fc0`, la place `FUN_00485490`), les alliés qui passent
  les portes (`FUN_0048b970`, `FUN_0048af70`), `PedSetMissionCritical` et le rappel critique (`FUN_004782a0`), `PedFindInAreaXYZ`,
  `SoundStopCurrentSpeechEvent`, `PedRestrictToTrigger`, `PedSetInfiniteSprint`, `PedCanTeleportOnAreaTransition` ; `--courir`.

- `1_04` se termine réussie à 81,05 s (05.11 ; 21d2552, 493e338, 04c5ba0, 40fba0d, 530271b, 082a5d4, 7430472, 61114d6, e0d8e5e) :
  `PedGetOffsetInWorldCoords`, `PedFaceHeading`, `PedSetStationary` ; les types des armes (les fiches ObjectButes de Weapons/Info,
  `FUN_0050eaa0`, `WeaponGetType`) et l'inventaire (`ms_pPool654`, `FUN_0045cb70` … `FUN_0045cf70`, `src/peds/Armes`) ; l'arme en main
  (`PedSetWeapon`, `PedSetWeaponNow`, `PlayerSetWeapon`, `PedGetWeapon`, `WeaponEquipped`…, les pistes `PropSetDesired`, `PropGetDesired`,
  `PropAttach`, `PropDetach`, `PropSetInTransition`, le HUD des armes, onze conditions) ; les projectiles (`WeaponFire`, `FUN_0065f050`,
  les fiches ProjectileButes, les marques, `ObjectNumProjectileImpacts`) ; les objets cassables (les entrées spec des `.ipb`, object.dat,
  `RegisterHashEventHandler`, `PAnimDestroyedByPed`) ; les arbres des objets diffusés depuis Stream/World.img (`pxTree.cat`) ; les
  gestionnaires globaux (`RegisterGlobalEventHandler`, les événements 7 et 1) ; `PedGetLastHitWeapon`, `PedGetWhoHitMeLast`, les
  ramassages (`PickupCreateXYZ`, `GetPointListSize`), `PedCanSeeObject`, la minuterie de mission ; `--arme`, `--viser-objet`, `--tirer`,
  `--ramasser`. **`1_03` finit aussi aux briques** (148,04 s) et **`1_02` par son vrai chemin** (132,92 s) ; `1_01` à 10,46 s.

- `1_03` se termine réussie à 148,90 s (04.11 ; 4bad48f, f6ffb2a, 8858b63, 83d005d, 2d90228, efb107a, 56e2e07, 7d48737, 85ab5d8) :
  les gestionnaires des événements des déclencheurs (`RegisterTriggerEventHandler` 0x5bbfa0, `SetNumberOfHandledTriggerEventObjects`,
  `AreaSetTriggerMonitoringRules`) et leur suivi à chaque image (`FUN_006d6f60`), la piste `Script` (`FUN_006851a0` : la porte du
  garage appelle `cbDoorOpened`), les arbres du joueur (`/Global/Player`, `/Global/PlayerAI`, `FUN_00484d10`), `PlayerSetControl`
  (0x5d12f0 : la manette coupée, +0x1e90), la laisse (`PedSetTetherToPoint`, `IsTethered`), `PedIgnoreStimuli` et
  `SetAmbientPedsIgnoreStimuli`, l'état des portes (l'objet 0xc3b808), `IsSocializingWithPlayer` et `TargetedByPlayer` : avec la
  riposte de l'outil, les vagues de `WaveInit`, les deux portes, Davis sur ses tonneaux, `CutsceneDavisBegs` et `MissionSucceed` ;
  **`1_02` reste réussie à 124,97 s**, `1_01` à 10,46 s.

- Le bully 5 de `1_03` frappe par son arbre (03.11 ; cbf3095, 1d2506a, 0372668, 925d09f, 45b2db1, ad3d979, 888d4c3, e3d11ce,
  1148862) : les arbres du modèle posés à la création (`FUN_00484d10`, le modelinfo +0x64 … +0x70), 26 classes de conditions de
  `AI.act` (véhicules, lieu et rôle, état global avec `EnterNIS` / `ExitNIS`, les dernières avec la piste `SetActionTimer`), les
  droits d'attaque (`FUN_00486c40`, `IsTargetAttackRightsTaken`), `stat` et son tirage (0x5ffff0), les dés des contextes
  (`WeightedRandom`, `Random`, `RandomDiceRoll`, le tirage de l'image +0x1748), les jetons de combat (`FightToken`, `FUN_00486e40`) et
  les piétons proches (`FUN_0047d1d0`) : **`1_02` reste réussie, à 124,97 s** avec dix-huit jabs ; dans `1_03`, le joueur, qui ne
  riposte pas, meurt sous les coups des bullies 1 et 5 à 101,31 s.

- `1_03` jusqu'au garage (02.11 ; 860a312, 126b5b0, 958b996, 69a6217, d2a992b, a7edecd, e5c2942, 37ada88, 41fbf52) : `--suivre`
  et `--viser`, `PedSetPosPoint` (l'origine d'un piéton est à ses pieds, `FUN_005712f0`), `PedFaceObject`, `PedFaceObjectNow`,
  `RegisterPedEventHandler` et le coup reçu (`FUN_005b2190`), `PedClearObjectives`, `PedAttackPlayer`, `PedMakeTargetable`, les
  actions des nœuds de trajet (`FUN_0062e780`), `PedOverrideStat`, `PedSetFlag`, les attitudes (`CAttitudeSet`,
  `CVariableAttitudeSet`) et le respect des factions, le choix d'une cible par l'IA (`FUN_004992a0`) : **`1_02` reste réussie à
  121,94 s**, `1_03` va jusqu'au garage.

- La mort, les dernières conditions et le suivi de trajet (27.10, nuit, 2 ; bd5a363, fb991db, 01693c4, 082a35e, 474710f, 9befc54,
  e40e7a5, 82d730e, 40241d1) : la piste `Death` (`FUN_00479110`) au lieu de la santé nulle, `ParametricTime`, `PropTargetInteractive`,
  `MissionUnlocked` et `MissionForceCompleted`, `HitTime` à -1 sans coup, la vue (`FUN_0048d320`), `DamageTarget`, `tests/test_surface`
  réparé, `PedFollowPath` et la piste `FollowPath` (le rayon de 1 m, `FUN_0048c430`) : **`1_02` reste réussie à 121,94 s**, `1_03`
  joue sa poursuite d'introduction.

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
