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

## Les lumières de nuit (02.10.2026)

Deux mécanismes des `.idb` :

- **Modèles à horaire (`tobj`)** : 158 modèles, visibles de l'heure
  d'allumage à celle d'extinction (CTimeModelInfo +0x34 / +0x38) : paires
  jour / nuit (`autoscreen01d` 7-19, `autoscreen01n` 19-7), les faisceaux du
  calmar de la fête foraine (`ca1d_squidbeams` 18-7), de l'herbe (6-19)… 24
  modèles portent « -1 » comme heure d'allumage, tous des lumières de nuit
  (`dl_*_nlights`, `dl_*_winglow`) : **hypothèse** prise ici, -1 = la tombée
  du jour, 19 h (l'heure où timecyc passe à la nuit). À confirmer dans le code
  du CTimeModelInfo.
- **Effets `2dfx`** : 625 lumières sur 164 modèles (lampadaires, lustres,
  lampes murales, guirlandes de la fête foraine…) : position dans l'espace du
  modèle, couleur RVBA, distance de visibilité (30 m le plus souvent), taille,
  texture de halo « coronastar » (542) ou « corona » (83).

Dans la visite : un bloc à horaire est caché hors de ses heures ; chaque
lumière posée devient un halo additif (disque doux de sa couleur, rayon =
taille), testé contre la profondeur au centre (un mur devant la masque), dont
l'intensité monte quand le soleil de timecyc baisse (0,1 en plein jour, 1 la
nuit). Les textures de halo ne sont pas encore utilisées.
