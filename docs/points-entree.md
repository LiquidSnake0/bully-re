# Points d'entrée du moteur vus depuis l'API Lua

Fonctions du moteur les plus appelées par les liaisons Lua
(`docs/api-lua-map.tsv`), lues et nommées.

| Adresse | Appelée par | Nom | Ce qu'elle fait |
|---|---|---|---|
| 0x005ee6d0 | 35 actions de piéton (`PedAttack`, `PedFlee`, `PedEnterVehicle`…) | `GameMalloc(size)` | L'allocateur du jeu : choisit un tas selon un identifiant courant (0x5eec80 ; 0x28, 0x26, ou 8..0x16 pour les tas de script), protège par section critique, met la mémoire à zéro, suit le point haut en 0xd14164. Chaque action alloue un objectif. |
| 0x00471240 | 29 actions de piéton, les mêmes | `CPed::PushObjective(CObjective*)` | Pile d'objectifs du piéton : compteur à +0x58, entrées à +0x8 (au plus 20). Si le nouvel objectif est du même type que le courant (types 7 et 8 comparés sur un champ), il est relâché au lieu d'être empilé. Les objectifs ont un compteur de références en [3] et un destructeur virtuel en slot 0. |
| 0x005c2770 | `PedFaceObject`, `CameraFollowEntity`, `PlayerIsInAreaObject`… | `LuaGetEntity(handle, type, a, b)` | Traduit un handle Lua en entité : type 0 objet (table 0x20c5b68 ou 0x5c1310), 1 véhicule (0x44a430), 2 piéton (0x5c7220), 3 joueur (0xc1aea8). Pour un piéton, renvoie son véhicule (+0x1554) si demandé. |
| 0x006d5860 | `PedCreatePoint`, `PedMoveToPoint`, `VehicleSetPosPoint`… | `CPointPool::GetByHandle(h)` | Handle → entrée du pool de points 0x20c7c88 (index ushort à +4, table de correspondance 0xbc5d2c), disposition de pool re3. |
| 0x006d6300 | les mêmes | `CPointPool::GetSlot(i)` | Index → entrée du pool 0x20c7c54, si la case n'est pas libre (drapeau 0x80). |
| 0x00576d80 | `AreaPOICompareName`, `PedGetNameHashID`, portes, effets | `HashString(s)` | Hachage des noms (points d'intérêt, portes, effets, modèles). |
| 0x0073ad50 | 60 liaisons de tout type | à lire (14 octets) | Trop court et trop transversal pour être autre chose qu'un accès à un état global. |

Règle de nommage automatique corrigée : une fonction du moteur ne reçoit le
nom d'une liaison Lua que si deux liaisons au plus l'appellent. La première
version avait nommé 0x73ad50 d'après une seule de ses soixante appelantes.
