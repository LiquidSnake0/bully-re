# Les textures : le format `.nft`

`Stream/World.img` contient 4 469 fichiers `.nft`, soit près du double des
modèles. Ce ne sont pas des images au sens habituel : **un `.nft` est un
fichier NIF**, avec la même ligne d'en-tête « Gamebryo File Format, Version
20.3.0.9 » et la même table de blocs que les `.nif` de `docs/nif.md`. La
différence est ce qu'il contient : uniquement des blocs de texture, jamais de
géométrie.

Le lecteur est donc le même, `src/gamebryo/NifFile`. Il a suffi d'y ajouter
les quatre types de blocs manquants.

## Les six types de blocs

| Type | Nombre | Rôle |
|---|---|---|
| `NiPixelData` | 35 635 | le format et les octets de l'image |
| `NiStringExtraData` | ~35 600 | le chemin du `.tga` d'origine |
| `NiIntegerExtraData` | ~35 000 | la version de l'outil d'export |
| `NiSourceTexture` | ~35 400 | relie un nom de fichier à ses pixels |
| `NiSourceCubeMap` | 214 | une texture de cube, même disposition |
| `NiPalette` | 127 | palette de 256 couleurs |

## NiPixelData

C'est le bloc qui portait tout l'inconnu. Sa disposition, vérifiée à l'octet
près sur les 35 635 blocs :

```
u32  pixelFormat          4 sur toutes les textures du jeu (compressé)
u8   bitsPerPixel
i32  rendererHint         -1
u32  extraData
u8   flags
u32  tiling
u8   sRGB                 propre à la 20.3
     4 × { u32 type; u32 convention; u8 bitsPerChannel; u8 isSigned; }
i32  palette              référence NiPalette, -1 si aucune
u32  numMipmaps
u32  bytesPerPixel
     numMipmaps × { u32 width; u32 height; u32 offset; }
u32  numPixels            octets d'une face, toutes mipmaps comprises
u32  numFaces
u8   pixels[numPixels × numFaces]
```

La taille d'un bloc vaut donc exactement **79 + 12 × numMipmaps + numPixels ×
numFaces**, ce qui donne un contrôle direct contre la table des tailles de
l'en-tête.

Les quatre canaux sont toujours présents, même quand la texture est
compressée : le premier porte le type 4 et la convention 4, les trois autres
le type 19 et la convention 5, qui signifient « vide ». Les tailles confirment
du DXT1 : une image 8 × 8 occupe 32 octets, soit deux blocs sur deux de huit
octets, et une 128 × 128 en occupe 8 192.

La pyramide de mipmaps descend jusqu'à 1 × 1, avec un plancher de huit octets
par niveau puisqu'un bloc DXT ne peut pas être plus petit. Au total l'archive
porte 258 535 niveaux de mipmap et 1,25 Go de pixels.

## NiPalette

Trivial une fois vu : `u8 hasAlpha`, `u32 numEntries`, puis `numEntries × 4`
octets de couleurs. Toujours 256 entrées, soit 1 029 octets.

## NiSourceTexture, et le piège du cas externe

La structure ne change pas selon que la texture est interne ou externe, ce qui
n'était pas évident :

```
i32  name
u32  numExtraData  puis autant de i32
i32  controller
u8   useExternal
i32  fileName      index de chaîne
i32  pixelData     référence, ou -1 quand la texture est externe
u32  pixelLayout, useMipmaps, alphaFormat
u8   isStatic, directRender, persistRenderData
```

Dans les `.nft`, `useExternal` vaut 0 et le second mot pointe vers le
`NiPixelData` du même fichier. Dans les `.nif`, les 466 `NiSourceCubeMap`
ont `useExternal` à 1 et ce mot vaut -1 : ils nomment un `.nft` à charger.
Le bloc fait 36 octets dans ce cas et 44 dans l'autre, la différence venant
du nombre de références supplémentaires, pas de la présence du champ.

Avoir traité ce cas fait aussi progresser la lecture des modèles : le test
des `.nif` décode maintenant **363 085 blocs** au lieu de 286 403.

## Le boutisme, encore

131 fichiers `.nft` sont grand-boutistes, tous préfixés `CS_` comme les 247
`.nif` dans le même cas. Le piège est identique et mérite d'être répété : la
version, l'octet de boutisme et **le nombre de blocs se lisent toujours en
petit-boutiste**, et le boutisme ne s'applique qu'à partir du nombre de types.
Lire le nombre de blocs en grand-boutiste donne 201 326 592 au lieu de 12.

## Ce qui résiste

Quatre fichiers sur 4 469 se désalignent au quatrième bloc : **BBonusB**,
**Barr01_Switch**, **BeerKeg** et **BirdBath**. Tous ont la même forme, trois
textures de 128 × 128 avec huit niveaux de mipmap, et tous portent un octet
de trop entre le troisième et le quatrième bloc. Pris isolément, leur
`NiPixelData` se lit pourtant à l'octet près et sa taille correspond à celle
annoncée. L'octet parasite n'est donc pas dans le bloc que je décode mal, et
son origine reste à trouver. Le test les attend en échec plutôt que de les
masquer.

## Décoder les pixels

`src/gamebryo/TextureDecode` transforme un `NiPixelData` en RGBA. Cinq formats
existent réellement dans l'archive, mesurés en rapportant la taille du premier
niveau de mipmap à son nombre de pixels :

| pixelFormat | Format | Octets pour 16 pixels | Blocs |
|---|---|---|---|
| 0 | RGB 24 bits | 48 | 134 |
| 1 | RGBA 32 bits | 64 | 134 |
| 2 | palette 8 bits | 16 | 127 |
| 4 | DXT1 (BC1) | 8 | 31 714 |
| 6 | DXT5 (BC3) | 16 | 3 526 |

Les numéros suivent l'énumération PixelFormat de Gamebryo ; le 5, DXT3, n'est
jamais utilisé. Pour les formats bruts, l'ordre des canaux se lit dans les
types de canal de l'en-tête plutôt que d'être supposé.

**Une hypothèse réfutée par les données.** J'attendais des blocs DXT inversés
dans les 131 fichiers grand-boutistes, comme les autres champs. C'est faux : les
pixels sont restés en petit-boutiste. Dans un bloc DXT1 opaque, les encodeurs
placent presque toujours la couleur la plus claire en premier :

| Fichiers | Blocs DXT1 | c0 > c1 |
|---|---|---|
| petit-boutistes | 88 007 163 | 86,5 % |
| grand-boutistes, lus tels quels | 1 738 088 | 79,5 % |
| grand-boutistes, mots inversés | 1 738 088 | 49,7 % |

Inverser donne exactement le hasard. Seuls les champs du NIF ont été convertis
à l'export, pas le contenu des images.

`tests/test_texture` vérifie d'abord des blocs construits à la main, dont le
résultat se calcule d'après la spécification (les deux modes de DXT1, les deux
modes d'alpha de DXT5, la réplication des bits 5:6:5), puis décode le premier
niveau des 35 635 textures : toutes passent, 1,65 milliard de pixels.

## Voir un modèle : outils/nif2obj

```sh
BULLY_DATA=/chemin/vers/Bully build/outils/nif2obj 70wagon
```

L'outil écrit `export-obj/70wagon/` avec un OBJ, son MTL et les textures en
TGA, lisibles dans Blender (import OBJ, axe Z vers le haut). Il suit la même
chaîne que le jeu :

1. **Le modèle nomme son dictionnaire de textures.** La colonne TXD des
   définitions `.idb` donne le `.nft` à charger : `fun_libwalls` prend ses
   textures dans `funhouse.nft`. Seuls 2 450 des 5 720 modèles ont un `.nft`
   du même nom, le reste partage des dictionnaires, exactement comme les TXD de
   GTA.
2. **Le NIF est parcouru depuis sa racine** en composant les transformations
   des nœuds : `monde(enfant) = monde(parent) ∘ (R·(s·p) + t)`.
3. **Chaque forme prend la texture de base de son `NiTexturingProperty`.** Le
   nom du `.tga` d'origine, débarrassé du chemin de la machine d'export
   (`Z:\Bully\Temp\...`), est cherché parmi les `NiSourceTexture` du `.nft`.

Vérifié à l'œil sur le break `70wagon` : 64 formes, 2 723 sommets, 2 022
triangles, six textures. La carrosserie, les panneaux en faux bois, les vitres,
les feux et les pare-chocs s'assemblent à leur place ; les roues manquent,
normal, ce sont des modèles séparés (`Foreign_wheel`). Les textures décodées
sont reconnaissables au premier coup d'œil.

La convention de rotation, `v' = R·v` avec R lue ligne par ligne comme le
`NiMatrix3` de Gamebryo, tient sur ce modèle mais n'a pas encore été éprouvée
sur des pièces franchement tournées les unes par rapport aux autres.

Les exports dérivent des données du jeu : `export-obj/` est dans `.gitignore`,
aucune image n'entre dans le dépôt.
