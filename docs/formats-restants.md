# Formats restants de World.img et Scripts.img

Relevés rapides sur les données (22.09.2026), à approfondir.
Le `.nft` a quitté cette liste le 24.09.2026 : c'est un NIF de textures, voir [nft.md](nft.md).

| Extension | Nombre | Ce que c'est |
|---|---|---|
| `.lur` | 52 dans World.img, 515 dans Scripts/Scripts.img | Bytecode Lua **5.0** (`\x1bLuaP`, version 0x50, petit-boutiste, int 4, size_t 4, Instruction 4, SIZE_OP 6, SIZE_A 8, SIZE_B 9, SIZE_C 9, nombre 8 octets). Le streaming les nomme `%s.LUC`. `Scripts/Scripts.img` (+ `.dir`, même format que World) contient les 515 scripts de mission, tous en `.lur`. |
| `.agr` | 550 | Groupes d'animation binaires : dword version 0x100, puis des comptes (0x3ea, 0x2dc…) et des données compactées. Chargeur à trouver (streaming ids 0x58ac..0x5af0, `%s.AGR`). |
| `.cat` | 119 | Catalogues : quatre dwords d'en-tête puis des paires (id, décalage) ; liés aux objets animés (AniBroom, AniDice…). |
| `.lip` | 493 | Synchronisation labiale (`lipfile_%03d.lip`), suite de trames de 12 octets environ. |
| `.hxd` | dossier `Anim/` | Animations hors streaming (ANIBBALL.HXD…), format à lire. |

Bully utilise donc Lua 5.0 (et non 5.1) : la VM à embarquer devra être celle-là,
l'API de 914 noms de `docs/api-lua.txt` s'y branche.
