# Les cinématiques

Établi le 18.10.2026 dans l'index Ghidra (`tools/chercher.py`), les scripts désassemblés
(`tools/lur.py`) et `Cuts/Cuts.img`. Code : `src/cutscene/CutsceneFiles`, test
`tests/test_cutscene` (lit les 863 entrées de l'image).

## Ce que les scripts appellent

Aucune mission ne touche au moteur directement : les 80 scripts qui jouent une cinématique
appellent **`PlayCutsceneWithLoad(nom, …)`**, une fonction Lua de `SCutscenes.lua` (importée
dans `_G` par `main.lua`). `1_01` fait `PlayCutsceneWithLoad("1-1-1", true, true, true)` dans
`MissionSetup`, après avoir posé le joueur à la grille.

`SCutscenes.lua` tient **`cutsceneTable`**, 111 entrées construites par `F_BuildCutsceneTable` :
`name`, `x`, `y`, `area`, `radius` (10), `time` (la durée en ms : 61 666,66 pour `1-1-1`),
`IPLs`, `collision`, `textures`, `setupCutscene`, `specialInit`. `PlayCutsceneWithLoad` cherche
le nom dans la table et appelle `F_PlayCutScene(i, true, …)`, qui enchaîne :

1. `CameraFade(1000, 0)` (vers le noir), `Wait(1000)` ; la zone, la position et le cap du joueur
   sont notés pour le retour ;
2. **`LoadCutscene(nom, false)`** ; si la cinématique se joue ailleurs,
   `AreaForceLoadAreaByAreaTransition(true)`, `AreaTransitionXYZ(area, x, y, 10)`
   (docs/zones.md) ; **`LoadCutsceneSound(nom)`** ; `CutSceneSetActionNode(nom)` si
   `setupCutscene` ; `CameraDefaultFOV()` ;
3. si **`IsCutsceneLoaded()`** : **`StartCutscene()`**, `CameraFade(1000, 1)`, `Wait(1000)`,
   puis une boucle `Wait` tant que **`GetCutsceneRunning() ~= 0`** et
   **`GetCutsceneTime() < time − 1000`** ; le bouton 7 la coupe
   (`CutsceneFadeWithCamera(true)`) ; `1-1-1` a un cas particulier : passé 58 s, les
   grilles de l'école sont fermées (`GeometryInstance("ScGate01Closed")`…) ;
4. `MissionSurpressMissionNameText()`, `CameraFade(1000, 0)`, `Wait(1000)`,
   **`StopCutscene()`**, `CutsceneFadeWithCamera(false)`, `CameraSetWidescreen(false)`, retour
   du joueur (`AreaTransitionXYZ`), `PlayerFaceHeading`, `CameraReturnToPlayer()`,
   `AreaRemoveExtraScene()`, `CameraFade(1000, 1)`.

Les fonctions du moteur, toutes dans la table `00ae9890` de `CreateLuaState` sauf les deux
dernières :

| Fonction Lua | Adresse | Ce qu'elle fait |
|---|---|---|
| `LoadCutscene(nom, drapeau)` | 0x5bb100 | `CCutsceneMgr::LoadCutsceneData` (`FUN_006c4480`, 7 504 octets) |
| `LoadCutsceneSound(nom)` | 0x5bb130 | `FUN_006c3750` : la banque de sons du nom (sans son dossier), `DAT_020c5be7 = 1` |
| `StartCutscene()` | 0x5bb150 | `FUN_006c6210` : l'arbre d'actions `/Global/CutScenes`, les animations de chaque objet, `DAT_020c5b1c = 0`, **`DAT_020c5be4 = 1`** |
| `StopCutscene()` | 0x5bb160 | `CCutsceneMgr::DeleteCutsceneData` (`FUN_006c3ed0`) : objets, modèles et sous-titres libérés |
| `GetCutsceneTime()` | 0x5bb170 | `FUN_006c3970` : `DAT_020c5b1c × 1000`, arrondi (ms) |
| `GetCutsceneRunning()` | 0x5bb190 | l'octet `DAT_020c5be4` |
| `IsCutsceneLoaded()` | 0x5bb240 | l'octet `DAT_020c5be2`, posé à la fin du chargement |
| `CutSceneSetActionNode(nom, drapeau)` | 0x5bb1b0 | `FUN_006c3da0` : un nœud de `/Global/CutScenes` par nom, chargé au besoin |
| `CutSetCutsceneTableSize(n)` | 0x5bb1e0 | `DAT_00bceb00 = n` |
| `CutSetCutsceneName(nom)` | 0x5bb200 | `strncpy(DAT_00bceb08, nom, 16)` |
| `AddPAnimToNextCutscene(déclencheur)` | 0x5bb260 | lit +0x1c de l'entrée du pool des déclencheurs ; aucun script ne l'appelle |
| `CutsceneFadeWithCamera(b)` | 0x5d4610 | `SoundFadeWithCamera(0, b)` |
| `MusicAllowPlayDuringCutscenes(b)` | 0x5d49a0 | |

Le temps avance dans `FUN_006c4350`, appelée à chaque image : si `DAT_020c5be4` et que le jeu
n'est pas en pause, chaque objet de la cinématique est mis à jour (vtable +0x4c), et
`DAT_020c5b1c += DAT_00bb64f0 × CTimer::ms_fTimeStep`. **La fin ne vient pas du moteur mais de
l'arbre d'actions** : le nœud de cinématique (`FUN_006555a0`, un `ActionNode` avec son drapeau
+0x5c) appelle `FUN_006c3a30` au début (`DAT_020c5be4 = 1`) et `FUN_006c3a70` à la sortie
(`DAT_020c5be4 = 0`), quand ses animations sont finies. La durée de `cutsceneTable` est celle de
ces animations : pour 45 des 58 cinématiques nommées en clair dans la table, elle est égale à la
dernière clé de la piste caméra ; pour 13 elle en diffère (`1-B` : 80 s de caméra, 67,7 s dans
la table ; `2-02` : 48,3 contre 32,7).

## Les fichiers : Cuts/Cuts.img

863 entrées, lues par nom de cinématique (`1-1-1`, `2-05`, `3-S11c`…). `LoadCutsceneData`
cherche d'abord `NOM.CUT` (`NOM_W.CUT` en hiver), qui n'existe dans aucune version livrée, et
retombe sur :

| Fichier | Contenu | Relevé |
|---|---|---|
| `NOM.INF` | le nombre d'objets, puis un nom par ligne (`CS_PLAY`, `CS_TE_SECRETARY`, `CS_CAR`) ; un nom en `f_` est le **jeu de poids faciaux** de l'objet du même nom et passe en fin de liste (« Read facial weight set ») | 122 fichiers, 438 objets, 300 faciaux ; 16 objets au plus |
| `NOM.SUB` | `{1}{1}30.000` (images par seconde) puis `{début}{fin}CLÉ` par sous-titre, en images, lus par `sscanf("{%d}{%d}%s")` dans des entrées de 0x20 octets ; la clé est dans `Config/Text` ; le texte `NOM_SUB` est demandé à part (`FUN_0068fbf0`, `%s.bin` haché) | 107 fichiers, 2 275 sous-titres, 30 images/s partout |
| `NOM.DAT` | **la piste de la caméra** (`FUN_004f86d0`), texte : quatre sections finies par `;`, chacune « n, » puis n lignes : deux à **six** nombres (`FUN_004f8200`, 0x18 octets par clé : instant, valeur, puis quatre nombres), deux à **douze** (`FUN_004f8420`, 0x30 octets : instant, x y z, deux tangentes, deux nombres) ; puis une liste de positions (nombre, `x,y,z`), vide dans les 126 fichiers. Première section : le champ de vision (80° → 48,6° à 9,7 s dans `1-1-1`) ; deuxième : à zéro partout ; troisième : la position de la caméra (319.7, −64.2, 11.4 au-dessus de la grille) ; quatrième : sa cible | 126 fichiers, tous à cinq `;` |
| `NOM_CHAR.agr`, `NOM_CAR.agr`, `NOM_PROP2.agr` | les animations des objets, par groupe (docs/agr.md) ; `LoadCutsceneData` parcourt ses 40 groupes de hiérarchies et charge `%s.AGR` pour ceux dont le nom commence par `NOM` | 170 |
| `NOM_CS.HXD`, `NOM_CS_CAR.HXD`, `NOM_CSPROP2.hxd` | les hiérarchies (docs/hxd.md) | 170 |
| `OBJET.mxd` (`OBJET_W.mxd` en hiver, sauf `CS_PLAY`) | le modèle d'un objet, cherché dans `Cuts\Models\` (docs/mxd.md) | 168 |
| `Cuts\NOM.DAT` | la même piste caméra, aussi lue par `FUN_00684290` pour une caméra scriptée hors cinématique | |

`DAT_020c5c09` dit si la piste caméra existe ; `DAT_020c5bf0` compte les objets, `DAT_020c4c18`
leurs noms (0x40 octets chacun), `DAT_020c4ba0` leurs modèles, `DAT_020c5b68` les objets
créés (30 au plus). Le chargement finit par `CStreaming::LoadAllRequestedModels` jusqu'à onze
fois, puis « Create Objects », la caméra, les sous-titres, et `DAT_020c5be2 = 1`.

`src/cutscene/CutsceneFiles` lit les trois fichiers texte comme le moteur (`CCutsceneInf`,
`CCutsceneSub`, `CCutsceneCamera`) et liste les animations d'une cinématique ;
`tests/test_cutscene` vérifie les comptes ci-dessus et `1-1-1` : trois objets dont deux avec
leur facial (pas la voiture), 20 sous-titres de 270 à 1 695 images, 13 / 20 / 30 / 29 clés,
61,67 s ; `1-1-02` n'a que sa caméra.

## Dans lancer_lua

`LoadCutscene`, `LoadCutsceneSound`, `IsCutsceneLoaded`, `StartCutscene`, `GetCutsceneRunning`,
`GetCutsceneTime`, `StopCutscene`, `CutSetCutsceneTableSize` et `CutSetCutsceneName` sont réels
sur `CCutsceneFiles` : la cinématique est « chargée » si son `.INF` donne des objets (sans
objet, le moteur ne pose jamais `DAT_020c5be2`, et `SCutscenes.lua` passe à `StopCutscene`),
elle tourne le temps de la dernière clé de sa piste caméra (l'arbre d'actions n'est pas joué),
et ses sous-titres passent au journal à leur image de début. Sur `1_01` :

    build/outils/lancer_lua 80 --mission 1_01 --entrer _1_01_BULLYTRIG@70 --zone 2@72 --aller _1_01_ENDPOINT@74

`MissionSetup` charge `1-1-1` à 1,06 s (3 objets, 20 sous-titres, 61,67 s), la lance à 1,29 s, le
premier sous-titre `1-1-1.SUB0001` tombe à 9,0 s, la cinématique finit à 62,93 s, `StopCutscene`
à 63,00 s, et `main` démarre ; **`1_01` se termine réussie à 78,1 s**. `--sans-cinematiques`
les saute (`IsCutsceneLoaded` répond faux, comme quand le chargement échoue) : 10,5 s depuis que la
parole a sa vraie durée (docs/son.md).

## Reste à faire

- Les animations : relier `NOM_CHAR.agr` et `NOM_CS.HXD` aux objets de l'`.INF` (ce que fait
  `FUN_006c6210` par les hachages `NOM_OBJET`), puis les jouer dans la visite.
- Le sens des deux derniers nombres des clés caméra (0,3 et 0,2 par endroits) et de la
  deuxième piste.
