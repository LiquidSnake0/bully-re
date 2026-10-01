# Les animations `.agr`

550 groupes d'animations dans `Stream/World.img` (identifiants de streaming
0x58ac à 0x5af0, nommés `%s.AGR` par 0x52e990). Format établi le 30.09.2026,
à partir du code de `bully.exe` décompilé et vérifié sur tous les fichiers.

## Un groupe, des animations

Un `.agr` est une suite d'animations mises bout à bout :

| taille | contenu |
|---|---|
| u32 | 0x100 |
| u32 | type, l'encodage des images clés : 999 à 1004 |
| u32 | nombre d'images clés |
| u32 | 0 |
| f32 | durée en secondes |
| n × taille | les images clés |
| 8 × m | les positions, jusqu'à l'animation suivante |

Taille d'une image clé selon le type, lue dans 0x6b2ca0 : 999 → 32 octets,
1000 → 20, 1001 → 12, **1002 → 8**, 1003 → 20, 1004 → 12. Chaque type a son
décodeur (0x6b8f40 aiguille : 1001 → 0x6b70e0, 1002 → 0x6b7540, 1000 →
0x6b7aa0, 1003 → 0x6b7f50, 1004 → 0x6b83b0, 999 → 0x6b8940).

Relevé : **3 446 animations dans les 550 fichiers, toutes décodées**, dont
2 772 de type 1002, 239 de 1003, 177 de 1001, 143 de 999, 72 de 1004, 43 de
1000. Toutes les attentes des piétons (`IDLE_GSF_A`, `IDLE_JOCK_A`…) et leurs
déplacements sont en 1002 ; les objets (portes, cloches, drapeaux, vélo) se
répartissent dans les six.

## Les six encodages (établis le 01.10.2026)

Ils vont par paires : le même quaternion, avec ou sans translation dans la
clé. Les types sans translation ont des enregistrements de position après les
clés (format plus bas) ; les types avec translation n'en ont **jamais**.

| type | clé | précédente | instant | quaternion | translation | décodeurs |
|---|---|---|---|---|---|---|
| 1000 | 20 o | u16 +0 | u16 +2 | 4 × f32 +4, ordre w x y z | positions à part | 0x6b5440 |
| 999 | 32 o | u16 +0 | u16 +2 | 4 × f32 +4, ordre w x y z | 3 × f32 +20, mètres | 0x6b5440 |
| 1001 | 12 o | u16 +0 | u16 +2 | 4 × i16 +4 en 32767e, ordre **x y z w** | positions à part | 0x6b4970 |
| 1003 | 20 o | u16 +0 | u16 +2 | comme 1001 | 3 × i16 +12, millimètres (+18 bourrage) | 0x6b4970, 0x6b1830 |
| 1002 | 8 o | w0 bits 0-10 | w0 bits 11-19, 511e | compressé (ci-dessous) | positions à part | 0x6b1710 |
| 1004 | 12 o | comme 1002 | comme 1002 | comme 1002 | u32 +8 compressé, centimètres | 0x6b1710, 0x6b1870 |

L'instant u16 est en 65 535e de la durée (constante 0x941f28 = 1/65 535), les
composantes i16 en 32 767e (0x941f30). La translation du 1004 tient en un mot :
x = bits 0-9 (signe bit 10), y = bits 11-20 (signe bit 21), z = bits 22-30
(9 bits, signe bit 31), le tout × 0,01 (0x900d30) : ±10,23 m au centimètre. Le
bassin de `1_02_MeetWithGary` y tombe à 0,87 m, la hauteur de bassin des
piétons en 1002. Les 1 732 400 images clés des six types ont une norme de 1,
sauf les 624 clés nulles du 1002 (plus bas).

**Le nombre d'os n'est pas fixe** : 36 pour un piéton, de 1 (`DartBrd`) à 33
(`Slingsh`) pour un objet, et le même dans tous les encodages d'un même objet
(2 pour `ANIBBALL`, 15 pour `Bike`, 26 pour `SIAMESE`). Les têtes de piste
ouvrent l'animation : précédente 0 **et** instant nul. Le champ seul ne suffit
pas, « précédente 0 » veut aussi dire « la clé 0 » : la deuxième clé de l'os 0
porte le même champ, mais un instant non nul.

Les décodeurs de lecture du moteur (0x6b70e0 pour 1001, 0x6b7aa0 pour 1000,
0x6b7f50 pour 1003, 0x6b83b0 pour 1004, 0x6b8940 pour 999) lisent tous la
clé 35, ARROW, et en tirent un angle de cap par la conversion
quaternion → axe et angle (ramené dans [0, 360[).

**Piège de lecture** : les secteurs sont complétés de zéros, mais une clé 1003
finit par deux octets de bourrage nuls, souvent après une translation nulle.
Retirer les zéros de fin avant de découper tronquait la dernière animation de
74 fichiers : on ne les retire que pour chercher la fin des positions.

## Le type 1002

Une image clé tient en 8 octets, deux mots w0 et w1, décompressés par
0x6b1710 :

| bits | contenu |
|---|---|
| w0 0-10 | index de l'image clé **précédente** du même os (0 : aucune) |
| w0 11-19 | instant, en 511e de la durée (constante 0x941f38 = 1/511) |
| w0 20 | signe de x |
| w0 21-30 | \|x\| en 1023e (constante 0x941f40 = 1/1023) |
| w0 31 | signe de y |
| w1 0-9 | \|y\| |
| w1 10 | signe de z |
| w1 11-20 | \|z\| |
| w1 21 | signe de w |
| w1 22-31 | \|w\| |

Soit un quaternion (w, x, y, z) sur 44 bits. Les images clés de tous les os
sont entrelacées dans l'ordre du temps, chacune pointant la précédente de son
os : un format pensé pour le streaming, qu'on lit d'un seul passage. Les 36
premières sont la pose de départ des 36 os. Sur les 2 749 animations
décodées, toutes les images clés ont une norme de 1 à 0,0015 près, sauf ~500
de norme nulle, sur les bras et les mains de 63 animations : une absence de
donnée pour cet os, qu'on saute.

**Les 36 os** sont les nœuds du squelette sous `Dummy`, dans l'ordre du
fichier : Dummy, Root, Root Pelvis, Root L Thigh, Root L Calf, Root L Foot,
Root R Thigh, Root R Calf, Root R Foot, Root01, Root Spine, Root Spine1,
Root Spine2, Root Neck, Root Head, Root Ponytail1, Root EyeLids, Root Brow,
Root Eyes, Root L Clavicle, Root L UpperArm, Root L Forearm, Root L Hand,
Root L Finger0, Root L Finger1, Root L Finger11, Left_Shoulder,
Root R Clavicle… Right_Shoulder, ARROW. Établi en comparant la rotation de
départ de chaque piste à la rotation locale de chaque nœud de Jimmy : les os
immobiles de l'animation (bassin, colonne, visage, clavicules, doigts)
tombent sur leur nœud à 0,001 près (`tests/test_agr`). Les nœuds portent
aussi une étiquette `tag=N` dans leur `UserPropBuffer` (0 Root, 1 Pelvis, 2 à
7 colonne et tête, 31-37 bras gauche, 41-43 jambe gauche, 51-53 jambe
droite ; le visage porte `EyeLids`, `EyeBalls`).

**La rotation remplace celle du nœud** (sa translation reste celle du
fichier), dans la même convention que les NiAVObject : la matrice construite
de façon usuelle depuis le quaternion est celle que le nœud stocke ligne par
ligne.

**Les positions** : u16 index d'image clé, i16 x, y, z en millimètres.
L'image clé désignée donne l'instant **et l'os** : dans les animations de
piétons, ce sont deux pistes, celle de la clé 1 (`Root`, le bassin, autour de
z = 0,845 m) et celle de la clé 35 (`ARROW`, la flèche au sol). Le trajet de
la flèche est le déplacement du personnage : nul pour une attente, 1,48 m
vers +y en 1,067 s pour le pas de `F_Jocks` n°2. On applique au bassin son
écart au début de l'animation moins le trajet de la flèche, et c'est le
personnage entier qui avance de ce trajet.

## La marche

Le corps, dans le repère des animations, regarde vers +y : les pas de marche
avancent vers +y. Les groupes `S*_S` (`SGEN_S`…) contiennent aussi des
animations qui avancent, mais vers −y : ce sont des reculs et des esquives
(vérifié à l'image, bras levés). Les vraies marches sont dans les groupes
`F_*` que l'IDE donne à chaque piéton (`F_Girls`, `F_Jocks`, `F_Greas`,
`F_Preps`, `F_Nerds`…, colonne ANIMGROUP) : des cycles qui bouclent (la pose
de fin redonne celle du début à 0,0013 près), autour de 1,07 s pour deux pas,
de 1,1 à 1,7 m/s.

Code : `src/anim/Agr` ; test : `tests/test_agr` (Player_Tired, puis les 550 fichiers).

## Les objets animés (01.10.2026)

Un modèle animé est déclaré dans la section `panm` des `.idb` (props.ide) : id,
modèle, txd, **groupe .agr**, groupe de piéton. 237 modèles. Les pistes d'un
objet animent les nœuds **sous « Root »**, en profondeur dans l'ordre du
fichier, la piste 0 étant « Root » lui-même (un piéton part de « Dummy »).
Vérifié avec `outils/sonde_objets` : les têtes de piste retrouvent la
rotation et la translation de leur nœud à 0,001 près (boîte aux lettres,
dé, porte, les 19 os du rideau du carnaval, les 23 de l'armure ; seuls les os
symétriques de l'armure, de même pose, se confondent). Les translations des
pistes sont absolues, dans le repère du parent, comme celle du nœud.

Un groupe d'objet mêle des animations de décor et des animations
d'événement : dans celui de `PortaPoo`, la première qui bouge projette la
cabine à 13 m. La visite joue en boucle, parmi celles qui bougent, une qui
reste sur place (dérive des positions < 1 m), de préférence cyclique.
Les placements du monde ne posent que quelques objets animés (8 au carnaval :
deux `PortaPoo` qui tanguent, six figurants `CARNI0*` ; un garage à vélos à
l'école, un interrupteur à l'asile) : les portes et coffres viennent des
scripts.
