# L'heure du jeu

**Les périodes de la journée sont dans `Config/timeCycl.dat`**, en clair et
commenté (début et fin en heures pleines, puis les couleurs de l'horloge à
l'écran) :

| période | heures | moment de population |
|---|---|---|
| MORNING | 7 – 9 | DAY |
| FIRST_CLASS | 9 – 12 | CLASS |
| LUNCH_TIME | 12 – 13 | DAY |
| SECOND_CLASS | 13 – 16 | CLASS |
| AFTERNOON | 16 – 21 | DAY |
| EVENING | 21 – 23 | NIGHT |
| BED_TIME | 23 – 7 | CURFEW |

Le reste vient des **scripts Lua** (Scripts/Scripts.img, bytecode Lua 5.0 compilé
avec des nombres float 32 bits ; `tools/lur.py` les désassemble, sa sortie
n'entre pas dans le dépôt) :

| fait | valeur | source |
|---|---|---|
| départ | 8 h 00 | SInitGl.lur : gGameStartHour = 8, gGameStartMinute = 0 |
| vitesse | 60 s de jeu par seconde réelle | main.lur : ClockSetTickRate(60, 30) (second argument non lu) |
| couvre-feu | 23 h → 7 h | STimeCycle.lur, F_CurfewDefaultRules (heure >= 23 ou heure < 7), comme BED_TIME |

Les scripts réagissent aussi à des heures plus fines, à l'intérieur des
périodes : le dortoir des garçons (Bdorm.lur) vide ses élèves à 8 h 40,
9 h 15, 11 h 30, 12 h 45, 13 h 15, 15 h 30, 15 h 45. Une première version de
l'horloge en avait tiré des classes 9 h – 11 h 30 / 13 h – 15 h 30 et une nuit
à 19 h ; `timeCycl.dat` l'a corrigée.

`Act/Globals.cat` déclare aussi des périodes nommées (`PER_MORN`, `PER_LUNCH`,
`PER_ANOON`, `PER_EVEN`, `PER_CURFEW`, `PER_TIRED`) reliées aux fonctions de
STimeCycle.lur ; le moteur garde une période ainsi (0x6a8a50) : nom à +0x18,
heure de début +0x24, minute +0x28, heure de fin +0x2c, minute +0x30. Format
`.cat` : docs/cat.md.

Code : `src/core/Horloge.h` (CHorloge : Avancer, Heure, Minute, Moment ;
ChargerPeriodes lit timeCycl.dat) ; test : `tests/test_horloge`. Dans la
visite : `--heure HH:MM`, `--population auto` (les piétons des quatre moments
posés d'avance, seuls ceux du moment courant existent ; la visite annonce
« il est 21:00 : passage de DAY à NIGHT »).
