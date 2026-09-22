# Streaming

`CStreaming` numérote tout ce qui se charge depuis `Stream/World.img`
(11 980 entrées : 5 724 `.nif`, 4 469 `.nft`, 550 `.agr`, 493 `.lip`,
488 `.col`, 119 `.cat`, 85 `.ipb`, 52 `.lur`). La fonction 0x52e990 rend
le nom de fichier d'un identifiant de streaming :

| Identifiants | Contenu | Nom |
|---|---|---|
| 0 .. 0x2fa8 (12 200) | modèles | `%d.nif` (le nombre est le champ +4 du modelinfo) |
| 0x2fa8 .. 0x56b8 (10 000) | dictionnaires de textures | `%d.nft` |
| 0x56b8 .. 0x58ac (500) | collisions | `%s.col` |
| 0x58ac .. 0x5af0 (580) | groupes d'animation | `%s.AGR` |
| 0x5af0 .. 0x5b54 (100) | placements | `%s.IPB` (ou `.IPC` selon 0xbf3811) |
| 0x5b54 .. 0x5d41 (493) | synchronisation labiale | `lipfile_%03d.lip.LIP` |
| 0x5d41 .. 0x5db9 (120) | catalogues `.cat` | `%s.%s` |
| 0x5db9 .. | scripts Lua compilés | `%s.LUC` |

Les placements : `CStreaming::LoadCdDirectoryIPBFiles` (0x52ebf0) parcourt
les images ouvertes, `LoadCdDirectoryIPB` (0x52ded0) lit le `.dir`, et pour
chaque entrée `.IPB` retrouve le slot de `CIplStore` par le hachage du nom
(0x437060), lit le fichier dans `work_buff` et l'analyse (0x438f50, puis
`IplFileFormat` 0x435780). Format dans `src/core/IplFile.h`.
