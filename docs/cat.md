# Arbres d'actions compressés (`Act/Act.img`, `.cat`)

Les 479 fichiers `.cat` d'`Act/Act.img` décrivent le comportement des
personnages : une hiérarchie de nœuds (banques, nœuds jouables, feuilles,
références), des conditions qui gardent l'entrée dans chaque nœud, et des
pistes que joue un nœud (animations, sons, intentions…). Les 119 `.cat` de
`Stream/World.img` sont une autre famille (catalogues liés aux objets
animés, voir formats-restants.md).

## Chargement (bully.exe)

- `FUN_005f6f20` ouvre `Act\Act.IMG`, crée la banque racine « Global », puis
  charge la liste `Act\Globals.act`… (0xb0a4f0) par `FUN_005f6c50`, qui
  ajoute `.cat` au nom.
- `FUN_005fb3f0`, que le jeu nomme « CompressedActionTree::load() », lit
  les deux premiers mots (taille, fin de l'en-tête), charge l'en-tête dans
  un tampon de travail et le reste dans un bloc alloué, puis l'analyse :
- `FUN_005fb220` alloue les nœuds d'après les comptes de l'en-tête (banques
  0x20 octets, références 0x30, nœuds jouables 0x28, feuilles 0x18) ;
- `FUN_005fb1a0` lit une banque, `FUN_005fa9c0` un nœud jouable,
  `FUN_005fafb0` la liste des enfants, `FUN_005fa910` les conditions,
  `FUN_005fa8b0` les pistes.

Les noms passent par `HashString` (0x576d80) : la chaîne mise en
majuscules, `h = h * 0x83 + c`, gardé sur 31 bits.

## Format

En-tête, huit mots de 32 bits :

| | |
|---|---|
| +0 | taille du fichier |
| +4 | fin de l'en-tête = début des données |
| +8 | début des chaînes |
| +0xc | fin de l'arbre |
| +0x10 | nombre de banques (hors racine) |
| +0x14 | nombre de nœuds jouables, feuilles comprises |
| +0x18 | nombre de références |
| +0x1c | nombre de feuilles |

Puis deux tables de renvois, même forme : un compte (mot), et pour chaque
entrée une valeur (mot ; dans la première, un décalage dans les chaînes),
un compte k (16 bits) et k décalages des données qui la citent. Le moteur
s'en sert pour rebrancher les chaînes dans les enregistrements.

Puis l'arbre, octet par octet, à partir du caractère `b` de la racine :

- un nœud : son nom (mot ; bit de poids fort à 1 = hachage du nom, sinon
  décalage dans les chaînes), un octet n et n conditions (décalages dans
  les données), pour un nœud jouable un octet m et m pistes, puis ses
  enfants ;
- les enfants : un compte (16 bits), puis pour chacun un caractère, `b`
  banque, `n` nœud jouable, `l` feuille, `i` ou `r` référence (deux
  décalages dans les chaînes : le nom, le chemin du nœud visé), suivi du
  nœud.

Une condition, dans les données, commence par le hachage du nom de sa
classe, que le moteur cherche dans un registre de fabriques
(`FUN_0061a4f0`) : `Not`, `Or`, `ConditionGroup`, `HavePOIOfType`,
`IsSocializing`, `pedType`, `RangeToTarget`, `WeaponModelRequest`…

## Les groupes de conditions (22.10.2026)

La seconde table de renvois ne cite pas des chaînes : sa valeur est un décalage **après la fin de l'arbre**
(mot +0xc de l'en-tête), là où le fichier range des **listes de conditions** écrites comme celle d'un nœud
(un octet n, puis n mots : des décalages dans les données). Le champ +4 d'une `ConditionGroup` (le mot +8
de son enregistrement, l'objet étant construit à +4) y renvoie : c'est le groupe, évalué comme une liste
(`NOT` / `OR` compris). Les 3 220 valeurs des secondes tables des 479 fichiers se lisent toutes ainsi.
Exemple, `/TrainingPlayer/Default/Locomotion/animations` : `ConditionGroup(WeaponEquiped Or WeaponEquiped
Or …)`, six armes en `OR`. `TargetConditionGroup` porte son groupe de la même façon. `CActionTreeFile::Groupe`,
`GroupeDe` ; `tools/cat.py` affiche le groupe entre parenthèses.

## Les conditions jugées et le contrôleur de l'outil (22.10.2026)

- **Les données restent chargées** : `CActionBank` garde chaque fichier (`sources`), et chaque nœud sait d'où il
  vient (`CActionNode::source`) ; ses conditions se lisent après la greffe sous `/Global`.
- **Les inclusions** `i` sont remplacées par l'arbre de leur fichier (`CActionBank::Inclure`) : la banque
  `actions` de `GlobalActions` reçoit `Grapples`, `RisingAttacks`, `PropAttacks`… ; les références `r` restent
  des liens (`GlobalLoco` depuis `TrainingPlayer`), que la descente ne suit pas.
- **La liste de conditions** de `FUN_005fb9b0` (`src/core/ConditionListe.h`, `EvaluerListe`) sert aux missions, aux
  nœuds et aux groupes : `NOT` inverse la suivante ; après un faux, un `OR` laisse la suivante décider ; après un
  vrai, la suivante est encore jugée mais ne compte pas ; le premier faux sans `OR` derrière rend la liste fausse.
  `CActionBank::Juger` fait `NOT`, `OR`, `False` et `ConditionGroup`, et confie le reste à un `CJugeAction`.
- **Champs relevés** (mots de l'enregistrement après le type ; l'objet est construit à +4) :

| Classe | Champs |
|---|---|
| `ConditionGroup`, `TargetConditionGroup` | +4 : renvoi de la seconde table vers le groupe |
| `WeaponEquiped` | +4 : le nom de l'arme, cité par la première table (`unarmed`, `any`, `bat`, `leadpipe`…) |
| `ActionRequest` | (`ConditionActionRequestButton`, `FUN_005fd650`) +4 : le numéro de l'action (0x16, 3, 1…), +8 : le mode (3 : bouton lâché, `(état & 3) == 0` ; 2 : `état & 3` ; sinon `état & 1`), +0xc / +0x10 : un seuil et son comparateur sur la durée tenue (`FUN_005fbc30`), +0x14 : si non nul, seule la durée compte |
| `ActionNodePlayTime` | (`FUN_005fb940`) +4 : un temps (s), +8 : le comparateur de `FUN_005fb750` sur le temps du nœud ; deux champs |
| `TargetPlaying` | +4 : un nom de nœud haché (`RollAway`…), +8 : 1 ; deux champs |
| `LuaCond` | +0x10 : `mission`, +0x14 : la fonction du script (`F_PlayerGroundKicks`, `F_FinalAttacks`…) |

  Le comparateur des conditions à seuil (`FUN_005fb750`, 22.10) : 0 `≈`, 1 `≉`, 2 `<`, 3 `>`, 4 `<` ou `≈`, 5 `>` ou
  `≈`, `≈` étant `|a − b| ≤ 1e-5` (`FUN_00432910`, `DAT_008ff230`) ; `ComparerCondition`, `EtatBoutonCondition`
  (`src/core/ConditionListe.h`).
- **Le contrôleur** (`src/core/ActionController.h`) : l'arbre posé, le nœud joué, son départ et sa fin (le plus tard
  des entrée +12 + durée de vie +16 des pistes). Entrer dans un nœud (`PedSetActionNode`, l'emplacement +0x4c que
  `FUN_004849a0` appelle) descend dans une banque. Les passages viennent des pistes (section suivante) ; la règle de
  l'outil (un enfant qui aboutit, sinon la descente depuis l'arbre posé) ne sert plus qu'en secours.

## Les passages : Opportunity, Sequence, PlayOnTarget (22.10.2026, troisième séance)

- **`Opportunity`** (table 0x938074, mise à jour `FUN_005f7d70`, arrêt `FUN_005f7440`) : chaque image, une piste pas
  encore prise tente sa chance : le tirage si sa période (+0x38) dépasse 1 (`FUN_00465910` : le plus petit bit posé de
  (mot +0x10c du piéton + compteur d'images `DAT_00c1a99c`) mod 0xffff + 1 doit atteindre la période, une chance sur
  la puissance de deux au-dessus), ses conditions propres (+0x3c, aucune dans les 68 arbres du démarrage), puis le
  nœud visé (+0x20, relatif au nœud qui porte la piste) et la descente dans ce nœud (+0x4c) : prise si elle aboutit à
  un autre nœud que le nœud joué, ou au même quand l'octet +0x30 le permet ; sans chemin, prise sans nœud. Pas prise,
  elle s'arrête quand le temps du nœud dépasse +0x28 (s'il est positif) ; prise, quand il atteint +0x2c (s'il est ≥ 0).
  À l'arrêt, une piste prise dépose sa demande au contrôleur (`FUN_005f3bc0`) : la priorité +0x34 la plus forte
  l'emporte, à égalité la première déposée. Relevé : 3 080 `Opportunity` dans les arbres du démarrage, 225 à période.
- **`OpportunityRandomLatch`** (lancement `FUN_005f7510`, mise à jour `FUN_005f7f80`) : la même, mais la bascule
  (+0x54) est tirée au lancement entre +0x4c et +0x50 par le `rand()` du CRT (`RAND_MAX` 32767, `0x900ce8`).
- **`Sequence`** (arrêt `FUN_005f7030`) : à la fin du nœud, la descente dans le nœud visé dépose une demande de
  priorité 0 ; la mise à jour (`FUN_00609b30`) ne finit jamais d'elle-même. Le nœud visé (`FUN_005f4e30`) n'est
  pas limité à l'arbre posé : la poussée de la prise (`PushFwd/RCV`) envoie la cible dans
  `HitTree/Standing/PostHit/BellyUp/OnGroundBounce`, puis `BellyUp/On_Ground` (22.10.2026, quatrième séance).
- **`PlayOnTarget`** (`ExecuteOnTargetTrack`, mise à jour `FUN_006615f0`) : à son entrée, la cible du piéton
  (`FUN_004d5980`, un piéton : `(+0x108 & 7) == 3`) joue le nœud visé (`FUN_005f4500`), une fois.
- **Ce que l'outil n'a pas** : la pile des nœuds joués du contrôleur (+0x44, +0x48 : les parents jouables restent
  actifs) et les références montées (`GlobalLoco` sous `TrainingPlayer`). D'où deux garde-fous dits : une
  `Opportunity` qui vise un nœud contenant déjà le nœud joué (une remontée dans la pile) ou un nœud hors de l'arbre
  posé ne fait pas passer (une `Sequence`, si). Une demande sans nœud (piste sans chemin) ne fait rien. Supposé : la priorité de départ
  d'une image est −1 (la plus faible priorité relevée est 0).

## Les coups : Collision et la réaction de la cible (22.10.2026, quatrième séance)

- **`CollisionTrack`** (table 0x92decc ; fabrique `FUN_0064a6a0`, entrée « Collision » de la table de `FUN_00650150` à
  0x6507c8 ; lancement `FUN_005f9480`, mise à jour `FUN_005f9e70`, arrêt `FUN_005f8240`). Champs relevés :
  +0x18 (24) et +0x1c (28) la fenêtre du coup dans le temps du nœud ; +0x20 (32) le son (`'KickHeavy'`) ;
  +0x2c (44) le **drapeau du coup** (`'GROUNDHITHEAVY'`, `'FRIENDLYHIT'`), passé à la cible ; +0x34 (52) les dégâts ;
  +0x68 (104) un nœud de l'attaquant, joué si le coup porte sur un piéton (`FUN_005f4490`, ou `FUN_005f3e20` selon
  l'octet +0x217) ; +0x78 (120) un second chemin, non suivi. Dans la fenêtre, le moteur teste les sphères des os de
  l'attaquant contre les piétons proches (`FUN_005f9cf0`) ; l'outil n'a pas les os : le coup touche la cible verrouillée.
- **Le coup reçu** : la méthode du slot 15 de `CPed` (`FUN_0047b810`, 19 paramètres : dégâts, attaquant, position,
  nœud de coup, drapeau…). Sans nœud de coup donné, le contrôleur de la cible joue son nœud de coup (+0x4b8 du
  contrôleur, descente +0x4c puis `FUN_005f4960`) ; dans l'outil, `/Global/HitTree`.
- **Conditions de la réaction** (enregistrements lus dans HitTree.cat) : `ConditionFlag` (+4 un nom haché : vrai si
  c'est le drapeau du coup reçu), `MePlaying` (+4 un nom de nœud, +8 = 1 : le nœud joué par le piéton lui-même ou un
  parent), `HitLocation` (+4, +8 la zone touchée ; l'outil, sans sphères, la suppose vraie pendant un coup).
- **Un piéton à terre** : `HitTree/BASE/HitTreeBase/…/On_Ground` (« si MePlaying… ») porte l'Opportunity qui mène à
  `GroundAndWallHits/On_Ground` ; là, `BellyUp/GROUNDHITHEAVY` est pris si `ConditionFlag` = GROUNDHITHEAVY. Le moteur
  y arrive par un `Execute` des nœuds de coup (contrôleur secondaire) ; l'outil, qui ne joue pas les `Execute` en
  contrôleur secondaire, y entre directement quand la cible joue un nœud `On_Ground` (supposé).
- **Les `Execute`** : le contrôleur de l'outil relève les pistes `Collision` du nœud qu'un `Execute` fait jouer
  (`HeavyAttacks/…/GroundKick` → `./Collision` → la feuille `ped`) ; le reste d'un nœud exécuté n'est pas joué.
  (Levé le 23.10 : voir la section suivante, un `Execute` n'a pas de contrôleur à part.)
- **Supposé, dit** : le corps projeté par une prise (`PushFwd/RCV`, `FRIENDLYHIT`) ne touche pas celui qui le pousse
  (l'outil écarte le partenaire du `PlayOnTarget` pendant deux secondes) ; le drapeau du coup tient une seconde.

## Les dégâts, la santé et l'humiliation (23.10.2026)

- **Un `Execute` n'a pas de contrôleur à part.** Son lancement (`FUN_005f7070`, table `ExecuteTrack` 0x937fe4) fait
  descendre le nœud visé (+0x4c, avec le contexte du piéton, donc jugé) puis `FUN_005f3e20` ajoute les pistes du nœud
  obtenu à la liste du contrôleur, celle du nœud joué ; `FUN_005f3fb0` lance chaque piste quand le temps du nœud atteint
  son entrée (+12), les entrées négatives à l'arrêt du nœud. Une durée de vie positive (+0x10) fait arrêter ces pistes
  avec l'`Execute` (`FUN_005f7150`). L'outil garde, des nœuds exécutés, les `Opportunity`, `Collision`, `DamageHandle`,
  `SetHealth`, `SetPedFlags`, la `Sequence` (gardée pour la fin du nœud joué) et leur fin (la fin du nœud joué est la
  plus tardive de toutes). Ainsi une feuille de coup reçu (`Standing/Melee/…`) exécute `Base/HitBase`, dont la
  `DamageHandle` porte les dégâts et l'`Opportunity` mène à `KnockOuts`.
- **Le relevé du coup** (+0x408 du contrôleur, `PedGetLastHitWeapon` 0x474a90 le rend ; contexte +0x10) : +0x40c
  l'attaquant, +0x434 le temps depuis le coup (remis à 0 par `FUN_00472b70`, l'ancienne valeur passant en +0x438, puis
  augmenté du pas à chaque image par `FUN_004850b0` tant qu'il ne vaut pas -1), +0x440 les dégâts, +0x46c le drapeau
  haché. La méthode du coup (`FUN_0047b810`) calcule les dégâts : ceux de la piste `Collision` (+0x34 ; +0x50 pour un
  objet), multipliés pour un coup du joueur sans arme par `PlayerButes` +0xac / +0xb0 / +0xb4 selon son état physique
  (`DAT_00c1af4c`, `PlayerGetPhysicalState` ; 1, 1, 1 dans `Globals.cat`), puis arrondis au-dessus après le facteur
  +0x1cd4 de l'attaquant (`FUN_0085dbc0`) et au-dessous après le facteur +0x1cfc de la cible (`FUN_0085d540`), à 1
  depuis le constructeur `FUN_0047ffe0`.
- **`DamageHandle`** (`HandleDamageTrack` 0x9313e4, lancement `FUN_0060fc70`) : le piéton subit les dégâts du relevé
  multipliés par +0x18 (24), portés par l'attaquant du relevé, par `FUN_0047b7f0` → `FUN_0047b6b0` → `FUN_004782a0`
  (invulnérabilités, santé minimale) ; l'octet +0x1c (28) ajoute un effet de coup (`FUN_0059b750`). Facteurs relevés :
  0 en parade, 0,1 et 0,2 contre un mur, 1 (`Base/HitBase`), 0,35 pour le joueur (`IsPlayer`), 2 et 2,5 au lance-pierre.
- **`SetHealth`** (`SetHealthTrack` 0x9350b0, lancement `FUN_00627500`) : la valeur +0x1c pose la santé (+0x18,
  `FUN_00478190`, pas sous la santé minimale), la rend (+0x19, `FUN_00474b70`, jusqu'au maximum) ou l'ôte (+0x1a,
  `FUN_004782a0`, sans les invulnérabilités). `HumiliateFlee` d'une victime de mission ôte 500.
- **`SetPedFlags`** (`PedFlagsTrack` 0x931380, lancement `FUN_0060f8e0`) : le drapeau +0x1c du piéton (+0x1314 + 4n)
  reçoit l'octet +0x18. Trois drapeaux servent aux coups (`FUN_0047b6b0`) : 12 (+0x1344) et 30 (+0x138c,
  `PedSetInvulnerable`) écartent tout coup, 20 (+0x1364, `PedSetInvulnerableToPlayer`) ceux du joueur.
- **`SetTargetAsGrappleTarget`** (lancement `FUN_0062a580`) : le piéton et sa cible deviennent partenaires
  (`PedSetGrappleTarget`, +0x1500 = l'octet +0x18), le piéton descend dans le nœud +0x24 (36), la cible dans +0x34 (52),
  chacun le joue (`FUN_005f4490`) sauf si l'octet +0x19 / +0x1a demande de n'en exécuter que les pistes.
- **`HitAnimation`** (0x934cb8, une `AnimationTrack`) : sa mise à jour `FUN_00617040` passe d'abord par `FUN_0060ae00`,
  la même fin qu'une `Animation` (vitesse 1, faute du champ 48).
- **L'arrêt d'une `Opportunity`** (`FUN_005f7440`) : quand le nœud s'arrête, une piste prise et pas encore arrêtée
  (bascule -1) dépose sa demande ; la plus forte priorité l'emporte (`FUN_005f3bc0`).
- **Conditions lues** :

| Classe | Fonction | Ce qu'elle juge |
|---|---|---|
| `HitTime` | `FUN_005fc400` | +0x434 (ou +0x438 si l'octet +0xc) du relevé comparé à +4 par +8 (`Standing/Melee` : ≤ 0, l'instant du coup) |
| `stat` | 0x5ffff0 (hors de l'index) | la colonne +0xc du piéton (`GameGetPedStat`) ≥ +4 si l'octet +8 est nul ; l'octet +9 ajoute un `MePlaying` |
| `IsPlayer` | `FUN_005ffe40` | le type du piéton (+0x1310) vaut 13 |
| `PedFightingSystem` | `FUN_005fd740` | +4 égal à +0x16dc (`PedSetFightingSystem` 0x5ca590) |
| `Health`, `PercentageHealth` | `FUN_005fd8f0`, `FUN_005fd930` | la santé (sur le maximum +0x1ca4) comparée à +4 par +8 |
| `DamagePending` | `FUN_005fbf00` | les dégâts du relevé comparés à +4 par +8 |
| `IsHealthAtHumiliationLevel` | `FUN_00601a90` | santé / maximum sous la colonne BM / 100 (le bully de 1_02 : 15 %) ; la voie de la peur n'est pas suivie |
| `IsMissionSpecific` | `FUN_005ffb70` | l'octet +0x1570 vaut 2 : un piéton créé par un script (`FUN_0046dea0(modèle, 2)`) |
| `CharacterSize` | `FUN_00601460` | +0x74 du modelinfo, la colonne SIZE de l'IDE rangée dans Small, Medium, Large, Fat, Huge (`FUN_00488a20`) |
| `PedFlags` | `FUN_005fd700` | le drapeau +4 non nul (non jugée par l'outil, voir plus bas) |
| `HumiliationGroupIs` | `FUN_006039b0` | +0xd44 du piéton égal à +4 (non jugée : aucune écriture directe de +0xd44, deux lectures, 0x490f23 et 0x6039b7) |

- **Garde-fous de l'outil desserrés** : une `Opportunity` hors de l'arbre posé passe si le nœud visé est dans l'arbre
  global de son porteur (un piéton tombé dans `HitTree` s'en relève : `BellyUp/On_Ground` → `../BellyUpGetUp`), ou si
  son chemin remonte jusqu'à `/Global` (`HumiliateFlee/…/RunStart` → `HitTree/KnockOuts/Rear/Rear`).
- **Ce qui reste supposé ou non lu** : `PedFlags` n'est pas jugée (la charge de la prise prendrait `NOT PedFlags` vers
  `ExecuteNodes/StanceTransition/Give`, un chemin à cinq `..` qui ne se résout pas sans les références montées) ;
  `HumiliationGroupIs`, `AnimationGroupLoaded`, `IsSpeechPreloaded`, `TargetRelativePosition` ne le sont pas : dans
  `Humiliate_Me`, le bully tombe dans la branche `Fail` ; l'effet de coup, le cas de `FUN_004255b0` (la cible tombe d'un
  coup du joueur) et `FUN_00426560` (qui change les facteurs +0x1cd4 / +0x1cfc) ne sont pas suivis.

## Le chemin d'un nœud (18.10.2026)

Les scripts désignent un nœud par un chemin, `PedSetActionNode(ped, "/Global/1_01/Talking",
"Act/Conv/1_01.act")`. Dans le moteur :

- `FUN_005f6f20` crée la banque **`Global`** sous une racine sans nom (`DAT_01b649dc`, celle que
  `/` seul désigne) ; `FUN_005f6c50("Act/Conv/1_01.act")` retire un `/` de tête, écrit `.cat` à
  la place de l'extension, charge le fichier (`FUN_005fb3f0`) et greffe sa banque de tête sous
  `Global` (vtable +0x18 de la racine) : `1_01` devient `/Global/1_01`. `FUN_005dbe60` dit si un
  fichier est déjà chargé, par le hachage de son nom sans dossier.
- `FUN_005f59b0(chemin)` résout : depuis la racine si le chemin commence par `/` ou `\`, un
  segment à la fois (`FUN_0061a250`), `..` remontant au parent (vtable +0x34), et sinon l'enfant
  (vtable +0x2c compte, +0x30 enfant i, +8 nom) dont le nom haché est `HashString(segment)` ; un
  chemin sans `/` de tête passe par un registre de noms (vtable +0xc de la racine), pas lu.
- `FUN_004849a0(ped, chemin, fichier)`, ce que `PedSetActionNode` (0x5ca0d0 → `FUN_00473fa0`)
  appelle : le nœud, sinon le fichier chargé puis le nœud retrouvé ; vrai s'il existe, et le
  piéton le joue (vtable +0x4c, `FUN_005f4490`). Le script Lua reçoit ce vrai ou faux.

`src/core/ActionBank` (`CActionBank` : `Charger`, `EstCharge`, `Trouver`, `TrouverOuCharger`)
refait ces quatre fonctions sur `CActionTreeFile` ; `tests/test_actionbank` vérifie les chemins
que `1_01.lua` demande (`/Global/1_01/Talking`, `ShortIdle`, `Gates/BarUpHold`…), `..`, les deux
séparateurs, la casse, et le chargement à la demande de `Player.act` puis `1_02.act` sous la même
racine. Dans `lancer_lua`, `PedSetActionNode` répond pour de vrai (le nœud n'est pas joué).

## Les pistes

Le nœud ne garde que des pointeurs ; la piste est créée à la demande
(`FUN_005f6100`, emplacement 17 de la table virtuelle) et remplie par
`FUN_005fa7e0` :

- un mot de base de 16 bits ; s'il n'est pas nul, la piste hérite de celle
  placée **base octets plus loin** dans le fichier (`ADD ECX, ESI` dans
  l'assembleur : le modèle est copié d'abord, puis écrasé) ;
- des attributs, chacun un mot w de 16 bits suivi de sa valeur : taille
  `1 << ((w >> 1) & 3)` octets (1, 2, 4 ou 8), position `w >> 3` dans
  l'objet, un autre attribut suit tant que `w & 1`.

La valeur en position 0 est le type : le hachage du nom de classe de la
piste, cherché dans le registre de fabriques (`FUN_0061a4b0`). Relevé :
**51 001 pistes, toutes typées** (par héritage pour 36 464), 398 types, tous
nommés : `Animation` (8 166), `Opportunity` (6 647), `Execute`, `SoundFX`,
`Sequence`, `SetPedFlags`, `Spawn`, `DialogLine`, `Target`, `JointDriver`,
`LocomotionAnimationNew`… Les chaînes (chemins de nœuds : `./SitDown`,
`../../../../AIActionOpps/SpectatorOpps`) se retrouvent par la table de
renvois : l'attribut placé au décalage cité.

Une piste `Animation` porte en position 24 **`HashString("GROUPE\NOM")`**
de l'animation (`C_PLAYER\PUNCH_SLOP_1` = 0x6158a6b0 : le groupe est le
fichier `.agr`), en 28 un masque d'os (`UpperBody_All`, `UBO_Arms_Hd_Sp`…),
puis des réglages (fondus, vitesse, boucle). Les noms « GROUPE\NOM » sont
écrits en clair dans `Anim/MAINPED.HXD` et les autres `.HXD` (2 889 dans
MAINPED, chacun suivi de son hachage). Le format des `.HXD` et le passage du nom à
l'animation du `.agr` sont dans docs/hxd.md ; `tools/cat.py --pistes`
affiche « [RAT_PED.agr n° 0] » à côté de chaque animation.

Les autres champs d'une piste `Animation`, lus dans `AnimationTrack`
(table virtuelle : lancement `FUN_00612360`, mise à jour `FUN_0060ae00`,
arrêt `FUN_0060af80`) ; la position d'un attribut est son décalage dans
l'objet :

| Position | Sens |
|---|---|
| 12 | (toutes les pistes) instant d'entrée en jeu dans le nœud (s) |
| 16 | (toutes les pistes) durée de vie dans le nœud (s) ; −1 : sans limite. Une boucle à 16 = 2,27 finit le nœud à 2,27 s |
| 24 | HashString de l'animation |
| 28 | masque d'os (`FUN_00607340`) |
| 32 | mode : 2 = boucle ; sinon la piste finit avec l'animation (1 : appelle aussi `FUN_006c0c60` à la fin) |
| 36 | couche du lecteur (0 à 6) |
| 40 | instant de départ (s) |
| 44 | instant de fin (s) ; −1 ou au-delà : la durée du HXD |
| | La fin de la piste (mise à jour `FUN_0060ae00`, 22.10.2026) : en mode 0, la piste s'arrête quand le temps joué, parti de 40 et avancé à la vitesse 48, dépasse 44 (ou la durée de l'animation, HXD +0xc) ; les modes 1 et 2 ne finissent pas d'eux-mêmes. Le contrôleur de l'outil finit un nœud au plus tard de ses pistes qui finissent (durée de vie 16, ou fin d'une animation en mode 0), avec les durées de MAINPED.HXD |
| 48 | vitesse ; multipliée par la stat 0x14 du piéton / 100 |
| 52 | fondu (s) ; 0 : sans fondu ; −1 : celui du HXD |
| 56 | passé à `FUN_006bd320` à l'arrêt |
| 60 | octet : pose un drapeau du piéton (+0xe4) le temps de la piste |
| 61 | octet : la vitesse suit le déplacement réel (synchro de la marche) |

## Un exécuteur simplifié

`outils/arbres.h` déroule un arbre pour un piéton de la visite, au lieu des
programmes écrits à la main. C'est une approximation, avec ces règles :

- chemins `.`, `..` et noms hachés ; une cible hors du sous-arbre est ignorée ;
- piste Animation : champs 16 (durée de vie dans le nœud, même en boucle) à 52 ;
  mode 1 : la dernière pose reste figée ;
- fin d'une animation : `sequence` ; sinon un nœud jouable qui offre des
  occasions se rejoue (état d'attente), un autre revient au nœud jouable ancêtre
  le plus proche, à défaut au départ (un banc choisit un enfant une fois, il ne
  rejoue pas ses frères) ; un nœud qui lâche l'objet ressort au-dessus de celui
  qui l'avait pris ;
- `Opportunity` : ouverte de max(12, 44) à 16 (44 : l'instant au plus tôt ; la
  reprise « ./ » de `Sit_Smoke_Idle` a 44 = 3,33, la fin de son animation),
  prise au hasard (8 % par seconde) si les conditions du nœud visé passent ;
  champ 8 (un octet) à 1 : prise dès qu'elle s'ouvre ; elle peut viser le nœud
  lui-même (reprise) ; `OpportunityRandomLatch` : à coup
  sûr, à un instant tiré entre ses champs 76 et 80 ;
- `Execute` dans un nœud sans animation : un saut ;
- une branche qui ne joue aucune animation (nettoyage, `ReleaseGroup`) n'est
  choisie qu'à défaut ; un piéton resté sur un nœud vide reprend au départ ;
- conditions : `WeightedRandom` et `Random` passent ; le tirage se fait parmi
  les sœurs qui passent (une variante est choisie, pas chacune à 50 %) ; `IsFemale` suit le
  piéton ; `ActionRequest`,
  `IsScriptedAmbient`, `false`, `IsPlayer`, `IsAuthority`, `PedModelID`,
  `Health`, `DamagePending`, `HitTime`, `PropTargetInteractive`,
  `TargetRelativeOrientation`, `OBJECTIVE` sont fausses ; `Not` inverse la suivante ;
  `OR` : une au moins ; les autres passent ;
- `HoldState` : le nœud garde l'animation et la pose d'avant (l'état au début
  de la chaîne d'entrées, pas celui d'un banc traversé) ; ses occasions et sa
  `sequence` tournent ;
- `PlayOnTarget` envoie le partenaire ; le nœud visé est passif : on n'y entre
  que sur ordre, on n'en sort que par sa `sequence`. Le rôle passe avec l'ordre :
  qui l'envoie mène, qui le reçoit suit ; un suiveur fige sa pose à la fin de
  son animation et, envoyé vers un nœud sans animation, en applique les effets
  sans quitter le sien. Dans un nœud animé, un `Execute` fait jouer au
  partenaire le `PlayOnTarget` de la feuille visée (`./TargetOrientation`) ;
- `TargetSync` (placement relatif des deux piétons) n'est pas géré : les paires
  partagent leur origine, les décalages de bassin des animations font le reste
  (voir ci-dessous).

### TargetSync

Champs : 12 et 16 (fenêtre), puis **24, 28, 32 = décalage x, y, z (m) et
36 = angle (rad)**. Prise GRAP_INIT et couple `Hold` : y = 0,9, angle π ;
vol d'arme `Steal_Easy` : y = 0,7, angle π ; `Cuddle` : x = −0,4, angle 0.

**Ce que fait le moteur (établi le 07.10.2026).** `SyncBaseTrack` a des
méthodes vides, mais `TargetSyncTrack` (vtable 0x9352ac) a sa propre méthode
d'exécution, `FUN_0060f350` (troisième entrée), lue dans l'index puis
confirmée à l'assembleur par REA :

1. le piéton qui joue la piste (le meneur) lit sa cible, `ped + 0x155c` ;
   sans cible, rien ;
2. il construit la matrice M = rotation de l'angle autour de z
   (`FUN_00412b90`), translation (x, y, z) ;
3. matrice de la cible = matrice du meneur × M (`GetMatrix` 0x46dbd0 sur le
   meneur, produit 0x413780, `SetMatrix` 0x46dd40 sur la cible) : la cible
   est posée **dans le repère du meneur**, au décalage (x, y, z), tournée
   de l'angle ;
4. cap de la cible (`+0x1700`) = cap du meneur + angle, ramené dans
   l'intervalle (0x432d90, 0x473370).

Le repère du meneur est celui de l'entité (sa position au sol, qui suit la
flèche), pas le bassin animé. Un piéton regarde vers +y : y = 0,9 et angle π,
c'est « la cible à 0,9 m devant, face au meneur » ; x = −0,4 et angle 0,
« à 0,4 m sur le côté, même sens ».

Ce que disent les
animations, à t = 0 :

| Animation | Flèche (ARROW) | Bassin |
|---|---|---|
| Grap n° 23 `GRAP_INIT_GV` | (−0,03 ; 0,22) | 0,22 devant |
| Grap n° 22 `GRAP_INIT_RCV` | (0,05 ; 1,18) | 1,17 devant |
| C_Player n° 366 `GRB_STEAL_ATT` | aucune | à l'origine |
| C_Player n° 365 `GRB_STEAL_VIC` | (0 ; 0) | à l'origine |

Les paires Grap portent leur écart dans la flèche (0,96 m, presque les 0,9 de
TargetSync) : poser les deux piétons à la même origine revient au même que
poser chacun sur sa flèche et la cible à 0,9 m, face au meneur. Le vol d'arme
n'a pas d'écart : seul TargetSync sépare les deux piétons. Modèle du moteur, confirmé par `FUN_0060f350` : chaque piéton suit sa
flèche, TargetSync pose la cible par rapport au meneur.

**Ce que ça change pour la visite (mesuré le 07.10.2026, `sonde_poi` affiche
désormais la flèche à t = 0 et son cap).** Les animations d'empoignade portent
la cible sur la flèche de la victime, exactement là où TargetSync la pose :
GRAP_IDLE_RCV (Grap n° 6) a sa flèche en (0,08 ; 0,96), cap 180° ;
GRAP_INIT_RCV (n° 22) en (0,05 ; 1,18) ; LOCKER_HOLD_RCV (POI_Gen n° 4) en
(0,06 ; 0,44) ; GFIGHT_CYC_GV (Gfight n° 1) en (0,06 ; 0,29), cap 180°. Les
poser à la même origine revient donc au modèle du moteur, à quelques
centimètres près : **rien à changer** pour les prises, les bagarres, Wall_Hold,
LockerStuff et Swirlie. Seules les animations **sans position de flèche**
dépendent de TargetSync : le sermon (`Tell_Off`, 0,9 m, π), le harcèlement
(`BookHarass`, 0,9 m, π) et le bras dessus bras dessous (`Cuddle`, x = −0,4)
l'appliquent. Le baiser (`NPC_Ambient.cat` `Hold`, 0,9 m, π) a été essayé
avec la règle exacte : les têtes de deux élèves se rentrent dedans (les
animations sont faites pour un squelette adulte) ; il garde son recul réglé
à l'image (bassins à 35 cm). Reste hors visite : le vol d'arme
(`Steal_Easy`, 0,7 m, π), qui suppose une arme sur la cible.


Pilotés par un arbre :

| Point | Arbre | Sous-arbre, départ |
|---|---|---|
| fumeur au mur | Ambient.cat | `Wall_Smoke`, `Wall_Start` |
| fumeur de classe (`F_ClassSmokers`) | Ambient.cat | `Wall_Smoke`, comme le dit `EventFunc.lur` (5_02.cat `StandingSmoke` est un arbre de mission) |
| couple | NPC_Ambient.cat | banc de `Hold` ; le garçon part de `Hold`, la fille suit |
| bagarre de filles | Grapples.cat | banc de `GirlFight_Init` : Init → Loop (5 à 10 s) → Out |
| bagarre au sol | Grapples.cat | `mount`, `MountIdle/Give` : `MountOpps` choisit le coup selon la clique (`FacePunch`, `KneeDrop`, `Headbutt`, `Dismount`…) |
| place assise | Ambient.cat | `Sitting_Down/SitHigh` : s'asseoir (`Sit_Start`), attendre, fumer ou discuter assis, se relever ; variante des filles (`IsFemale`) |

Place assise : le même arbre mêle deux conventions d'animation. Celles de
Sitting_Boys (gestes assis, fumer) ont le bassin à l'origine, orienté comme
debout : l'origine est le siège. Celles de C_Player et F_Girls (`SIT_DOWN`,
`SIT_IDLE_NPC_BENCH`, `SIT_LOOP_G`…) partent debout devant le banc, face à
lui, et finissent assises le bassin à (−0,08 ; 0,53) ou (0 ; 0,59) de
l'origine, tourné de 180°. La visite garde les deux placements et prend celui
du groupe de l'animation jouée ; le bassin de référence suit aussi le groupe
(1,10 m pour F_Girls et Gfight, 0,86 m sinon). Une fille prend la variante
`IsFemale` quand elle existe.

| spectateur | Ambient.cat | banc de `SpectatorAnims`, départ `SpecLike` : une réaction `GEN_IMPRESSED` tirée au hasard, puis `SpectatorWait/SpectatorLikeWait` (pose tenue, `HoldState`) qui relance `SpecLike` au bout de 1,5 s |
| groupe qui discute | Talking.cat | `Talking` : une branche par gabarit (`CharacterSize`, faux ici), clique et sexe ; attente `Load` puis geste de parole tiré au hasard (`S_NERD`, `SGIRLS`, `S_GEN`, `S_FAT`, `CHAT`…) |
| harcèlement | Ambient.cat | `BookHarass/GrappleAttempt/GrappleSuccess` : le caïd tient les livres hors de portée (`BOOKTEASE_GIV`), la victime (`Harrassed`, `BOOKTEASE_RCV`) tente de les attraper ; fin `*_END`. Les animations ont le bassin à l'origine et pas de flèche : les deux piétons sont posés face à face à 0,9 m (la TargetSync de la piste) |
| plaqué au mur (événement `F_HeldAgainstWall`) | Ambient.cat | `Wall_Hold/GrappleAttempt/GrappleSuccess/Hold_Idle` : le meneur (`Harassing`, `NPC_GENERIC\LOCKER_HOLD_GV`, POI_Gen n° 7) soulève sa victime (`Harassed`, `LOCKER_HOLD_RCV`, n° 4) contre le mur. Même origine pour les deux (bassin de la victime à 0,44 m devant, tourné de 180°, à 1,26 m de haut). Le lacet du point est celui de la victime, dos au mur : la paire est tournée d'un demi-tour (vérifié à l'image, cour de l'école, point (480,2 ; −70,6)) |
| fourré au casier (événement `F_LockerStuff`) | Ambient.cat | `LockerStuff/…/StuffGrap` : la prise debout déjà jouée pour les bagarres (`GRAP_IDLE_GV` + `_RCV`, Grap n° 7 et 6, même origine). Approximation : dans le jeu la scène finit dans un casier (`PropTargetInteractive`, l'objet casier interactif), qu'on ne pose pas ; on ne joue que l'empoignade. Vérifié à l'image dans le couloir de l'école (ischool, point (−648,1 ; −307,4)) |
| sermon (événement `F_TeacherHarassingKids`) | Ambient.cat | `Tell_Off/GrappleAttempt/GrappleSuccess/Pull_In_heavy/HOLD_IDLE` (HOLD_IDLE un cran plus bas que dans `Wall_Hold`, sous `Pull_In_heavy`, nom résolu du banc #0df0cd97) : l'adulte (`Give`, `NPC_GENERIC\CHEW_LOOP_T`, POI_Telloff n° 4) lance l'enfant (`RCV`, `CHEW_LOOP_P`, n° 3), deux boucles **1001** de 5,83 s. Leurs flèches sont nulles : pas d'origine commune comme les prises, on applique la TargetSync de `Hold` (0,9 m, angle π), face à face. Points : « Parent_harrasement » (GREASER, (577,9 ; −462,7) et (527,3 ; −464,3)), « ParentScold » (TOWNPERSON, (444,8 ; 234,3)). Vérifié à l'image (577,9 ; −462,7), 9 h : Peanut pointe le doigt sous le nez de Hal |
| tête dans les toilettes (événement `F_Swirlie`) | Ambient.cat | `Swirlie` : si la cible est l'objet toilettes (`PropTargetInteractive`), l'arbre joue une réplique et **exécute `LockerStuff/PropStuffTarget/StuffGrap`**, la même empoignade que `F_LockerStuff`. Même approximation : `GRAP_IDLE_GV` + `_RCV` (Grap n° 7 et 6, même origine), sans l'objet. Points : (−674,4 ; −334,5 ; 5,5) et (−583,7 ; −334,4 ; 0), lacet 270. Vérifié à l'image dans les toilettes de l'école (ischool), 12 h 30 |
| bras dessus, bras dessous (événement `F_CoupleCuddling`) | Ambient.cat | `Cuddle/GrappleAttempt/…/GIVE` : `AMB_ARMLINK_1` (NPC_Love n° 1) lance `Receive`, `AMB_ARMLINK_2` (n° 0) ; les deux sur place, bassin à l'origine. TargetSync x = −0,4 m, angle 0 : côte à côte dans le même sens, le second à la gauche du premier (vérifié à l'image, point (6,8 ; −113,1)). La sortie `Wait/BreakGive` est gardée par « Not IsScriptedAmbient » : pour une scène d'événement, IsScriptedAmbient est vraie et le couple reste enlacé |
| événement particulier | Ambient.cat | la scène que lance sa fonction d'événement dans `Scripts/EventFunc.lur` (`/Global/Ambient/Scripted/…`) : `F_Crying` → `Crying`, `F_DrunkenBeggar` → `Drunk`, `F_Cheerleading` → `Cheering`, `F_Workout` → `Workout`, `F_WallHangout` → `Wall_Lean`, `F_Sweep` → `SweepFloors`… (scènes à un piéton) ; `F_ClassSmokers` → `Wall_Smoke` ; F_Biker, F_Criminal… n'ont pas de nœud (code du moteur) : attente |
| prise debout | Ambient.cat | `LockerStuff/StuffGrap`, `GrappleSuccess/Pull_In_heavy/Give` (empoignade `GRAP_INIT`), puis `Hold_Idle` (`GRAP_IDLE`) |

Tout piéton dont l'arbre accroche un objet reçoit la cigarette (qui n'apparaît
qu'aux instants de l'arbre). Côté AI_POI, le nœud `Spectator` ne joue rien : il lance (`Play`)
`AIActionOpps/SpectatorOpps`, un arbre assemblé ailleurs ; les réactions
elles-mêmes sont dans Ambient.cat. Côté AI_POI, `Hangout` réserve une durée (piste `Hangout`, 26,7 à 33,3 s) et
lance `AIActionOpps/TalkingOpps` ; les gestes sont dans Talking.cat. Le banc
`HarassMoves` d'Ambient.cat, lui, contient les coups (pousser, coups de pied),
suivis d'une réaction gérée par le combat : pas une scène qui tourne.

Un suiveur ne prend pas d'occasion : il attend l'ordre de son partenaire (sinon
la victime abandonnait seule). Chaque piéton piloté par un arbre a sa propre
graine de hasard, où sa position se mêle (sinon deux groupes de même rang
faisaient les mêmes gestes au même instant).

Le vol d'arme (`GrappleOpps/Scripted/WeaponSteal`) est écarté : il suppose une
arme sur la cible, et ses animations (C_Player) se placent par `TargetSync`. `BULLY_TRACE=1` affiche les
derniers nœuds traversés par chaque piéton.

La visite (`outils/visite.cpp`) applique 40, 44 et 48 aux étapes de ses
programmes : au premier point d'intérêt, elle lit les pistes `Animation` des
479 arbres d'`Act.img` et garde, pour chaque animation, le réglage le plus
fréquent. Relevé sur 2 660 animations : `NPC_LOVE` boucle des baisers à 1,6,
fins à 2 ; `SMK_STND_SMKB` à 0,6 ; les cigarettes au mur à 1. La stat 0x14 du
piéton qui multiplie la vitesse n'est pas lue (100 supposé).

Les objets tenus : une piste `PropAttachEx` porte en 12 l'instant (en
secondes de la durée **du HXD**, pas du `.agr`), en 24 le modèle
(`cigarette`), en 28 le **HashString du point d'attache** de
`Models/Peds/MXDs.MGR` (`LeftCig` = 0x47c19c6a, `MouthCig`,
`RightCig` = 0x2654fed) et en 32 l'emplacement (`LeftHand`, `Mouth`,
`RightHand`) ; `PropDetachEx` porte en 24 le point à libérer. Pour
`Wall_Smoke` (Ambient.cat) : main gauche à 2,5 s, bouche à 3,33, main droite
à 7,67 pendant `SMK_WALL_LIGHT`, lâchée à 1,6 s de `SMK_WALL_STUB` ; pour
`StandingSmoke` (5_02.cat) : 2,67, 3,2 et 7,67 avec des `PropAttach`. Les
points et l'échelle de temps : docs/mxd.md.

Relevé : les 479 fichiers se lisent en entier, l'arbre finit exactement à
l'en-tête +0xc et les quatre comptes concordent partout (7 552 banques,
4 546 nœuds jouables, 259 références, 13 435 feuilles). En hachant les
chaînes de bully.exe, des scripts, des noms d'archives et des tables de
chaînes, `tools/cat.py` résout 51 % des noms de nœuds et tous les types de
conditions (20 846).

## Exemple : AI_POI

La racine `POIPoint` (si le piéton n'a ni objectif, ni vélo, ni arme…)
contient `Scenario` (`ScenarioSeek`, `ScenarioDialog`, `getGift`…), `Hangout`,
`sitting` (si `HavePOIOfType`, `Not IsSocializing` ; feuilles `SitDown`,
`ClearPOI`) et `spectator` : c'est là que le jeu choisit ce que fait un
piéton arrivé à un point d'intérêt (docs/trigger.md). Avec `--pistes` :
le nœud jouable sous `sitting` enchaîne la séquence `./SitDown`, `SitDown`
joue `AIActionOpps/SitOpps` avec des opportunités de réaction
(`Reactions/HitReact`, `Social_System`) ; `spectator` joue
`AIActionOpps/SpectatorOpps` et peut basculer vers `./StopSpectacleEarly` ou
`FleeObjective`.

Code : `src/core/ActionTree` (CActionTreeFile, ActionHash) ; test :
`tests/test_actiontree` (les 479 fichiers, les 51 001 pistes) ; lecture :
`tools/cat.py <nom.cat> [--pistes]` (arbre aux noms résolus) et
`tools/cat.py --stats`. Scripts Ghidra :
`bully-test/scripts/ExportArbre.java` (une fonction et ses appelées),
`ExportAsm.java` (les instructions d'une fonction).

## Le nœud joué : PedIsPlaying

`PedIsPlaying(ped, chemin, sous)` (0x5cb830) résout le chemin par `FUN_005f59b0` et le compare au nœud
que joue le contrôleur d'actions du piéton (+0x10, `FUN_005f3bf0`) ; avec `sous`, `FUN_005f4f80` remonte
les parents du nœud joué (emplacement +0x34) jusqu'à le trouver. `CActionBank::Joue` le refait,
`tests/test_actionbank` le vérifie sur `/Global/1_01/Talking`. Dans `lancer_lua`, le nœud joué est celui
que `PedSetActionNode` a posé, ou celui où descend le contrôleur dans l'arbre que pose `PedSetActionTree`.

## La descente du contrôleur : PedSetActionTree (22.10.2026)

`PedSetActionTree(ped, arbre, fichier)` (0x5cac20) charge le fichier (`FUN_00473f80`) puis `FUN_005dc040` pose
l'arbre au contrôleur, qui y descend par l'emplacement +0x4c de chaque nœud : une banque (`FUN_005f5490`) teste
ses conditions s'il en a (`FUN_005fb9b0`, emplacement +0x50), puis rend le premier de ses enfants, dans l'ordre,
dont la descente aboutit ; un nœud jouable ou une feuille (`FUN_005f5500`) se rend lui-même si ses conditions
passent. Dans `/Global/TrainingPlayer`, la banque `DEFAULT` vient en premier, sans condition, et sa feuille
`DEFAULT` est le nœud joué : `PedIsPlaying(0, "/Global/TrainingPlayer/Default", true)` devient vrai et
l'entraînement de `1_02` démarre. `CActionBank::Choisir` le refait ; les conditions des nœuds ne sont pas encore
jugées (une branche qui en porte n'est pas prise), et les références (`i`, `r`) ne sont pas suivies.

`PedMePlaying(ped, nom)` (0x5cb910 → 0x5f4840 → `FUN_005f46a0`) compare le **nom** haché, pas le chemin, au nœud
joué puis à ses parents (`FUN_005f4f30`) : `CActionBank::JoueNom`. Au démarrage, `FUN_005f6f20` charge les 68
fichiers de la liste 0xb0a4f0 (Globals, Missions, Player, AI, HitTree, les styles de combat, les armes) :
`CActionBank::ChargerGlobaux`. Les réglages « Butes » (`AreaTransitionButes`, `TimeCycleButes`…) sont des pistes de
`Globals.cat` trouvées par le hachage de leur nom (`FUN_0072ad00`) et rangées par `FUN_0072ae00` :
`CActionBank::Butes`.
