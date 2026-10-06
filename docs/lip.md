# La synchronisation labiale (`.lip`)

493 fichiers dans `Stream/World.img`, `lipFile_000.lip` à `lipFile_492.lip`
(25 vides). Chacun regroupe des répliques : pour chaque réplique, l'ouverture
de la bouche image par image, et l'emplacement du son dans sa banque.
Établi le 06.10.2026 sur les données, puis confirmé dans `bully.exe`.

Code : `src/anim/Lip` ; test : `tests/test_lip` (les 493 fichiers).

## Dans bully.exe

- `FUN_0052e990` nomme les entrées du streaming : l'indice `0x5b54 + n`
  (jusqu'à `0x5d40`) donne `lipfile_%03d.lip`.
- `FUN_0068df50` charge un fichier dans l'emplacement `n` (493 au plus,
  `0x1ed`) : il lit les deux mots de 16 bits de l'en-tête, copie les
  répliques (24 octets chacune) et les données dans deux blocs, puis marque
  l'emplacement chargé (octet `0x1734 + n` de l'objet).
- `FUN_0068dd30` lit l'ouverture de la bouche à un instant (ci-dessous).

## Le fichier

| Décalage | Taille | Contenu |
|---|---|---|
| 0 | 2 | nombre de répliques `m` |
| 2 | 2 | taille des données, en octets |
| 4 | 24 × `m` | les répliques |
| 4 + 24 `m` | | les données |

Rien ne suit les données (vérifié sur les 493 fichiers). Une réplique :

| Décalage | Type | Contenu |
|---|---|---|
| 0 | u32 | un petit numéro (11783 pour la première de `lipFile_000`) |
| 4 | f32 | la durée, en secondes |
| 8 | u16 | `n`, le nombre d'images (environ 33,5 par seconde) |
| 10 | u16 | le départ dans les données, en octets |
| 12 | u32 | le décalage du son dans sa banque (multiple de `0x800`) |
| 16 | u32 | la taille du son |
| 20 | u32 | un hachage (non identifié) |

Les répliques se suivent dans les données : chacune prend `ceil(n / 4)`
octets, et le départ de la suivante est la fin de la précédente (493 fichiers
sur 493, 40 882 répliques).

## Les images

Deux bits par image, **poids forts d'abord** : l'image `k` est dans l'octet
`depart + k / 4`, décalée de `(3 − k mod 4) × 2`. Le niveau va de 0 (bouche
fermée) à 3 (grande ouverte) : 2 014 903 images à 0, 302 509 à 1, 538 355 à
2 et 445 987 à 3.

L'ouverture à l'instant `u` (0 au début, 1 à la fin de la réplique), d'après
`FUN_0068dd30` :

```
x = u × (n − 1)
i = troncature(x)            (_ftol)
j = i + 1, ou i à la dernière image
f = x − i
ouverture = ((1 − f) × niveau(i) + f × niveau(j)) / 3
```

La division par 3 est le double 1/3 rangé en `0x9145d0`. Le résultat va donc
de 0 à 1 : c'est un degré d'ouverture, pas un visème (pas de forme de bouche
par son).

## Ce qui reste

- Le hachage de fin de réplique et le numéro du début : à rapprocher des noms
  des répliques (scripts, banques de sons).
- La banque de sons que désignent décalage et taille.
- Qui appelle `FUN_0068dd30` et quel os il pilote (la mâchoire, `Jaw`, os 15
  de `MAINPED.HXD`, est le candidat évident).
