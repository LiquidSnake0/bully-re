# Rendu temps réel : la première visite

`outils/visite` promène une caméra à la première personne dans une scène du
jeu (un fichier de placements `.ipb`), en temps réel.

```sh
BULLY_DATA=<racine du jeu> build/outils/visite iboxing.ipb
BULLY_DATA=<racine du jeu> build/outils/visite iboxing.ipb --pos -717 377 296 0 -8 --image vue.ppm
BULLY_DATA=<racine du jeu> build/outils/visite isc_dorm.ipb --banc 36
```

Touches : ZQSD / WASD pour marcher (lues par position, donc identiques en
AZERTY et en QWERTY), Espace / C pour monter et descendre, Maj pour aller
vite, flèches ou souris (clic pour la capturer) pour regarder, P pour une
capture, Échap pour quitter. `--image` et `--banc` marchent sans fenêtre.

## Le choix : tout le rendu en logiciel, 400 × 240

L'image fait 400 × 240, la définition de l'écran du haut de la New 3DS, la
cible finale du projet. SDL2 ne sert qu'à ouvrir une fenêtre, lire le clavier
et agrandir l'image : ni la caméra ni le rasteriseur n'en dépendent, et sans
SDL2 la visite se compile quand même (`-DVISITE_SANS_SDL`). C'est le même
principe que pour les chargeurs : rien qui ne se transpose pas.

| Scène | Triangles | Temps par image (PC) |
|---|---|---|
| `iboxing` (salle de boxe) | 20 738 | 3,7 ms |
| `isc_dorm` (dortoir) | 34 665 | 4,5 ms |

Ce sont des chiffres de PC. Ils ne disent rien de l'ARM11 à 804 MHz de la New
3DS ; ils disent seulement que l'algorithme n'est pas le problème.

## Trois pièces

- **`src/render/Camera`** : repère du jeu (Z en haut), lacet et tangage,
  passage monde → caméra → écran. **Le découpage au plan proche** est ici :
  sans lui, un triangle dont un sommet passe derrière l'œil se projette de
  travers sur tout l'écran, ce qui arrive à chaque mur dès qu'on entre dans
  un bâtiment. Un triangle découpé donne au plus deux triangles.
- **`RasterTrianglePersp`** : triangle texturé en **perspective correcte**.
  En perspective, u n'est pas linéaire à l'écran, u/w l'est : on interpole
  u/w, v/w et 1/w, puis on divise. Le test le montre sans ambiguïté : sur un
  sol incliné, la moitié de la texture tombe à y = 16 au lieu de 32 en
  interpolation naïve. Les fonctions d'arête avancent pas à pas au lieu
  d'être recalculées à chaque pixel.
- **La transparence**, lue dans les modèles : `NiAlphaProperty` est
  maintenant décodée (2 305 blocs, tous à l'octet près ; le test des modèles
  passe de 363 085 à 365 390 blocs décodés). Drapeaux Gamebryo : bit 0
  mélange, bits 1-4 et 5-8 facteurs source et destination, bit 9 test alpha.
  Quatre modes de rendu : opaque, test alpha, mélange, et **ajout** quand la
  destination vaut ONE. Les halos de lumière au sol de la salle de boxe sont
  des ajouts : dessinés opaques, c'étaient des carrés noirs.

## Ce que le rendu ne fait pas encore

- pas de tri des surfaces transparentes (elles sont dessinées après les
  opaques, dans l'ordre des modèles) ;
- pas de couleurs de sommets ni d'éclairage du jeu : une lumière directionnelle
  fixe, calculée une fois par triangle ;
- une seule scène à la fois, pas de streaming des secteurs.

## La marche : les collisions du jeu

F passe du vol à la marche. En marche, la caméra a un corps (pieds, yeux à
1,60 m, rayon 30 cm) qui marche dans les volumes de collision du jeu :

- les 488 `.col` de `World.img` sont chargés une fois (3 862 modèles de
  collision), puis ceux des placements de la scène sont posés avec **la même
  transformation que les modèles visibles** : le modèle de collision vit dans
  l'espace entité, comme la géométrie (`docs/ipl.md`) ;
- `src/collision/Marche` : le sol est la plus haute surface sous les pieds
  (triangles, boîtes converties en 12 triangles, sphères), trouvée depuis la
  hauteur d'une marche (45 cm) ; les murs repoussent deux sphères placées le
  long du corps, à l'horizontale seulement, pour qu'un mur ne soulève
  jamais ; gravité et saut (Espace).
- `tests/test_marche` le vérifie sans fichier du jeu : chute et appui, marche
  de 30 cm montée, marche d'un mètre refusée, arrêt à un rayon du mur, rampe
  suivie au millimètre.

Ce que la salle de boxe a appris :

- **les modèles jamais dessinés comptent pour la marche.** `WALKABLE_` porte
  des sols ; `NOGO_` est une **zone interdite**, un volume fermé qu'on ne
  traverse pas. Sans `NOGO_iboxingOP`, le corps sortait de la zone jouable et
  tombait, parce qu'au-delà il n'y a plus aucun sol. Avec, il s'arrête au
  bord : c'est la règle du jeu, pas un défaut.
- `--promenade n` marche n images droit devant, sans fenêtre, et affiche le
  trajet. Depuis (−727, 377) : vers le ring, arrêt à x = −717,21, soit
  exactement le bord du ring (−716,91) moins le rayon du corps ; vers +y,
  arrêt contre la limite de la zone ; pieds à z = 293,91 tout du long.

Ce n'est pas la physique de `bully.exe` (`CWorld::ProcessLineOfSight`,
`CCollision::ProcessColModels`), dont les dispositions ne sont pas encore
retrouvées : c'est ce qu'il faut pour marcher, testable et transposable.
Pas encore d'arbre de partition : chaque image teste tous les triangles de la
scène, ce qui suffit à cette taille (moins de mille triangles de collision
pour la salle de boxe).

## Enchaîner les scènes voisines (30.09.2026)

Le monde extérieur n'est pas une scène : ce sont 41 fichiers de placements
qu'on charge selon l'endroit où l'on est. `outils/carte` lit les 85 `.ipb` de
`World.img` et donne l'emprise de chacun (`outils/monde.h`, `CarteDesIpb`) :

- **`t*`**, les quartiers : `tschool` (l'école), `tbusines`, `trich`,
  `tindust`, `tcarni`, `tgokart`, `tjyard`, `tBMX`. Bâtiments, sols, grilles ;
  de 200 à 500 m de côté.
- **`zone_*`**, les tuiles : `zone_school1..6`, `zone_busines1..8`,
  `zone_indust1..6`, `zone_rich1..11`, `zone_carn1`. Le mobilier urbain
  (lampadaires, poteaux, appliques), 80 à 150 m de côté.
- **`tGlobal`** : le relief (`GLOBALSKIN*`), les pylônes, la rivière, sur
  toute la carte ; son emprise couvre tout, il est toujours chargé.
- **`i*`**, les intérieurs : posés à part (la salle de boxe est à z = 294),
  ils restent des scènes seules. `iMGRace*` sont les parcours des courses,
  `ttest` et `ftest` des terrains d'essai : écartés.

La visite, avec un fichier extérieur ou `--monde`, charge tout ce dont
l'emprise est à moins de `--rayon` mètres (60 par défaut) et libère ce qui
s'éloigne au-delà du double ; l'écart entre les deux seuils évite de charger
et libérer en boucle à une frontière. Chaque fichier devient un morceau :
sa géométrie, son éclairage, ses collisions ; celles de tous les morceaux sont
réunies à chaque changement. Vérifié par `--survol 3600` (une minute droit
vers l'est à 12 m/s, sans collisions) : depuis la cour de l'école (60, −100),
les tuiles de l'école se chargent puis se libèrent, `tbusines` arrive à
x = 300, `tschool` part à x = 492, et l'on finit dans le quartier d'affaires.

**Rejet par modèle.** Dehors, c'est 300 000 à 460 000 triangles chargés. La
scène garde maintenant un bloc par modèle placé (plages de sommets et de
triangles, sphère englobante) ; la visite rejette les modèles hors du cône de
vue ou au-delà de 250 m avant de passer leurs sommets dans le repère caméra.
Images identiques au pixel près (salle de boxe et école comparées avant et
après), et : salle de boxe 4,3 → 3,1 ms, cour de l'école 65,6 → 47,3 ms
(21 i/s, 78 000 triangles dessinés, 178 modèles retenus), quartier d'affaires
39,8 ms. Le rasteriseur lui-même est désormais le poste principal.

## Alléger le rendu extérieur (30.09.2026, suite)

Mesure d'abord, en coupant le rendu à chaque étape (quartier d'affaires) :
7 ms pour retenir les modèles et passer leurs sommets dans le repère caméra,
4 ms pour découper et projeter, **22 ms de rasterisation**. C'est donc le
rasteriseur qu'il faut soulager, pas la géométrie.

- **Faces arrière.** `NiStencilProperty` décodée (`docs/nif.md`) : toutes
  celles du jeu disent « les deux faces ». Une forme sans elle n'est dessinée
  que de face, comme le fait Gamebryo par défaut. Dans la cour de l'école,
  31 000 triangles sur 78 000 tombent ainsi. Écart d'image : 22 pixels sur
  96 000 (école), 196 (quartier d'affaires). Dans la salle de boxe vue de
  dehors, on voit désormais dans la pièce : les murs intérieurs regardent vers
  l'intérieur, et le jeu, qui élimine leur dos, fait la même chose.
- **Du plus proche au plus lointain.** Les modèles retenus, tous morceaux
  confondus, sont triés sur la distance de leur centre : ce qui est devant
  remplit la profondeur d'abord, et les pixels cachés s'arrêtent au test de
  profondeur, avant la texture. La transparence, elle, se pose dans l'ordre
  inverse, de l'arrière vers l'avant, comme Gamebryo trie ses objets
  transparents. Les seuls pixels qui changent sont là où des surfaces
  mélangées se recouvrent (buissons, fenêtres éclairées) : l'ancien ordre,
  celui des fichiers, n'était pas plus juste.
- **La boucle par pixel.** La partie fractionnaire des coordonnées de
  texture sans `floorf` (troncature corrigée sous zéro), l'échelle de la
  texture calculée une fois par triangle, et plus de bornage de la couleur
  quand l'ombrage ne peut pas dépasser 1.

Essayés et écartés : ne pas dessiner les modèles de moins de 1, 2 ou
4 pixels à l'écran (gain dans le bruit de mesure) ; `-O3` (image identique,
gain dans le bruit).

Meilleur de trois mesures de 40 images, avant → après : cour de l'école
39,7 → 29,9 ms, quartier d'affaires 42,1 → 28,2 ms (35 i/s), salle de boxe
3,1 → 2,3 ms.

Prochaine étape : les piétons, ou un arbre de partition pour les collisions
(dehors, chaque pas teste 25 000 triangles).
