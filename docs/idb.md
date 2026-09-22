# Les définitions de modèles binaires (.idb)

Bully ne lit plus les IDE texte de GTA au démarrage : ils sont compilés en
`.idb` et emballés dans `Objects/ide.img` (77 fichiers, `default.idb` en
tête, 35 secteurs). Les IDE texte restent dans `Objects/` comme source, avec
leurs en-têtes de colonnes. `CFileLoader::LoadImgIde` (0x42caf0) lit l'archive
entière en mémoire, puis `LoadIdeBinary` (0x42c970) parcourt chaque fichier.

Format d'un `.idb` : un dword de taille utile, puis des sections
`[tag][données][premier id][dernier id]`. Le tag est le nom de section GTA
lu comme un entier petit-boutiste (`peds` = 0x70656473, donc les octets
« sdep » dans le fichier). Les chaînes sont alignées sur 4 octets : on lit
des dwords tant que l'octet haut du dernier n'est pas nul. Après chaque
section, `RegisterModelRange` (0x52dc50) enregistre [premier, dernier] si
le premier n'est pas -1.

| Tag | Chargeur | État |
|---|---|---|
| peds | 0x42bfd0 | recréé (`LoadPeds`), 289 entrées de `default.idb` conformes au texte |
| objs | 0x42aa20 | recréé (`LoadObjs`), 3 351 entrées sur 76 fichiers, toutes de type 0 |
| tobj | 0x42af80 | recréé (`LoadTobj`) : objs + heures d'allumage et d'extinction, 160 entrées |
| cars | 0x42a160 | recréé (`LoadCars`), 27 entrées, bornes vélos/véhicules |
| weap | 0x429ee0 | recréé (`LoadWeap`), 146 entrées |
| item, accs, clth | 0x42b400, 0x42a080, 0x42a6a0 | recréés : id, modèle, txd, bornes d'ids |
| cash, scnd | 0x42b510 | recréés : id, modèle, txd |
| panm | 0x42a4d0 | recréé (`LoadPanm`), 237 entrées de `props.idb` |
| 2dfx | 0x42b610 | recréé (`Load2dfx`), 651 entrées sur 75 fichiers |
| path | — | un dword sauté |

`test_ide` lit les 77 fichiers de `Objects/ide.img` en entier et vérifie
`default.idb`, `ifunhous.idb`, `iboxing.idb`, `props.idb` et `access.idb`
contre leurs IDE texte (`Objects/*.ide`, `Interior/`, `Prop/`, `Terrain/` :
un texte par `.idb`).

Entrée `peds` : id, modèle, txd, female, taille, type, stat, quatre groupes
d'animation, unique, racine et fichier de l'arbre d'actions, racine et
fichier de l'arbre d'IA, nom. Le binaire repère au passage les slots
`spfirst` / `splast` (0xa136d0 / 0xa136d4) et crée le modelinfo
(0x51c810, ou 0x51b210 si le slot est déjà un piéton).

Ordre des sections dans `default.idb` : peds (259), cars (27), weap (146),
cash (2), item (67), scnd (6), objs (6), peds (30), clth (1). Les 75
fichiers monde commencent par objs ; `access.idb` par accs.

Entrée `objs` : type (0..5), id, modèle, txd, nombre d'objets, autant de
distances de dessin que type/2 + 1, flags, trois dwords que le jeu ne lit
pas (un float souvent nul, 1.0, 0), un dword → octet +0xb du modelinfo
(255 partout), puis pour les types pairs trois dwords → octets +0x2d, +0x2e,
+0x2f. Toutes les entrées des données sont de type 0. Le chargeur crée un
`CSimpleModelInfo` (0x51c5f0, 0x34 octets), garde la première distance
(+0x24), résout le txd (0x50e7b0 → +0x16), convertit les flags IDE en bits
du modelinfo (0x429d30, recréé dans `ConvertFlags`), pose 0x1000000 pour
les noms `nog_` / `walkable_`, 0x100 pour les ids des sept plages de
0x429e70, et inscrit l'id dans la table des noms spéciaux (« _start_ »… à
0xa136e8) via 0x43f220.

Flags IDE rencontrés dans les données (3 297 entrées) : 0 (1 043), 0x80
(447), 0x4 (409), 0x2000 (330), 0xc (150), 0x84 (128), 0x1000 (120),
0x1080 (107), 0x22000 (90), 0x20004 (80), 0x20000 (50). Distances les plus
fréquentes : 30, 100, 40, 20, 50, 15, 25, 60.

Entrée `tobj` : comme objs, puis deux dwords (heure d'allumage, heure
d'extinction) → +0x34 / +0x38 du `CTimeModelInfo` (0x51c650).

Entrée `cars` : id, modèle, txd, type (`car` / `bike`), handling, nom de
jeu (les `_` deviennent des espaces), deux groupes d'animation, classe, puis
fréquence, niveau (non lu), règles de composants, id de roue, échelle de
roue. Le `CVehicleModelInfo` vient d'une réserve statique de 32 × 0x1e0
octets (0xc771e4).

Entrée `weap` : id, modèle, txd, deux groupes d'animation, un entier non
lu, distance de dessin, deux octets. Réserve de 150 × 0x58 octets (0xc735e4).

Entrée `panm` : id, dff, txd, AGR, AGR de piéton, test alpha, collision
secondaire, verrouillage manuel de cible.

Entrée `2dfx` : id du modèle, position, couleur RVBA, type (0 partout), deux
noms de textures du txd `particle`, distance, portée, taille, taille
d'ombre, puis neuf nombres rangés à des offsets épars de l'effet (voir
`C2dEffectIdeEntry`). L'effet fait 0x40 octets et s'enchaîne au modèle par
indice (0x50ea20 : l'effet reçoit l'ancien +0x10 du modèle, le modèle
reçoit l'indice du nouvel effet).
