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

## Les piétons, premier pas (30.09.2026)

Les 259 entrées « peds » des `.idb` donnent le modèle (`DOGirl_Zoe_EG.nif`
dans World.img) et le dictionnaire de textures de chaque piéton. Sans rien
décoder de plus, ils se dessinent déjà : les sommets d'un modèle animé sont
stockés dans sa **pose de repos** (les bras écartés), et c'est cette pose que
la visite affiche. `--pietons n` en pose n, pris dans toute la liste, en
cercle à 8 m devant la caméra, les pieds sur le sol du décor, tournés vers le
centre. Ce que ça a demandé :

- **Les formes cachées.** Un piéton porte deux aides d'édition : `Mesh`, une
  tige de 8 m, et `Editable Poly`, une flèche au sol. La première a le
  drapeau « caché » de Gamebryo (bit 0 des drapeaux, APP_CULLED), que le
  parcours des formes respecte désormais, pour les nœuds comme pour les
  formes : le décor n'en change pas (0, 1 et 39 pixels sur les trois vues de
  référence). La seconde n'est pas cachée mais ne déclare aucune texture :
  pour les piétons, on l'écarte.
- **Le dictionnaire d'hiver.** L'IDE associe souvent un piéton à sa variante
  d'hiver (`GRGirl_Lola_W`), qui ne contient pas les textures du modèle de
  base : une texture introuvable est cherchée ensuite dans le dictionnaire
  qui porte le nom du modèle.
- **Le sens du lacet.** Un piéton regarde vers −y dans son repère, et c'est le
  conjugué du quaternion de placement qui tourne les sommets (`docs/ipl.md`) :
  le lacet se donne donc avec un signe moins. Vérifié par le calcul sur
  quatre directions avant de l'être à l'image.
- Un dictionnaire absent ne plantait jamais, parce qu'on ne cherchait que des
  dictionnaires existants : sa structure est maintenant initialisée à zéro.

Sur 12 piétons demandés dans la cour de l'école, 11 sont posés ; le douzième,
`special7`, est une entrée de l'IDE sans modèle.

## Le squelette (30.09.2026, suite)

Les trois blocs de peau sont décodés et la formule du skinning établie
(`docs/nif.md`, « Le squelette »). Chaque modèle à peau est désormais
déformé par ses os au chargement (`outils/scene.h` → `src/gamebryo/NifSkin`),
y compris quelques objets du décor (116 pixels changent dans la cour de
l'école). `--bras` met aux piétons les bras le long du corps : le haut du
bras tourne autour de son y local de l'angle qui amène la main 5 cm à
l'extérieur de l'épaule (36° pour Jimmy, 51° pour Kirby et ses épaulières).
C'est une pose de démonstration, pas une animation du jeu.

## Les animations (30.09.2026, fin de soirée)

Le format des `.agr` est déchiffré pour le type 1002 (`docs/agr.md`), trouvé
dans le code de `bully.exe` (le décompresseur d'images clés en 0x6b1710) puis
vérifié sur les 550 fichiers. Chaque piéton de la visite joue maintenant
l'attente de sa catégorie (`IDLE_GSF_A` pour une élève, `IDLE_JOCK_A`,
`IDLE_GREAS_A`, `IDLE_AUTH_A` pour un préfet ou un flic…), décalée dans le
temps d'un piéton à l'autre. Dans la fenêtre, ils bougent en temps réel ; à
chaque image, les rotations des 36 os sont interpolées (slerp), le squelette
recomposé et la peau recalculée en place : 0,88 ms pour 11 piétons.
`--temps s` rend l'image à un instant donné, `--fige` garde la pose du
fichier. Le choix de l'animation par catégorie est une approximation : le
jeu choisit par ses arbres d'action (`Act/`).

## Les piétons marchent (30.09.2026, nuit)

Chaque piéton prend dans ses groupes `F_*` (ceux de l'IDE, sinon `F_Adult`
puis `F_Jocks`) le cycle qui boucle, avance droit vers +y et va le plus près
de 1,3 m/s (`docs/agr.md`, « La marche »). Il avance de ce que parcourt la
flèche `ARROW`, sur le corps de collision de la caméra (sol, marches, murs) ;
bloqué un quart de seconde, il tourne d'un quart à un demi-tour au hasard.
Son bassin monte et descend comme dans l'animation, son corps est tourné
dans le sens de la marche. Dans la cour de l'école, après 10 s simulées : 11
marcheurs, tous au sol, de 1,11 à 1,67 m/s, entre 5 et 17 m de leur départ.
`--attente` les garde sur place, `--anim groupe:n` impose une animation à
tous pour la regarder.

**Les collisions en grille.** Onze marcheurs testaient chacun, à chaque pas,
les 25 000 triangles de collision du dehors : 2,8 s de calcul pour 5 s
simulées. `CMondeCollision::Indexer` range les triangles dans une grille en
plan de cases de 4 m ; `Sol` et `Repousser` ne testent plus que ceux des
cases touchées (avec trois rayons de marge pour la poussée), pris dans leur
ordre d'origine. Résultat identique (même image au pixel près, mêmes
positions des promenades de contrôle, 0 écart sur 2 000 requêtes au hasard
dans `tests/test_marche`), pour 11 ms au lieu de 2 800.

Prochaine étape : les autres encodages des `.agr`, les transitions entre
attente et marche, et des trajets moins aléatoires (les chemins des
piétons du jeu).
