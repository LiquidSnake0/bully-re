# Le cycle jour-nuit : `Config/timecyc*.dat`

Le format des GTA : par météo, 24 lignes (minuit d'abord, une par heure),
colonnes documentées en tête de fichier. Saisons : `timecycF` (automne,
chapitre 1), `timecycW` (hiver : WINTER_SUNNY, SNOWY, SNOWSTORM…), `timecycS`
(été), `sbtimecycS` (printemps, SPRING_*) ; six météos chacun (Sunny, CLOUDY,
RAINY, FOGGY, EXTRASUNNY, HURRICANE) ; `timecycE` : trois jeux de couleurs
spéciales. On lit les 37 premières colonnes : ambiantes (objets, monde,
piétons), soleil, contre-jour, ciel haut et bas, soleil (cœur, halo, taille),
sprites, ombres, FarClp, FogSt, NightFactor. Une ligne de timecycF n'a que 60
colonnes sur 74 : les premières sont là.

Automne, beau temps : ciel noir en haut et bleu nuit à l'horizon (32, 44, 89)
la nuit, rose à 6-7 h, bleu-gris le jour, horizon orange (216, 139, 68) à
17 h, nuit dès 19 h. Le soleil passe de 159 le jour à 46 la nuit.

Dans la visite (rendu logiciel, sans le calcul d'éclairage du jeu) :
- le ciel est peint **avant** la scène, en dégradé selon l'élévation du rayon
  (bas à l'horizon, haut à 60°) : les surfaces transparentes, qui n'écrivent
  pas la profondeur, se posent ensuite dessus ;
- après la scène, les pixels qui ont une profondeur prennent une teinte
  (60 % soleil + 40 % ambiante du monde, rapportés à midi : une
  approximation) et se fondent vers le bas du ciel entre FogSt et FarClp ;
- l'état est interpolé entre deux heures pleines, à l'heure du jeu
  (docs/horloge.md).

Options : `--saison automne|hiver|ete|printemps`, `--meteo n`, `--sans-ciel`.
Code : `src/render/TimeCycle` ; test : `tests/test_timecycle`.
