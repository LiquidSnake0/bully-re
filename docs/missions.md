# Les missions : la banque d'arbres d'actions

La liste des missions n'est pas dans un script Lua : c'est un arbre d'actions,
`Act/Missions.act` (`Missions.cat` dans `Act/Act.img`), le même format que les
comportements des personnages (docs/cat.md). `tools/missions.py` en sort la table.

## Chargement (bully.exe)

- `CGame::Initialise` (0x42ee90, profilé « CGameInitialise ») appelle
  `FUN_006ac5d0("MissionBank", "Act/Missions.act")` juste après l'écran de chargement.
- `FUN_006ac5d0` est une méthode du gestionnaire de missions. Elle charge l'arbre,
  puis `FUN_006ab7e0` le parcourt et range chaque nœud qui porte une piste
  `MissionData` (classe `CMissionData`).
- Champs du gestionnaire relevés dans `FUN_006ac5d0` :

| Champ | Contenu |
|---|---|
| +0x4 | la banque chargée |
| +0x4b8 | tableau des `MissionData`, une entrée de 0x4c octets par mission (la piste copiée) |
| +0x684 | tableau des nœuds de mission (pointeurs) |
| +0x688 | état de chaque mission, 6 octets par mission (le bit 0 de l'octet +4 est lu par `FUN_006aadc0`) |
| +0x744 | nombre de missions |
| +0x754 | nombre de missions de type 0 ou 3, avec l'octet 37 nul et l'octet 74 non nul |
| +0x740 | banque prête |

## La forme d'une mission

`Missions.cat` est la racine : une banque `MissionBank`, une mission factice, et
onze références vers les sous-arbres `chap1` à `chap5`, `classes`, `Minigames`,
`Punish`, `Tutorials`, `Doors` et `Debug`. Chaque mission est un nœud jouable :

```
n 1_01                                   ← le nom de la mission (MissionGetName)
   · MissionData 24='1.01 - Welcome To Bullworth Pt 1', 28='chap1\1_01.lua', 32='1_01_TITLE', 40=0 …
  n active                               ← gardé par MissionCanLaunch dans la mission factice
     · MissionButtonTrigger 24='1_01_Corona', 48='BUT_LAUNCH' …
    n RUN
      l Succeed
      l fail
```

Les conditions d'entrée du nœud (`si MissionCond`, `si MissionCond TimeCond`)
disent quand la mission devient lançable.

## Les champs de `MissionData`

La position est le décalage dans l'objet de la piste (docs/cat.md, « Les pistes »).

| Position | Sens | Comment on le sait |
|---|---|---|
| 24 | titre de travail | lisible |
| 28 | script Lua de la mission | lisible, `chapN\nom.lua` |
| 32 | clé du titre affiché (`1_01_TITLE`) | lisible, présente dans `Config/Text` |
| 40 | **type** : 0 histoire et courses, 1 cours, 3 boulots et courses de quartier, 4 tutoriels, 5 mini-jeux | répartition sur 473 missions ; `FUN_006ac110` traite le type 4 à l'heure, `FUN_006aac70` le type 1 |
| 36 | inconnu | **ce n'est pas « lancer au début de la partie »** : il vaut 1 pour 282 missions |
| 37, 74 | entrent dans le compte +0x754 | `FUN_006ac5d0` |

`tools/missions.py --types` : 132 missions d'histoire, 64 cours, 100 boulots et
courses, 107 tutoriels, 70 mini-jeux.

## Ce qui lance une mission

- Côté scripts, la primitive est **`ForceStartMission(nom)`**. La bibliothèque de
  test `RunMissionLib` le montre : `RunMission(nom)` attend la fin du chargement
  (`IsStreamingBusy`), puis appelle `ForceStartMission(nom)` ; `Test1_01` fait
  `RunMission("1_01")`. Le jeu l'appelle aussi pour les missions de fin
  d'histoire (`MainMap` pour 5_07, `SGlFunc` pour 6_01 et 3_08).
- Côté banque, une mission devient lançable quand ses conditions sont vraies ; le
  joueur la démarre en entrant dans son marqueur (`MissionButtonTrigger`).
- **La première mission** : dans `chap1`, `1_01` est la seule mission sans aucune
  condition (`tools/missions.py | awk -F'\t' '$1=="chap1" && $4=="-"'`). Ce qui la
  démarre au début d'une nouvelle partie n'est pas encore lu dans le moteur.
- `SMissPass.F_LaunchingMission` prépare l'état du monde selon la mission lancée
  (`MissionGetIndex`, `1_01`, `1_02A`…) : à lire quand on branchera le lancement.

## Suite

1. Dans `lancer_lua`, brancher `ForceStartMission(nom)` pour de vrai : trouver la
   mission dans la table, charger son script (champ 28), appeler `MissionSetup`,
   lancer `main` comme un thread, `MissionCleanup` à la fin.
2. Lancer `1_01` ainsi et relever les fonctions du moteur qu'elle appelle.
3. Lire dans le moteur qui démarre `1_01` à la nouvelle partie, et le champ 36.
