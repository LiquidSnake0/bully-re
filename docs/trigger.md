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
écrit, tourné selon l'action (lacet en degrés, 0 vers +y), en fondu marche ↔
attente.

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
une aboyeuse du carnaval jugée à l'œil : elle était fausse. L'orientation des
actions de trajet (`--patrouilles`) suit encore cette première lecture, non
revérifiée.

**L'animation selon le type** : les arbres d'actions (`Act/AI_POI.cat`, même
format binaire que Globals.cat) ne sont pas décodés. Les correspondances
viennent des noms de blocs et de la galerie :

| Type (blocs) | Groupe .agr | Animations |
|---|---|---|
| Sitting_Spot (« Sitting ») | Sitting_Boys | 2-6, assis sur un banc (bassin 0,60 m) |
| Wall (« Smoking », « Smokers ») | POI_Smoking | 0, 2-4, fume adossé (bassin reculé de 18 cm) |
| F_ClassSmokers | POI_Smoking | 5-8, fume debout |
| Spectator | NPC_Spectator | 0-2 |
| Hang_Out | Hang_Talking | les boucles (0-7, 9, 10) |
| Couple (« Kissing ») | NPC_Love | 5 et 6, deux piétons face à face |
| le reste | IDLE_* | l'attente de la clique |

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
Restent : la section `pois` des .ipb, les autres types (Brawl, Harassment,
Back_Alley…), et le décodage des arbres d'actions.
