# Table virtuelle de CPed et CPlayerPed

`CPlaceable (1) → CEntity (34) → CPhysical (42) → CPed (46) → CPlayerPed (47)`.
`CCivilianPed` a 46 slots comme `CPed`. reVC : `CPed` ajoute `SetMoveAnim`,
`Save`, `Load` ; Bully ajoute quatre virtuelles à `CPed` et une à `CPlayerPed`.
Seuls les slots redéfinis ou ajoutés sont listés ; les autres sont ceux de
[vtable-entity.md](vtable-entity.md).

| Slot | CPed | CPlayerPed | Nom | Confiance |
|---|---|---|---|---|
| 0 | 004731e0 ? | 004498f0 | destructeur | **sûr** |
| 1 | 0046de40 | hérité | `IsOfTypeId` | *probable* |
| 3 | 004731e0 | hérité | `Remove()` redéfini | *probable* |
| 8 | 004788d0 | hérité | `SetModelIndex(id, flags)` redéfini | *probable* |
| 12 | hérité (vide) | 0049bae0 | `ProcessControl()` du joueur, 928 octets, lit les touches (0x7386f0) | *probable* |
| 13 | 004839c0 | hérité | `ProcessCollision()` | *probable* |
| 14 | 0047b570 | hérité | `ProcessShift()` | *probable* |
| 15 | 0047b810 | hérité | 3765 octets, 19 paramètres : **pas** `Teleport`, le slot 15 de CEntity n'est pas celui de reVC | ? |
| 16 | 00474a80 | hérité | ? | ? |
| 17 | 0047b210 | hérité | `PreRender()` | *probable* |
| 18 | 00474820 ? | 0049a530 | `Render()` ; le joueur teste la caméra (0x4ce540) avant de rendre | *probable* |
| 19, 20 | 004741b0, 004771c0 | hérité | `SetupLighting` / `RemoveLighting` redéfinis, comme reVC | *probable* |
| 21 | 00474820 | hérité | `FlagToDestroyWhenNextProcessed()` : teste +0x1624 et +0x148c | *probable* |
| 22 | 0049a930 (joueur) | | `IsRenderable` ? du joueur : 0x4783d0 et mode caméra (0xc3ccf8) != 2 | ? |
| 23 | 0043a2d0 | hérité | implémentation de la virtuelle pure, partagée avec CVehicle | **sûr** |
| 24–29 | 00473e10… | hérité | boîte englobante, rayons : redéfinis pour le piéton | ? |
| 36–38, 41 | 00473200, 0047d510, vide, 0047ef50 | hérité | physique du piéton | ? |
| 42 | 00479110 | 0049ac50 | (uint8, ?) : sons et animations, 765 octets ; le joueur incrémente +0x1ed4 et gère un compteur global (0xbceb4b) | ? |
| 43 | 0046de90 | hérité | accesseur : retourne +0x1d2c | **sûr** (accesseur) |
| 44 | 00477530 | 0049a6a0 | `ProcessPunishmentDecay()` : décrémente +0x1d40 avec le temps (0xc1a9b4), parcourt le pool de piétons ; le joueur appelle la base puis plafonne +0x1d40 par le bas avec +0x1ebc | *probable* |
| 45 | 00477480 | hérité | `IncPunishmentPoints(int amount)` : ignore cinq modèles réservés, joue un son (0x458330, id 0x9a) pour le joueur, ajoute à +0x1d40 borné à 0, puis 0x4773d0 ; **`PlayerIncPunishmentPoints` appelle ce slot (+0xb4) sur le joueur** | **sûr** |
| 46 | | 0049a500 | vide | *probable* |

## Champs de CPed vus

| Décalage | Rôle |
|---|---|
| +0x1310 | état du piéton ; 0xd = état joueur contrôlé (testé partout avec `FindPlayerPed`, 0x4ce410) |
| +0x1d40 | `m_nPunishmentPoints`, les points de punition (API Lua `PedSetPunishmentPoints`, `PlayerIncPunishmentPoints`) |
| +0x1d48 | dernier tick de décroissance |
| +0x1ebc | `m_nMinPunishmentPoints` (API Lua `PlayerSetMinPunishmentPoints`) |
| +0x1d2c | accesseur slot 43 |

## Pools

`DAT_00c0f5f0` est `CPools::ms_pPedPool`, avec la disposition de re3 :
`{ entrées, drapeaux, taille, taille d'une entrée }` ; un drapeau à 0x80
marque une case libre. La taille d'entrée est stockée (re3 y met le
pointeur d'allocation), ce qui donnera `sizeof(CPed)` en lisant le pool.
Le temps courant en millisecondes est `0x00c1a9b4` (`CTimer::m_snTimeInMilliseconds`).

## Nommer par l'API Lua

Le binaire enregistre 914 fonctions Lua par leur nom (`docs/api-lua.txt`),
dans des tables `{ nom, fonction C }`. Le script `ExportLuaBinding.java`
remonte du nom à la fonction C puis à la méthode du moteur. Premier résultat :
`PedSetPunishmentPoints` → 0x5ccdf0 → `0x4773d0(points)`, la fonction que le
slot 45 appelle en dernier. La jauge +0x1d40 est donc les points de punition,
et le slot 45 `IncPunishmentPoints`. Confirmé ensuite par les liaisons du
joueur : `PlayerIncPunishmentPoints` (0x5d1cc0) appelle la virtuelle +0xb4 du
joueur courant (0xc1aea8), soit le slot 45 ; `PlayerGetPunishmentPoints` lit
+0x1d40 ; `PlayerSetMinPunishmentPoints` écrit +0x1ebc ;
`PlayerSetPunishmentPoints` appelle 0x4773d0. Le joueur courant est la
globale `0x00c1aea8`.
