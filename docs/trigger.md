# Les déclencheurs et les trajets : `DAT/Trigger.img`

`DAT/Trigger.img` (+ `Trigger.dir`, même format que World.img) contient **410
fichiers `.dat` en texte**, un par mission ou par lieu : `1_02.dat`,
`Patrol_School.dat`, `Population.dat`, `MainMap_hangouts.dat`, `Janitors.dat`,
`ischool_doors.dat`… Chacun commence par un en-tête de comptes :

    NPATHS 4
    NPOINTS 0
    NPERIMETERS 0
    NTRIGGERS 0
    NPOIS 0

puis des blocs `PATH`, `POINT`, `PERIMETER`, `TRIGGER`, `POI` entre `BEGIN` et
`END`, une ligne « MOT valeurs » par champ, indentée de tabulations.

## Les trajets (PATH)

    PATH
    	BEGIN
    		PATHNAME "hallspatrol_1A"
    		AREACODE 2
    		PATROLTYPE 0
    		NPATHPOINTS 9
    		PATHPOINT -621.971985, -318.695007, 0.068833
    		POINTYAWPITCHROLL 0.000000, 0.000000, 0.000000
    		WAITTIME 0.000000
    		UNID 0
    		NACTIONS 1
    		ACTIONTYPE 0
    		ORIENTATION 165.000000, 0.000000, 0.000000
    		WAITTIME 2.000000
    		PATHPOINT …

`WAITTIME` vient deux fois : celui du point, puis celui de chaque action. Relevé
sur les 410 fichiers : **1 543 trajets, 8 913 points, 62 actions**, toutes de
type 0 (s'orienter et attendre). Onze mots-clés seulement dans les blocs PATH.
`PATROLTYPE` vaut presque toujours -1 (18 fois 0, 2 fois 7). Une incohérence
d'origine : `1_S01_Running_Prefect_Path` annonce 9 points et en écrit 8 ; le
lecteur garde les points présents et la compte.

Les trajets d'ambiance se reconnaissent à leur nom : les rondes des préfets
(`hallspatrol_1A`, `1F_Prefect01`, `2F_Prefect01` pour le premier et le
second étage), par opposition aux trajets de missions (`1_02B_…`, `3_S11_…`)
et de tests (`GLOBALTESTPATH2`).

**La section `pthx` des `.ipb` n'est pas ça** : il n'y en a qu'une dans tout le
jeu (`TINDUST_BAR_DOOR_CAM_PORT`, un point, un trajet de caméra). Ses points
sont trois triplets (0x434370 : trois rappels distincts), dont seul le premier,
une position, est non nul dans le seul exemple.

## Les listes de points (POINT)

Un bloc `POINT` est un endroit nommé d'une mission : là où le script pose le joueur
(`PlayerSetPosPoint`), crée un personnage (`PedCreatePoint`), met un marqueur ou une
caméra. `POINTNAME`, `AREACODE`, `NPOINTPOINTS`, puis chaque point : `POINTPOINT x, y, z`,
`POINTYAWPITCHROLL` (degrés), puis les champs de son, de ramassage et d'horaire. Relevé sur
les 410 fichiers : **4 188 listes, 6 783 points**, tous les en-têtes `NPOINTS` justes.
`1_01.dat` en a 28 : `1_01_Player_Start` (298.36, −72.23, 6.07, lacet 185),
`1_01_CheerPoints` (plusieurs points), `1_01_Secretary`…

**Ce que le moteur garde d'un bloc POINT** (`FUN_006d59a0`, le lecteur, lu dans l'index le
18.10.2026). L'entrée de liste (pool 0x20c7c54) reçoit le nom haché (+8), **l'`AREACODE` sur un
octet (+0xc)**, l'emplacement de `DATLoad` (+0xd), et deux drapeaux en +0x18 tirés des trois
premiers caractères du nom, tels quels : `SV_` (bit 0, un point de sauvegarde, 49 listes) et
`DT_` (bit 1, une porte, 114 listes ; `dt_smokers` et `dt_SmashCar` n'en sont pas). Chaque
point est une entrée de 0x2c octets du pool 0x20c7c88, chaînée à la suivante par +6 :

| Mot-clé | Dans l'entrée | Relevé sur les 6 783 points |
|---|---|---|
| `POINTPOINT`, `POINTYAWPITCHROLL` | position +0, lacet +0x1c (le tangage et le roulis ne sont pas gardés) | |
| `POINTLABEL` | hachage en +0x10 | 971 non vides (`Fireplace`, `Water`, `Sink`…) : des points d'effets |
| `POINTPICKUPNAME` | index du modèle en +0x20 (`FUN_0051c040`, par hachage dans la table des `CModelInfo`) | 408 objets à ramasser : 75 `Rubband`, 40 `GeekCard`, 30 `brick`… |
| `POINTSFXID`, `POINTSOUNDBANKID`, `POINTSFXVOLUME` | +0x14 / +0x16, volume en +0x25 | 667 sons |
| `STREAMFADEINTIME`, `STREAMFADEOUTTIME` | +0x28, +0x29 (le temps divisé par 128) | |
| `POINTSML` | +0x24, recodé : 0 → 2, 1 → 3, 2 → 5, 3 → 0, 4 → 7, −1 → 1 | 290 non nuls |
| `STARTHOUR`, `ENDHOUR` | +0x22, +0x23 | 235 horaires |
| `WEATHERMASK` | +0x27, une lettre par temps (`FUN_00593f20` : F, W, P, S, R, X → bits 0 à 5 ; vide ou `all` → les six) | 87 `PSF`, 15 `R`, 13 `X`, 10 `PSFRW` |
| `USEOCCLUSION` | bit 0 de +0x2a | 88 |
| `MISSIONNAME` | +0xc : l'index de la mission dans la banque (`FUN_006a9e70`), −1 si vide, −2 pour `NONE` ; un `!` devant pose le bit 2 de +0x2a, un `-` le bit 3 | 82 (`PumpkinPost` 27, `TombstonePost` 19, `Christmas` 14…) |
| `POINTMISSIONSPEC`, `POINTPERMANENT`, `POINTTAG`, `PICKUPBUTES` | lus (`PICKUPBUTES` par `FUN_006d5730`) | 1 permanent, 1 tag |

`src/core/TriggerFile` lit tous ces champs (`CTriggerPointPoint`, `MasqueMeteo`) et
`tests/test_trigger` vérifie `1_01_EndPoint` (zone 5, lacet −90, roulis −2, le reste vide) et
les comptes ci-dessus.

**L'octet +0xc est bien l'AREACODE**, et `PlayerSetPosPoint` (`FUN_005d2310`) en fait la zone
de destination du joueur : il lit `(char)entrée[+0xc]`, remplace −1 par la zone courante
(`DAT_00bd1008`), et passe position, lacet et zone à `FUN_00415730` ; `PlayerSetPosPointArea`
(`FUN_005d2410`) fait pareil avec la zone donnée par le script. Ce n'est donc pas la zone où le
joueur **est**, c'est celle où il **sera** s'il y est téléporté ; la différence avec la zone visible
(`AreaGetVisible`) est traitée dans docs/zones.md.

**Dans le moteur**, `DATLoad("1_01.DAT", 2)` verse le fichier dans quatre pools :
listes de points (0x20c7c54), points chaînés (0x20c7c88, premier à +4, suivant à +6),
trajets (0x20c7c50) et déclencheurs (0x20c7c48), sous un emplacement (le monde à 1,
portes et tags à 5, la mission à 2, d'après les appels des scripts) ; `DATInit()` suit,
`DATUnload("2")` libère l'emplacement à la fin de la mission. Les scripts retrouvent une
entrée par nom à travers trois tables posées par `CreateLuaState` : `POINTLIST._NOM`
(0x5db130 → 0x6d6270), `PATH._NOM` (0x5db220 → 0x5ed1b0), `TRIGGER._NOM` (0x5db0f0 →
0x6d78c0) ; le « _ » de tête est retiré, le nom haché (`HashString`), et la réponse est
l'index dans le pool (listes et trajets à partir de 1, déclencheurs de 0), ou −1.
`GetPointFromPointList(liste, n)` rend x, y, z, lacet, 0, 0 du n-ième point ;
`GetPointList(liste)` les six nombres de chaque point. Code : `src/core/DatPools` ;
test : `tests/test_datpools` ; dans `lancer_lua`, `DATLoad`, `DATUnload`, `DATInit`,
`POINTLIST`, `PATH`, `TRIGGER`, `GetPointFromPointList`, `GetPointList`,
`PlayerSetPosPoint` sont réels.

## Périmètres, déclencheurs et population (01.10.2026)

Un bloc `PERIMETER` (placement, ISDOOR, HEIGHT, DEPTH, puis des
PERIMETERPOINTX / PERIMETERPOINTY) est suivi de « `NTRIGGERS k` » et des k
blocs `TRIGGER` qui l'utilisent. Hors de l'en-tête, NTRIGGERS est donc un
compte par périmètre. **Les points du périmètre sont relatifs à la POSITION du
déclencheur** (et tournés de son YAW) : le polygone de `RichArea` va de -273
à +261 autour de (449, 351) ; lu en absolu, la zone ne contiendrait pas sa
propre position. La zone s'étend en hauteur de POSITION.z à + ZHEIGHT.
Relevé : 2 694 périmètres, 2 546 déclencheurs. Une incohérence d'origine :
`tschool_trees.dat` annonce un déclencheur et s'arrête après l'en-tête.

Un déclencheur peut porter `POPULATIONDATA` : pour chacun des quatre moments
(POPULATION_DAY, _CLASS, _NIGHT, _CURFEW), un TOTAL et le compte des douze
catégories de piétons (PREFECT, NERD, JOCK, DROPOUT, GREASER, PREPPY,
STUDENT, COP, TEACHER, TOWNPERSON, SHOPKEEP, BULLY, la colonne type de la
section peds) ; et `AMBIENTVEHICLEDATA` : TOTAL, CAR, BIKE, POLICECAR par
moment. `Population.dat` : 37 périmètres, 34 zones dont 27 peuplées ;
`RichArea` = 6 le jour (1 preppy, 5 citadins), 3 au couvre-feu. Les zones
s'emboîtent (la ville, puis une boutique) : la plus petite qui contient un
point fait foi. `DT_ComicShop` a sa position au bord de son polygone en L.

Dans la visite : `--population jour|classe|nuit|couvrefeu` prend la plus
petite zone peuplée sous la caméra et y pose, à moins de 20 m, le nombre de
piétons de chaque catégorie que le fichier écrit pour ce moment ; ils errent
(marche et attente en fondu) et font demi-tour au bord de la zone. Vérifié
dans le parc du quartier riche.

Code : `src/core/TriggerFile` ; test : `tests/test_trigger` ; dans la visite :
`--patrouilles n` pose un piéton (un préfet pour une ronde) sur les n trajets
d'ambiance les plus proches ; il va de point en point, s'arrête le temps
écrit, tourné selon l'action, en fondu marche ↔ attente. Les actions d'un
point passent l'une après l'autre, chacune son temps (3_05_2Patrol : 38° puis
−38°, un regard à gauche puis à droite), puis vient l'attente du point.

**L'orientation d'une action** : le piéton regarde vers son lacet (degrés,
0 = +x). Établi à la cafétéria (ischool.ipb) : la file de `CafPath1`
(y = −272,6, lacet 90) fait face au comptoir, vers +y ; la cantinière
d'`AMBLUNCHLADYPATH` regarde les élèves côté comptoir (lacet 270) et la
cuisine au fond (lacet 90). Vérifié à l'image après 14 et 24 s de marche.
Ce n'est pas la règle des points d'intérêt (lacet + 180°) : ORIENTATION et
YAWPITCHROLL ne s'écrivent pas dans le même repère.

## Points d'intérêt (POI)

Un bloc `POI` / `BEGIN` : `NAME`, `AREACODE`, `NPOIPOINTS`, puis chaque point,
qui commence par son `GENDER` (Both, Male, Female) : `NAME` (le comportement,
« F_ClassSmokers », souvent vide), `TYPE` (DEFAULT, Specific_Event, Hang_Out,
Sitting_Spot, Scenario, Couple, Wall, Spectator, Brawl…), `PEDTYPE` (DEFAULT
ou une catégorie de piéton), `MAX`, `POIPOINT x, y, z`, `YAWPITCHROLL` (en
degrés, 0 vers +y), les périodes où le point sert, `USERADIUS` / `RADIUS`,
`IGNOREPOPULATION`, `OVERRIDELIMIT` ; `END` ferme le bloc. Relevé : 338 blocs,
1 199 points, comptes NPOIS justes partout (eventsRichArea, eventsPoorArea,
eventsSchoolHalls, eventsDowntown, MainMap_hangouts, PedPoi…).

**Dans le moteur** (18.10.2026) : `DATLoad` verse un bloc `POI` dans deux pools. Le groupe
(pool 0x20c7c58, lu par `FUN_00447bd0`) garde son nom haché à +4, l'AREACODE à +0xc,
l'emplacement de `DATLoad` à +0xd et son premier point à +0x16 ; chaque point est un `CStaticPOI`
de 0xa4 octets du pool 0x20c7c84 (genre à +0x74, nom haché à +0x84), chaîné au suivant par la
table `DAT_00bc5d08` (`FUN_00447a40` donne le premier, `FUN_00447a80` le suivant). **`POI._NOM`**
(0x5db170) retire le premier caractère du nom, toujours, et appelle `FUN_00448e30` : les
groupes sont parcourus **du dernier au premier** ; un groupe de ce nom rend l'index de son
premier point, sinon le premier point de ce nom dans le groupe ; −1 si rien. Le handle est donc
l'index dans le pool des points, et un script peut nommer un groupe (`POI._1_04_NEMSITSPOT`,
`POI._PETEYPIER`, `POI._CNERD1`) ou un comportement (`F_ClassSmokers` dans le groupe
« Smoking »). `CDatPools::Poi`, `PoiPoint`, `PoiGroupe` le refont, `tests/test_datpools` le
vérifie sur `1_04.dat` et `eventsDowntown.dat`, et `POI` est réelle dans `lancer_lua`.

Les périodes sont celles de `Config/timeCycl.dat`, la classe coupée en deux :
MORNING, EARLYCLASS (= FIRST_CLASS), LUNCH, LATECLASS (= SECOND_CLASS),
AFTERNOON, EVENING ; le couvre-feu en paliers de fatigue SLIGHTLYTIRED,
TIRED, MORETIRED, TOOTIRED (les fonctions F_StartCurfew_* de STimeCycle.lur).
Les heures de ces paliers ne sont pas dans les données : la visite suppose
23 h, minuit, 1 h et 2 h (l'évanouissement de 2 h).

La clique d'un point : PEDTYPE, ou TYPE quand il nomme une catégorie. Les
blocs `trich_nerds`, `trich_greasers`… ont PEDTYPE DEFAULT : la visite déduit
la clique du nom, une déduction (le jeu peut la lire ailleurs).

Dans la visite : `--poi n` pose un piéton sur les n points actifs les plus
proches (à l'heure de `--heure`), de la clique et du genre demandés, tourné
selon le lacet ; il disparaît quand aucune période du point n'est en cours.
Les variantes à part de l'IDE (colonne unique = −1 : costumes d'Halloween,
sous-vêtements, Gary) ne servent qu'en dernier recours.

**Le lacet** : le piéton regarde vers lacet + 180° (0 = +x). Établi sur les
places assises : les quatre « Sitting » autour de (597, −90), lacets 320, 45,
140 et 230, regardent alors vers l'extérieur à 2-4° près, et le banc de
(530, −148), lacet 290, tourne le dos à son mur. (Le corps animé regarde vers
+y de son repère, l'attente comme la marche : le bassin de toutes ces
animations a le même cap.) La première lecture, « 0 vers +y », reposait sur
une aboyeuse du carnaval jugée à l'œil : elle était fausse. Les actions de
trajet suivent une autre règle (voir plus haut).

**L'animation selon le type** : les groupes et les numéros viennent
maintenant du jeu. Les noms sont ceux des `.HXD` (docs/hxd.md), et les
arbres d'actions disent qui s'en sert (`tools/cat.py --pistes`, docs/cat.md) :

| Type (blocs) | Groupe .agr | Animations (nom dans le jeu) | Arbre qui les joue |
|---|---|---|---|
| Sitting_Spot (« Sitting ») | Sitting_Boys | 2-6 : SIT_LAUGH / SIT_SMOKE / SIT_TALK_NPC1-3 `_BENCH` | Ambient/Sitting_Down/SitHigh |
| Wall (« Smoking ») | POI_Smoking | 3, 4 : SMK_WALL_SMKA / SMKB (2 SMK_WALL_LIGHT et 0 SMK_WALL_STUB en entrée et sortie) | Ambient/scripted/Wall_Smoke |
| F_ClassSmokers | POI_Smoking | 7 : SMK_STND_SMKB | missions 3_01, 3_05 |
| Spectator | NPC_Spectator | 0-2 : GEN_IMPRESSED03 / 01 / 02 | Ambient/Spectator |
| Hang_Out | Hang_Talking | 0-9 : TALK, LISTEN, AMB_TALKING, AMB_WEIGHTSHIFT (10 : REAC_GOIN_DWN, une réaction) | |
| Couple (« Kissing ») | NPC_Love | 5 KISS_LOOP_B pour le garçon, 6 KISS_LOOP_G pour la fille | |
| Brawl | Grap ; Gfight entre filles | 7 + 6 GRAP_IDLE_GV / _RCV, 25 + 24 GRAP_MOUNT_IDLE_GV / _RCV, 45 + 24 GRAP_MOUNT_HIT_F ; GFIGHT_CYC_GV + _RVC | |
| Harassment | NPC_AggroTaunt | 1 REAC_BRING_IT (0 SLIT_THROAT, 2 COMEON, 3 DONTMESS, 5 IM_HERE) | |
| le reste | IDLE_* | l'attente de la clique | |

**Entrée, boucle, sortie** : comme les arbres du jeu, la visite enchaîne les
étapes d'un programme, chacune jouée un nombre de fois tiré au hasard, la
suivante entrant en fondu 0,3 s avant la fin de la précédente (l'animation
sortante ne repart donc jamais en boucle) :

- fumer contre un mur : SMK_WALL_LIGHT, SMKA (1-2), SMKB (1-2), SMKA (1-2),
  SMK_WALL_STUB, et on rallume ;
- fumer debout : SMK_STND_LIGHT, SMK_STND_SMKB (3-6), SMK_STND_STUB ;
- le couple : KISS_START, KISS_LOOP (2-4), KISS_END, versions _B et _G de
  mêmes durées ; les deux ont le même hasard et restent synchrones.

Chaque piéton entre dans la boucle à un instant qui lui est propre. Vérifié
en suivant les étapes sur 45 s et à l'image : le couple de (535, −152)
s'enlace puis se sépare (KISS_END) d'un même geste. Pas d'attente entre deux
cycles : l'attente de la clique a le bassin relatif, et la mêler aux
animations au bassin absolu ferait sauter les filles, dont le squelette est
plus haut.

Les déductions faites à l'œil avant de lire les arbres tenaient. Deux
corrections : pour fumer, j'avais mélangé dans la boucle l'allumage et le
mégot écrasé (LIGHT, STUB) ; pour le couple, B et G désignent le garçon et la
fille, l'animation se choisit selon le genre du piéton. `Gfight` est le combat de
filles (GFIGHT_CYC_GV / _RVC, n° 1 + 2) : une bagarre dont la première tirée
est une fille le joue, avec une seconde fille, depuis la même origine comme
les prises de `Grap`, et un bassin debout de référence à 1,10 m (début des
GFIGHT_IN, fin des GFIGHT_OUT, comme les F_Girls) ; programme GFIGHT_IN_GV /
_RCV, CYC (3-6), OUT_GV / _RVC. Vérifié à l'image (`--poi-filles`, qui fait
passer les filles d'abord) : deux dropouts face à face, empoignées, au sol.

Autres poses relevées : Sitting_Boys 1 et 7-9 assis par terre jambes
tendues, 0 la transition debout → assis ; POI_Gen 0-1 assis bas, 8-14 en
tailleur par terre, 15 une marche, 2-3 et 16 debout.

Ces animations sont faites pour un squelette dont le bassin debout est à
0,86 m (les F_Girls : 1,10 m). Leur piste de bassin compte en absolu
(`Anime::bassinRef`), mise à l'échelle du modèle : repos de son Root / 0,86
(0,906 m pour un petit, 1,018 pour un adulte, 1,094 pour Beatrice : le
rapport à la longueur de cuisse vaut 2,2-2,3 partout). La marche et l'attente
gardent le bassin relatif à leur début. Un couple : NPC_Love 5 et 6 avancent
déjà le bassin de 0,30 et 0,50 m vers l'autre ; chacun recule d'autant (à son
échelle) pour finir à 35 cm.

Vérifié à l'image près de (530, −148) : Beatrice assise sur le banc, dos au
mur ; un couple enlacé face à face ; un fumeur dos au mur, mais à un mètre de
lui plutôt qu'appuyé. `--galerie groupe[:réf][@a-b]` aligne les animations
d'un groupe devant la caméra, une par piéton, pour les reconnaître ;
`outils/sonde_poi.cpp` (non construit) mesure durée, boucle, bassin et cap.
**Les prises (`Grap`, 59 animations)** se jouent à deux : les deux piétons
sont posés à la même origine, avec la même orientation, et ce sont les
décalages de bassin des deux animations qui les placent l'un par rapport à
l'autre (le tenu du n° 6 est 0,96 m devant, tourné à −116°). Les paires se
reconnaissent à leur durée commune ; identifiées à l'image avec
`visite --paire Grap:a,b` : 7 + 6, 25 + 24, 45 + 24 (et sans doute 43, 44, 46
+ 24, d'autres coups à califourchon). `Gfight` est calé sur un bassin debout
à 1,06-1,11 m comme les F_Girls : probablement le combat de filles, non
utilisé. Vérifié en centre-ville : les dropouts de « Brawls » (542, −60) au
sol l'un sur l'autre ; l'agresseur de « StudentHarassment » (537, −47,5) face
à un nerd.

Restent : Back_Alley et School_Grounds (à l'attente), les autres paires de
Grap (transitions, projections), et le décodage des arbres d'actions.

## La section `pois` des .ipb

C'est la forme compilée d'une partie de ces blocs. Les 846 points des .ipb
(30 fichiers zone_*) se retrouvent tous, valeur pour valeur, dans sept
fichiers de Trigger.img : eventsSchoolgrounds (209), eventsRichArea (158),
MainMap_hangouts (155), eventsDowntown (110), eventsPoorArea (96),
eventsIndustrialArea (90), PedPoi (28). Le groupe porte le nom du bloc et
son AREACODE ; chaque point ses quatre chaînes GENDER, NAME, TYPE, PEDTYPE,
puis 21 mots : MAX, POIPOINT, YAWPITCHROLL, les dix périodes (0 ou 1),
USERADIUS, RADIUS (flottant), IGNOREPOPULATION, OVERRIDELIMIT. Les périodes
sont rangées dans l'ordre alphabétique de leurs mots-clés (AFTERNOON,
MORETIRED, SLIGHTLYTIRED, TIRED, TOOTIRED, EARLYCLASS, EVENING, LATECLASS,
LUNCH, MORNING), ordre établi en rapprochant les 846 points de leurs
jumeaux du texte : chaque position vote pour un seul mot-clé, 846 fois sur 846.
Les intérieurs (eventsSchoolHalls, tschool_hangouts, dortoirs, magasins…) et
les missions n'existent qu'en texte. Trois TYPE valent « Specific_Event" »,
guillemet compris dans les octets (zone_indust3, zone_rich1, zone_rich11) :
le texte actuel ne l'a plus, les .ipb viennent d'une version antérieure.

Code : `src/core/IplFile` (CIplPois, CIplPoiPoint, kIplPoiPeriode) ; test :
`tests/test_ipl` ; sonde : `outils/sonde_pois.cpp`. La visite garde
Trigger.img, qui contient tout.
