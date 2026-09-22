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
| objs | 0x42aa20 | lu : un type 0..5 par entrée choisit le nombre de distances de dessin ; à recréer |
| tobj, weap, cars, item, accs, 2dfx, panm, clth | 0x42af80, 0x429ee0, 0x42a160, 0x42b400, 0x42a080, 0x42b610, 0x42a4d0, 0x42a6a0 | à lire |
| cash, scnd | 0x42b510 (même fonction) | à lire |
| path | un dword sauté | recréé |

Entrée `peds` : id, modèle, txd, female, taille, type, stat, quatre groupes
d'animation, unique, racine et fichier de l'arbre d'actions, racine et
fichier de l'arbre d'IA, nom. Le binaire repère au passage les slots
`spfirst` / `splast` (0xa136d0 / 0xa136d4) et crée le modelinfo
(0x51c810, ou 0x51b210 si le slot est déjà un piéton).
