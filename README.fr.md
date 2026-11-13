# bully-re

*Version française. [English version](README.md).*

Recréation du moteur de **Bully: Scholarship Edition** (PC, 2008) à partir de
l'exécutable, à la manière de re3 et reVC pour GTA. Projet de longue haleine,
commencé le 21 septembre 2026.

Le jeu n'est pas inclus et ne le sera jamais : ce dépôt ne contient que du code
réécrit, des outils d'analyse et de la documentation. Pour l'utiliser il faut
posséder le jeu (Steam ou disque).

## Ce qu'on sait déjà

- `bully.exe` v1.0 : 8 Mo, non protégé, RTTI intact, Gamebryo 2.3, Lua 5.
- 20 406 fonctions, 4,4 Mo de code, 1 522 classes reconstruites depuis le RTTI
  ([docs/classes.md](docs/classes.md)), 66 racines d'héritage.
- Le modèle d'objets est celui de GTA : `CPlaceable → CEntity → CPhysical →
  CVehicle → CBike`, `CPed`, `CPlayerPed`. Rockstar Vancouver a repris la base
  de Rockstar North. Les noms et l'ordre des méthodes virtuelles de re3 / reVC
  servent donc de point de départ.
- Au-dessus, un système d'arbres d'actions propre à Bully (`ActionNode`,
  `Condition*`, `*Track`, `*Objective`) qui pilote les personnages et les
  activités.

## Avancement

**8,0 % du code de `bully.exe` recréé** (1271 fonctions sur 20 819), 12,7 % compris
(code, documentation, noms). Le détail par sous-système est dans
[docs/avancement.md](docs/avancement.md), régénéré à chaque commit par
`tools/avancement.py`.

Les **scripts Lua du jeu tournent** dans `outils/lancer_lua`, sur la VM Lua 5.0.2
d'origine : `main.lua` et ses threads image par image, la banque des missions lue
dans les arbres d'actions, et la première mission, `1_01`, jouée jusqu'à
`MissionSucceed` — sa cinématique `1-1-1` lue dans `Cuts.img` (61,7 s), les
changements de zone par la machine à états du moteur (fondu, scripts de zone
retirés puis chargés, portes appariées par `DATInit`), la parole scriptée, les
piétons qu'elle crée et fait marcher. Depuis le 21.10, à la nouvelle partie, **`1_02` s'enchaîne
seule** après `1_01` (les conditions des nœuds de mission lues : `MissionCond`, `TimeCond`, `DayCond`,
`NOT`, `OR`), sa narration se précharge et dure le temps de ses vrais échantillons (`Speech.bin`), le
joueur sort de l'école par une porte `DT_` et la mission va jusqu'au combat d'entraînement contre Russell.
Depuis le 22.10, le contrôleur d'actions du joueur descend dans l'arbre que pose `PedSetActionTree` (les
68 arbres globaux sont chargés au démarrage, comme le moteur) : l'entraînement démarre, et avec `--jouer`
(la parade, puis `0_BLOCK_0`) la leçon de parade passe. Depuis la seconde séance du 22.10, les conditions des nœuds sont jugées (`ConditionGroup`, `WeaponEquiped`, `ActionRequest`, `ActionNodePlayTime`, `TargetPlaying`, `LuaCond`) et chaque piéton a un contrôleur d'actions qui redescend dans son arbre quand le nœud joué finit : la séquence de boutons, la prise et sa charge passent (`--demander`, `--sequence`). Depuis la troisième séance du 22.10, les passages viennent des pistes elles-mêmes, comme dans le moteur : `Opportunity` (et son loquet aléatoire), `Sequence` et `PlayOnTarget`, avec leur règle de priorité ; le bully poussé joue `PushFwd/RCV` et la leçon de prise se termine. Depuis la quatrième séance du 22.10, les coups portent : les pistes `Collision` (le coup touche la cible verrouillée, l'outil n'ayant pas les os), la réaction de la cible par l'arbre des coups (`ConditionFlag`, `MePlaying`), une `Sequence` qui sort de l'arbre posé, et la fin des animations lue dans `MAINPED.HXD` ; poussé, le bully tombe, et le coup de pied lourd au sol lui fait jouer `GroundHitHeavy` : la leçon des coups au sol passe et `1_02` affiche son objectif de combat. Depuis la séance du 23.10, les coups blessent : un `Execute` ajoute les pistes du nœud visé à celles du contrôleur, comme dans le moteur, et la `DamageHandle` de `HitTree/…/Base/HitBase` ôte les dégâts du coup reçu ; les piétons reçoivent leur entrée de `pedstats.dat` (santé, régénération, seuil d'humiliation), les pistes `SetHealth`, `SetPedFlags` et `SetTargetAsGrappleTarget` sont jouées et onze conditions des coups sont jugées (`HitTime`, `Health`, `IsHealthAtHumiliationLevel`…) ; dans `1_02`, le bully se relève, onze jabs le mènent à 5 points de vie et la mission demande de l'humilier (`1_02_HumiliateHim`). Depuis le soir du 23.10, **`1_02` se termine réussie** (119 s de jeu) : le groupe d'humiliation de la victime (`+0xd44`, choisi par sa taille quand sa santé passe sous la moitié, par la piste `ManageTargets`) est lu par `HumiliationGroupIs` ; les conditions propres d'une `Opportunity` sont jugées (le mot du fichier vaut 0, la seconde table de renvois mène à une liste de conditions : 951 des 3 080 du démarrage en ont) ; les groupes d'animations (`LoadAnimationGroup`, `AnimationGroupLoaded`), les lignes de parole retenues (`SpeechTrack`, `SpeechStartTrack`, `IsSpeechPreloaded`), `TargetRelativePosition`, `FaceTarget` et `SeekEntityPosition` sont joués, et `PedFlags` est jugée. Le bully joue la vraie humiliation `5-8_NoogieSpit`, perd ses derniers points par `HumiliateFlee`, joue `HitTree/KnockOuts`, et `F_SetupRussellFight` joue la rencontre avec Russell avant `MissionSucceed`. Depuis la nuit du 23.10, **l'arbre de l'IA du bully est joué** à la place d'un geste d'attaque scripté : `PedSetAITree` pose un second contrôleur par piéton, `PedAttack` met un vrai `AttackPedObjective` dans la file des objectifs du piéton, dix-huit conditions de l'IA sont jugées (`Objective`, `AcquireTarget`, `JoinCombatZone`, `InCombatZone`, `Interval`, `WeightedRandom`, `ActionNodeRequest`… ; `LuaCond` lit désormais le résultat du script comme un nombre, `0.0` est faux), et les pistes `ManageTargets`, `CombatLoco`, `ActionNodeRequest`, `spawn` (avec son propre contrôleur), `ClearCurrentObjective` et `FleeObjective` sont jouées ; un nœud vit tant qu'une de ses pistes à l'octet +8 vit, comme dans le moteur. Dans `1_02`, le bully se tient dans sa zone de combat à 1,5 m, son `CombatActions` boucle, le groupe d'humiliation vient de la mise à jour de `ManageTargets`, l'humiliation rend la main à l'arbre du joueur, et le bully qui fuit tombe sans toucher le joueur : `1_02` se termine toujours réussie (119,03 s). Depuis le 26.10, **les références montées des arbres d'actions** sont lues : un fichier référencé (`Locomotion.act`, 42 fichiers) n'existe qu'une fois sous `/Global`, et son parent dépend de la clé du contexte, le nom de l'arbre de tête (`FUN_005f6960`, `FUN_005f6800`, `FUN_005f6290`) ; `LuaCond` appelle la fonction dans l'espace de noms de la mission (`NSCall`) : pendant la leçon de parade, le bully attaque vraiment le joueur (`Offense/Short/Strikes`, par les `ActionOpps` de `Locomotion.act`) ; le type et le sexe des piétons, l'intention de mouvement, le chapitre et la saison, et seize conditions de plus sont lus. `1_02` reste réussie (119,03 s). Depuis le 27.10, la phase de l'attaque que tient la piste `Collision` (`CollisionStart`, `AttackPhase`, `AttackTimer`), les composants du HUD, le gestionnaire de caméras, les manettes et l'état social sont lus, avec seize conditions de plus ; la vitesse de `CombatLoco` vient de son pas (`FUN_00625970`) : le bully gagne sa zone en sprint. `1_02` reste réussie (119,03 s). Le soir du 27.10, les règles de la vitesse de l'IA sont lues (les entrées vidées par `FUN_005f38e0` et la piste `IDLE`, la vitesse de la navigation `FUN_005e4a60`, les quatre sous-états de `CombatLoco`, l'arrêt au petit rayon : le bully tourne autour du joueur en lui faisant face), les piétons de l'IA appuient sur des boutons (`ActionRequest` lit le bouton brut), et **le bully pare** : le bouton 12 le mène dans `Defence/Block`, et un coup reçu là va dans `HitTree/Standing/BlockHits` ; une `Opportunity` prise fait finir son nœud même sans nœud visé. Huit conditions de plus sont jugées (`WeaponNeedInit`, `ConversationActive`, `OnMission`, `MissionCond`, `PedIsInOutfit` avec la garde-robe du joueur, `AttackerConditionGroup`…). `1_02` se termine réussie à 121,94 s, en quinze jabs. Dans la nuit du 27.10, un piéton ne meurt plus à santé nulle mais par la piste `Death`, comme dans le moteur ; `1_02` n'a plus aucune condition non lue (`MissionUnlocked`, `ParametricTime`, `PropTargetInteractive`), la vue des piétons (le cône et la portée de `pedstats.dat`) et les coups des prises (`DamageTarget`) sont lus, et **`1_03` démarre** : `PedFollowPath` et la piste `FollowPath` font suivre leurs trajets au joueur et à Davis, jusqu'à la poursuite que l'outil ne joue pas encore. Le 02.11, **`1_03` va jusqu'au garage** : l'outil joue la poursuite (`--suivre`, `--viser`), un coup reçu prévient les scripts (`RegisterPedEventHandler`), les actions des nœuds de trajet, les drapeaux, les stats et les attitudes des piétons sont lus, et l'IA choisit sa cible comme le moteur (`FUN_004992a0`) ; `1_02` reste réussie (121,94 s). Le 04.11, **`1_03` se termine réussie** (148,90 s) : les événements des déclencheurs (`RegisterTriggerEventHandler`, suivis à chaque image pour chaque piéton), la piste `Script` des arbres d'actions (la porte du garage qui s'ouvre appelle la mission), les arbres du joueur (`/Global/Player`, `/Global/PlayerAI`), `PlayerSetControl`, la laisse, les stimuli ignorés et l'état des portes sont recréés ; avec la riposte de l'outil, le joueur bat les bullies, ouvre les deux portes du garage, bat les vagues et frappe Davis sur ses tonneaux jusqu'à `CutsceneDavisBegs` et `MissionSucceed`. Le 05.11, **`1_04` se termine réussie** (81,05 s) : les armes (leurs fiches ObjectButes, le pool commun de l'inventaire, l'arme en main prise par `Weapons/SelectActions` et ses pistes), les projectiles (`WeaponFire`, sa vitesse balistique, la bille et la brique), les objets cassables des entrées spec des `.ipb` avec leurs événements par haché, les arbres des objets diffusés depuis `Stream/World.img`, les gestionnaires globaux et les ramassages sont recréés ; le joueur prend le lance-pierre, casse trois vitres du bus, grimpe à l'arbre et touche les six jocks pendant leurs sauts. `1_03` peut maintenant finir aux briques lancées de loin sur Davis (148,04 s), et `1_02` prend son vrai chemin, plus celui d'un bouchon (132,92 s). La nuit du 05 au 06.11, **`1_05` (« Save Algie ») va jusqu'à sa troisième étape** : les rondes des préfets (le gestionnaire des trajets de patrouille : la mission bouclait sur `AreaDisablePatrolPath`), `PedIsHit`, le groupe des alliés (`PedRecruitAlly`, `PedDismissAlly`, huit conditions), la piste `TetherLoco` qui fait suivre Algie, les alliés qui passent les portes avec le joueur, `PedSetMissionCritical`, `PedFindInAreaXYZ` et quatre fonctions de piéton et de parole sont recréés ; le joueur bat le premier bully et la bande de droite, entre à l'école avec Algie, le mène à la cabine des toilettes du second étage, bat l'embuscade, et Algie, lavé, le suit vers son casier. Le 06.11 (séance 2), **`1_05` se termine réussie** (185,20 s) : les objets animés des déclencheurs (le casier : sa fiche PropButes de `PropGlbl.cat`, son propre contrôleur d'actions, `PAnimSetActionNode`, ses drapeaux), `TargetProp` et le relevé des objets proches d'un piéton, les pistes `Sync` et `PropInteract`, l'écran de fin, les objectifs de mission et un `PlayOnTarget` qui attend sa cible sont recréés : Algie fouille son casier, reprend ses livres et paie le joueur. `1_06_01` démarre par le nœud `Run` de sa mission et attend le joueur chez le clochard, et les portes animées passent désormais par leur objet. Le 06.11 (séance 3), **`1_06_01` (« Get Help ») se termine réussie** (61,81 s) : le gestionnaire des collectibles (`colctbls.dat`), les objets du joueur, le pool des ramassages, les collectibles des placements `.ipb` changés en ramassages par la population, la prise au contact et l'historique des boutons des leçons de combat avec la piste `ActionRegisterButtonHistory` sont recréés : le joueur prend le transistor sur les hauteurs, le donne au clochard et réussit la leçon du coup chargé. `1_07` (« Defend Bucky ») démarre et passe `PAnimRequest`. À la séance 4 du 06.11, **`1_07` va jusqu'à la porte du garage** : les objets animés créés par les scripts (`PAnimCreate`), `PedFindInAreaObject`, les entités persistantes et les marqueurs du radar sont recréés ; le joueur rejoint Bucky, bat les quatre bullies et soulève la porte du garage. À la séance 5 du 06.11, **`1_07` se termine réussie** (151,47 s) : `PropTargetInteractive` (Bucky se sert de la caisse à outils), les ramassages laissés par les piétons, la cible par genre de `FaceTarget` et `SeekEntityPosition`, la barre de santé secondaire, `PAnimDelete` et sept fonctions de piéton et de joueur sont recréés ; le joueur bat la seconde vague, et la mission joue le clochard, le don de la planche et l'écran de fin. À la séance 6 du 06.11, la suite des armes commence : la charge de l'arme (`WeaponSetCharge`, `WeaponIncrementCharge`, lue par `WeaponFire`), les drapeaux des armes (`SetWeaponFlags`, `WeaponFlags`, `PedSetWeaponFlag`) et la minuterie de mission (`MissionTimerStartFixed`, `MissionTimerPause`) sont recréés. À la séance 7 du 06.11 et à la séance 1 du 07.11, elle continue : le pas du contrôleur d'actions, la manette qui tient un bouton (`--tenir`, la feuille `Charge` du lance-pierre jouée pour de vrai dans `1_04`), les réglages et les dégâts du tir, l'objet arme, `AttackTimerReset`, `HasRangedWeapon`, `DetectProjectile` et `DetectGrenade`, et le haut du corps du joueur en contrôleurs à part (le bouton de tir tenu seul fait partir la première bille de `1_04`). À la séance 2 du 07.11, les entités qui suivent un trajet sont recréées (le PathReference, le gestionnaire 0xd13730, `PAnimFollowPath` et ses quatre voisines, le rappel des nœuds atteints), avec `IsPlayerFatigued`, l'avance sur une cible qui bouge et la table des 32 projectiles avec leur vie. À la séance 3 du 07.11, `AreaClearAllPeds` est branchée (les commandes des missions refaites avec elle), avec `PedClearHitRecord`, `PedSetMinEngage` et la condition `EngageDistance`, la visée automatique du joueur (`WeaponPlayerFireAutoTarget`) et les décalages de visée de `WeaponFire`. À la séance 1 du 09.11, la visée du joueur est refaite en entier (`FUN_00662a40`) : les destinations forcées du tir (`PedSetProjDest`), la source de la cible (l'octet +0x31 de la piste, le composant de visée et son repli), le bouton 10 de la manette, le point devant le tireur sans cible, l'imprécision sur une cible qui n'est pas un piéton et `PedTargetPAnim` ; avec elles la vie des projectiles (`PedSetProjectileLifetime`), leur test d'eau et les pistes spawn qui meurent avec leur contrôleur vide. À la séance 1 du 11.11, le contrôleur d'actions se vide à la fin d'un nœud sans suite (`FUN_005f49a0`) et la durée d'un nœud ne vient plus que des pistes qui le retiennent (`FUN_005f4230`) : le haut du corps du lance-pierre repart au bouton suivant et `1_04` casse ses trois vitres au seul bouton 8 (17,56 s, 19,54 s et 21,55 s) ; `1_05` finit à 180,31 s et `1_07` à 150,88 s. Avec eux ce qui arrive à un projectile au contact : le choc (`FUN_004d3860`) et son explosion, le contact (`FUN_0046ad90`), la table des explosions, la fin d'un projectile (`FUN_004d45e0`) et les marques d'impact (`FUN_00560f70`). À la séance 1 du 12.11, la physique d'un projectile dans le monde (`CVolProjectile`) : le vol sous la gravité (`FUN_004d2160`, `FUN_00469760` ; +0x124 est en mètres par pas), l'air (`FUN_004698c0`), le repos (`FUN_00469a20`), le rebond (0,75 contre un piéton, l'élasticité +0x164 sinon), la paire du contact (`FUN_0046b720`), le contrôle de chaque image (`FUN_004d2290`), la poussée (`FUN_004d1ec0`), le freinage (`FUN_004cf350`), la fléchette qui se plante et le frisbee dévié ; l'outil ne retire plus un projectile à son contact et `1_04` écrit ses rebonds sur les trois vitres (17,52 s, 19,50 s et 21,52 s). L'outil des missions n'a toujours pas de sol sous ses projectiles : le repos et le freinage ne se voient que dans les tests.
L'outil joue le joueur (`--entrer`, `--aller`, `--zone`, `--porte`, `--jouer`, `--demander`, `--sequence`) ; tout le reste répond comme le
moteur, ou dit qu'il est un bouchon. Voir `docs/lua.md`, `docs/missions.md`, `docs/zones.md`,
`docs/cinematiques.md`.

## Ce qui est recréé et vérifié sur les vrais fichiers

Chaque chargeur de données a un test hôte (`tests/construire_tests.sh`, puis
`BULLY_DATA=<racine du jeu> build/tests/test_…`) qui le compare aux fichiers
du jeu ; les sept tests qui ne lisent aucun fichier du jeu (`tests/sans_jeu.sh`)
tournent sur GitHub à chaque push et pull request (`.github/workflows/tests.yml`),
après la compilation de toutes les sources et de tous les tests :

- `Config/dat/*.dat`, `handling.cfg` complet avec la boîte de vitesses et les
  conversions en unités du jeu ([src/vehicles](src/vehicles)) ;
- les définitions de modèles binaires `.idb` de `Objects/ide.img`, treize
  sections, 77 fichiers ([docs/idb.md](docs/idb.md)) ;
- les archives `.img` / `.dir` ([src/core/CdStream.h](src/core/CdStream.h)) ;
- les collisions COL3 / COL2 / COLL de `Stream/World.img`, 488 fichiers et
  3 863 modèles à l'octet près ([docs/collision.md](docs/collision.md)) ;
- les placements binaires « Ipl$ », 85 fichiers, onze sections
  ([src/core/IplFile.h](src/core/IplFile.h)) ;
- les modèles NIF de Gamebryo 2.3, 5 724 fichiers, géométrie, matériaux et
  textures décodés ([docs/nif.md](docs/nif.md)) ;
- les textures `.nft`, 4 469 fichiers, les 35 635 images décodées en DXT1,
  DXT5, RGB, RGBA et palette ([docs/nft.md](docs/nft.md)) ;
- un premier export visible : `outils/nif2obj` sort n'importe quel modèle en
  OBJ avec ses textures, et `outils/rendu` le dessine avec un rasteriseur
  logiciel sans dépendance ;
- le sens des rotations des nœuds NIF, tranché contre 3 842 boîtes de
  collision ([docs/nif.md](docs/nif.md)) ;
- une scène entière depuis les placements `Ipl$` : `outils/scene` rend une
  zone du jeu (bâtiment, dortoir) avec le sens du quaternion établi par les
  modèles eux-mêmes ([docs/ipl.md](docs/ipl.md)) ;
- une première visite en temps réel : `outils/visite` promène une caméra à la
  première personne dans une zone, rendu logiciel en 400 × 240 (l'écran du
  haut de la New 3DS), perspective correcte, découpage au plan proche et
  modes de transparence du jeu ; SDL2 n'ouvre que la fenêtre
  ([docs/rendu-temps-reel.md](docs/rendu-temps-reel.md)) ; F passe en marche,
  sur les volumes de collision du jeu (sol, marches, murs) ; dehors, les
  scènes voisines se chargent et se libèrent pendant qu'on avance
  (`outils/carte` dresse la carte des 85 fichiers de placements) : on passe
  de l'école au quartier d'affaires ; `--pietons n` pose des piétons du jeu
  déformés par leur squelette et **animés** par les animations du jeu (le
  format des `.agr` est déchiffré, [docs/agr.md](docs/agr.md)) : ils
  **marchent** dans le décor, sur le sol et contre les murs ;
- la synchronisation labiale `.lip`, 493 fichiers et 40 882 répliques,
  2 bits par image lus comme dans `bully.exe` ([docs/lip.md](docs/lip.md)) ;
- la numérotation du streaming ([docs/streaming.md](docs/streaming.md)) et
  les pools ([docs/pools.md](docs/pools.md)) ;
- la grille du monde, ses listes et ses dix pools d'entités
  ([docs/world.md](docs/world.md)).

Le plus gros écart avec GTA est là : un nœud de liste tient sur **un seul mot
de 32 bits** (4 bits de pool, 14 bits d'index, 14 bits pour le nœud suivant),
là où re3 en utilise trois pointeurs. Un secteur stocke donc des poignées et
non des pointeurs, et loge ses cinq listes dans 20 octets.

## Méthode

1. Cartographier : RTTI, tables virtuelles, formats de fichiers, appels de scripts Lua.
2. Recréer par sous-système, en commençant par ce qui se teste sans le rendu :
   chargeurs de données, `CPlaceable` / `CEntity`, véhicules.
3. Vérifier chaque fonction recréée contre l'original (mêmes entrées, mêmes sorties).
4. Gamebryo est recréé au fur et à mesure, uniquement ce que le jeu utilise.

## Outils

- `tools/ghidra/` : scripts headless (inventaire, RTTI, décompilation par table virtuelle).
- `tools/generer_squelettes.py` : produit `docs/classes.md` et `src/squelettes/`.
- Analyse sous Ghidra en conteneur (`blacktop/ghidra`), projet local hors dépôt.

## Ce qui vient après

Voir [docs/suite.md](docs/suite.md) : le premier point ouvert est le prochain chantier.

## Journal

Voir [docs/journal.md](docs/journal.md).
