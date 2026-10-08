# Formats restants de World.img et Scripts.img

Relevés rapides sur les données (22.09.2026), à approfondir.
Le `.nft` a quitté cette liste le 24.09.2026 : c'est un NIF de textures, voir [nft.md](nft.md).

| Extension | Nombre | Ce que c'est |
|---|---|---|
| `.lur` | 52 dans World.img, 515 dans Scripts/Scripts.img | Bytecode Lua **5.0** (`\x1bLuaP`, version 0x50, petit-boutiste, int 4, size_t 4, Instruction 4, SIZE_OP 6, SIZE_A 8, SIZE_B 9, SIZE_C 9, **nombre 4 octets : float**, test 0x4befaf3b ; corrigé le 08.10.2026, voir [lua.md](lua.md)). Le streaming les nomme `%s.LUC`. `Scripts/Scripts.img` (+ `.dir`, même format que World) contient les 515 scripts de mission, tous en `.lur`. |
| `.agr` | 550 | **Déchiffré le 30.09.2026** pour le type 1002 (80 % des animations) : voir [agr.md](agr.md). Restent les cinq autres encodages (objets animés). |
| `.cat` | 119 | Catalogues : quatre dwords d'en-tête puis des paires (id, décalage) ; liés aux objets animés (AniBroom, AniDice…). |
| `.lip` | 493 | **Déchiffré le 06.10.2026** : synchronisation labiale, 2 bits par image, voir [lip.md](lip.md). |
| `.hxd` | dossier `Anim/` | Animations hors streaming (ANIBBALL.HXD…), format à lire. |

Bully utilise Lua **5.0.2** (chaîne « Lua 5.0.2 » dans bully.exe), compilé avec des nombres
en float. La VM est embarquée dans `tiers/lua-5.0.2` et l'API complète (1 504 fonctions) est
dans `docs/api-lua-tables.tsv` : voir [lua.md](lua.md).
