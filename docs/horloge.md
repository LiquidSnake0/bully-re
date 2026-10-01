# L'heure du jeu

Bully règle sa journée dans ses **scripts Lua** (Scripts/Scripts.img, bytecode
Lua 5.0 compilé avec des nombres float 32 bits ; `tools/lur.py` les désassemble,
sa sortie n'entre pas dans le dépôt) et dans une base compilée,
`Act/Globals.cat`, qui déclare des périodes nommées (`PER_MORN`, `PER_LUNCH`,
`PER_ANOON`, `PER_EVEN`, `PER_CURFEW`, `PER_TIRED`…) reliées aux fonctions de
`STimeCycle.lur` (`F_StartMorning`, `F_StartLunch`, `F_StartEvening`…). Le
moteur garde une période ainsi (0x6a8a50, « période active ? ») : nom à
+0x18, heure de début +0x24, minute +0x28, heure de fin +0x2c, minute +0x30.
Le format de `Globals.cat` (attributs balisés sur 2 octets suivis de leur
valeur, table de chaînes à 0x6e16, table de références de 10 octets) n'est
pas encore décodé : les heures ci-dessous viennent des scripts.

| fait | valeur | source |
|---|---|---|
| départ | 8 h 00 | SInitGl.lur : gGameStartHour = 8, gGameStartMinute = 0 |
| vitesse | 60 s de jeu par seconde réelle | main.lur : ClockSetTickRate(60, 30) (second argument non lu) |
| couvre-feu | 23 h → 7 h | STimeCycle.lur, F_CurfewDefaultRules : heure >= 23 ou heure < 7 |
| classes | 9 h → 11 h 30, 13 h → 15 h 30 | Bdorm.lur (bascules à 8 h 40, 9 h 15, 11 h 30, 11 h 40, 12 h 45, 13 h 15, 15 h 30, 15 h 45), MainMap.lur (périodes d'apparition à 11 h 30 et 15 h 30) |
| nuit | 19 h → 23 h | **déduit** : le jour s'arrête après 18 h dans Bdorm.lur (fenêtre 8 h – 18 h) ; à confirmer par PER_EVEN |

Code : `src/core/Horloge.h` (CHorloge : Avancer, Heure, Minute, Moment) ;
test : `tests/test_horloge` (bornes comprises). Dans la visite : `--heure HH:MM`
et `--population auto` ; la population des quatre moments est posée d'avance,
seuls les piétons du moment courant sont dessinés et animés, et la visite
annonce chaque passage (« il est 19:00 : passage de DAY à NIGHT »). Vérifié
dans le parc du quartier riche : les promeneurs du jour disparaissent à 19 h.
