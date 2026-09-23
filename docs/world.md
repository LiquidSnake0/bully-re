# Le monde : secteurs, listes et poignées d'entités

`CWorld` de Bully est bien celui de GTA, mais **les listes ne sont pas celles de
re3/reVC**. C'est la principale surprise de ce sous-système, et elle touche tout
ce qui parcourt le monde.

## La grille

36 × 36 secteurs de 50 mètres, soit un monde de [-900, 900[ en x et en y. Les
constantes sont en `.rdata` : 50.0 et 18.0 en double (`0x900aa0`, `0x900a98`),
35.0 en float (`0x900a94`). La conversion n'est jamais une fonction, elle est
recopiée en ligne dans chaque fonction qui touche la grille :

    indice = _ftol(coord / 50.0 + 18.0), borné à [0, 35]

`0x85c6f0` est le `_ftol` du CRT, pas une fonction du moteur.

Le tableau des secteurs commence à `0xc1b178`. Ghidra nomme `0xc1b17c`, qui est
la deuxième liste du premier secteur, et c'est cette adresse que l'on retrouve
dans le code : `&DAT_00c1b17c + (x + y*0x24) * 5`. Le pas d'une rangée est
`0xb4` mots, soit 36 secteurs × 5 listes. Fin du tableau à `0xc216b8`, où
commence la liste des grosses bâtisses.

Chaque secteur tient donc en **20 octets** : cinq listes d'un mot chacune.

Ordre des listes, d'après le `switch` sur `m_type` de `CEntity::Add`
(`0x465d60`), où le pointeur de travail vise la liste 1 :

| `m_type` | décalage | liste |
|---|---|---|
| 1 building | −1 | 0 bâtiments |
| 4, 6, 7 objets | 0 | 1 objets |
| 2 véhicule | +1 | 2 véhicules |
| 3 piéton | +2 | 3 piétons |
| 5 dummy | +3 | 4 dummies |

`CPhysical::Add` (`0x469090`) n'a que les cas 2, 3 et 4/6/7 : un `CPhysical` de
type bâtiment ou dummy n'est inscrit dans aucune liste.

## Un nœud de liste tient sur 32 bits

Là où re3 a un `CPtrNode` de trois pointeurs, Bully a **un seul mot**. Les nœuds
viennent d'un pool dédié (`0x44dee0` demande 4 octets), dont la base des entrées
est dans la globale `0xc0f788`. Le mot est découpé ainsi :

| bits | contenu |
|---|---|
| 0..3 | pool de l'entité, 4 bits |
| 4..17 | index de l'entité dans ce pool, 14 bits |
| 18..31 | index du nœud suivant dans le pool de nœuds, `0x3fff` = fin de liste |

Une liste est donc juste un pointeur vers son premier nœud, et un secteur ne
stocke aucun pointeur d'entité : il stocke des **poignées**. Le chaînage lui-même
est un index, pas un pointeur, ce qui explique la base `0xc0f788`.

L'encodage se lit tel quel dans `CEntity::Add`, `CPhysical::Add`,
`CWorld::Add` et `CPhysical::AddToMovingList`, tous identiques :

    *noeud = (index | 0xffffc000) << 4 | pool & 0xf;          // suivant = 0x3fff
    if(tete == 0) *noeud |= 0xfffc0000;
    else          *noeud = (tete - 0xc0f788 >> 2) << 0x12 | *noeud & 0x3ffff;

`CPtrNode::SetItem` est `0x45d520`, `CPtrList::RemoveNode` est `0x44dde0` et la
libération du nœud `0x44def0`. La liste étant simplement chaînée, retirer un
nœud repart toujours de la tête pour trouver son prédécesseur.

## Les dix pools d'entités

`CPools::GetEntity` (`0x44a290`) résout une poignée : un `switch` sur dix pools,
chacun décrit par une structure dont le mot 0 est la base du tableau et le mot 3
la taille d'une entrée. L'adresse est donc `base + index * taille`.

`CPools::GetEntityPoolAndIndex` (`0x44c7e0`) fait l'inverse à partir de
`m_type` (`+0x108`, bits 0..2), l'index venant de `CPool::GetIndex`
(`0x44a5e0`, une simple division qui renvoie −1 si le pointeur ne tombe pas sur
une entrée) :

Les noms et les classes viennent de `docs/pools.md`, établis par RTTI ; c'est
la condition de sélection qui est nouvelle.

| pool | globale | classe | sélectionné quand |
|---|---|---|---|
| 0 | `ms_pPedPool` `0xc0f5f0` | `CPlayerPed` | type 3 |
| 1 | `ms_pVehiclePool` `0xc0f5f4` | `CAutomobile` | type 2 |
| 2 | `ms_pObjectPool` `0xc0f614` | `CObject` | type 4, `+0xc4` et `+0xec` nuls |
| 3 | `ms_pProjectilePool` `0xc0f618` | `CProjectile` | type 4, `+0xc4` non nul |
| 4 | `ms_pCutsceneObjectPool` `0xc0f61c` | `CCutsceneObject` | type 4, `+0xec` non nul |
| 5 | `ms_pDummyPool` `0xc0f600` | `CDummy` | type 5 |
| 6 | `ms_pPropAnimPool` `0xc0f608` | `CPropAnim` | type 6 |
| 7 | `ms_pBuildingPool` `0xc0f5f8` | `CBuilding` | type 1, slot 34 faux |
| 8 | `ms_pTreadablePool` `0xc0f5fc` | `CTreadable` | type 1, slot 34 vrai |
| 9 | `ms_pAccessoryPool` `0xc0f60c` | `CAccessory` | type 7 |

Le slot 34 qui sépare un bâtiment d'un « treadable » est le premier slot propre
à `CBuilding`, au-delà des 34 de `CEntity`. Dans re3 c'est `GetIsATreadable`,
et le pool n'a qu'une seule entrée ici.

Quatre bits pour le pool, donc seize pools adressables au maximum, et dix
utilisés. Le type 0 renvoie −1, aucune poignée.

Ce tableau relie enfin les 28 pools de `docs/pools.md` aux types d'entités :
seuls ces dix-là sont référençables depuis une liste du monde.

## Les grosses bâtisses

Une entité dont `+0x58` est non nul ne va pas dans la grille. Elle va dans une
liste unique, `0xc216b8`, avec exactement le même format de nœud.
`CWorld::Remove` (`0x45dc20`) la retrouve en parcourant la liste et en résolvant
chaque poignée jusqu'à retomber sur le bon pointeur.

`SortBIGBuildingsForSectorList` (`0x451460`) décroche puis remet en tête chaque
entité marquée, ce qui les regroupe au début de la liste du secteur.
`SortBIGBuildings` (`0x452930`) balaie les 36 × 36 secteurs et leurs cinq
listes ; c'est l'étape « Find big buildings » de `CGame::Initialise`.

## La liste des entités mobiles

`0xc1aea4`, même format de nœud. `CPhysical::AddToMovingList` (`0x469680`)
n'inscrit l'entité que si son nœud (`+0x17c`) est nul **et** si elle n'attend
pas sa collision (`+0xac`).

`RemoveFromMovingList` (`0x4696f0`) commence par regarder si le nœud retiré est
celui que tient le curseur global `0xc1ae84` ; si oui, le curseur avance sur le
suivant avant le décrochage. Sans cette précaution, retirer l'entité en cours de
traitement couperait la boucle qui parcourt la liste.

## Ce que CWorld::Add et CWorld::Remove testent

`CWorld::Add` (`0x45d560`) lit quatre champs de `CEntity`, tous stockés sur un
mot entier et comparés à zéro :

| décalage | rôle |
|---|---|
| `+0x28` | `bIsStatic` |
| `+0x58` | `bIsBIGBuilding` |
| `+0xac` | `bIsStaticWaitingForCollision` |
| `+0xf0` | non nul = l'entité rejoint la liste des mobiles |

`+0xf0` remplace le test `IsPhysical()` de reVC. Son sens exact n'est pas
retrouvé : le décalage est partagé par beaucoup d'autres classes, donc aucune
recherche par références croisées ne le tranche. Il garde son nom neutre.

`CWorld::Remove` est symétrique, et finit par un appel à `0x5e6830` quand le
slot 33 (drapeau `0x10000` du modelinfo) répond vrai. Désinscription d'un
gestionnaire, à identifier.

## Les entrées de secteur

Seul `CPhysical` en tient. `CEntryInfoNode` fait `0x14` octets, alloué par
`0x4296b0` et libéré par `0x4296c0`, avec cinq mots : la liste, le nœud dans
cette liste, le secteur, puis `prev` et `next`. Insertion en tête, liste
doublement chaînée, tête en `+0x178` juste avant le nœud de la liste des
mobiles en `+0x17c`.

`CPhysical::Remove` (`0x46a620`) parcourt ces entrées, retire chaque nœud de sa
liste de secteur, puis détruit l'entrée.

## Écart assumé dans la recréation

`src/core/Lists.h` garde des `CPtrNode` à pointeurs plutôt que des mots
compressés. La sémantique des listes est identique et le code reste lisible ;
la disposition mémoire d'origine n'est de toute façon pas reproduite ailleurs
dans le projet. Le jour où il faudra vraiment des poignées, par exemple pour
sérialiser une sauvegarde, tout est décrit ici.
