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
- pas de collision : la caméra traverse les murs ;
- une seule scène à la fois, pas de streaming des secteurs.

Prochaine étape naturelle : les collisions déjà lues (`src/collision`) pour
marcher sur le sol au lieu de voler, puis charger les scènes voisines.
