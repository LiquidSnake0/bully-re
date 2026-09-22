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
| peds | 0x42bfd0 | recréé (`CIdeBinary::LoadPeds`), vérifié : 259 entrées de `default.idb`, `player` et `DOgirl_Zoe_EG` conformes au texte |
| objs | 0x42aa20 | recréé (`CIdeBinary::LoadObjs`), vérifié : 6 entrées de `default.idb` conformes au texte, 122 de `ifunhous.idb` ; disposition validée sur les 3 297 entrées des 77 fichiers |
| tobj, weap, cars, item, accs, 2dfx, panm, clth | 0x42af80, 0x429ee0, 0x42a160, 0x42b400, 0x42a080, 0x42b610, 0x42a4d0, 0x42a6a0 | à lire |
| cash, scnd | 0x42b510 (même fonction) | à lire |
| path | un dword sauté | recréé |

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
