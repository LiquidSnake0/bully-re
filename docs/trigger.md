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

`Population.dat` déclare par zone (TRIGGER avec POPULATIONDATA) le nombre de
piétons par catégorie selon le moment : POPULATION_DAY, _CLASS, _NIGHT,
_CURFEW. Pas encore lu.

Code : `src/core/TriggerFile` ; test : `tests/test_trigger` ; dans la visite :
`--patrouilles n` pose un piéton (un préfet pour une ronde) sur les n trajets
d'ambiance les plus proches ; il va de point en point, s'arrête le temps
écrit, tourné selon l'action (lacet en degrés, 0 vers +y), en fondu marche ↔
attente.
