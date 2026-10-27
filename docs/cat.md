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
| `PedFlags` | `FUN_005fd700` | le drapeau +4 non nul (jugée depuis le 23.10 au soir, voir « L'humiliation ») |
| `HumiliationGroupIs` | `FUN_006039b0` | +0xd44 du piéton égal à +4 (le groupe d'humiliation, voir « L'humiliation ») |

- **Garde-fous de l'outil desserrés** : une `Opportunity` hors de l'arbre posé passe si le nœud visé est dans l'arbre
  global de son porteur (un piéton tombé dans `HitTree` s'en relève : `BellyUp/On_Ground` → `../BellyUpGetUp`), ou si
  son chemin remonte jusqu'à `/Global` (`HumiliateFlee/…/RunStart` → `HitTree/KnockOuts/Rear/Rear`).
- **Ce qui reste supposé ou non lu** : l'effet de coup, le cas de `FUN_004255b0` (la cible tombe d'un coup du joueur)
  et ce qui change les facteurs +0x1cd4 / +0x1cfc ne sont pas suivis. (`PedFlags`, `HumiliationGroupIs`,
  `AnimationGroupLoaded`, `IsSpeechPreloaded` et `TargetRelativePosition`, non lues ce jour-là, le sont depuis le soir :
  section suivante.)

## L'humiliation (23.10.2026, soir)

- **Le groupe d'humiliation** (+0xd44 du piéton). Ce n'est pas un champ du piéton écrit directement : c'est le +0x200
  du **composant social** du piéton, placé en +0xb44 (son piéton en +4 ; l'assembleur passe `ecx = piéton + 0xb44`).
  Le constructeur de ce composant (`FUN_0048e790`, appelé par celui de `CPed`) le pose à -1. `FUN_0048f5d0` le pose (si
  le piéton a son objet animé +0x114 ; l'ancien groupe, s'il est autre, est d'abord libéré par `FUN_0048dc60`, que
  `PedStopSocializing` appelle aussi), et le nom du groupe d'animations est pris dans la table de 22 noms de 32 octets
  de 0x9113d0 (`HUMIL_5-8_A` pour 0 … `HUMIL_6-1V6-1` pour 21). `FUN_004925d0` le choisit : par la taille de la victime
  (`CharacterSize`), le groupe qu'un autre piéton de la même taille a déjà (`FUN_00490ef0`, le pool du dernier au
  premier), sinon un tirage `rand()` / 32767 : Small 8 / 9, Fat 3 / 4 (seuil 0,5), Medium 0 / 1 / 2, Large 5 / 6 / 7,
  Huge 10 / 11 / 12 (seuils 0,33 et 0,66) ; sans l'octet du premier argument, 17, 20 ou 14 pour le joueur, 13 à 21 par les
  deux tailles pour un autre humiliateur. `FUN_00492950` donne un groupe à une victime qui n'en a pas (-1 ou au-delà de
  12) quand sa santé passe sous la moitié de son maximum (le double 0,5 de 0x8ff1f8), à la mise à jour d'une piste
  **`ManageTargets`** (`FUN_00627420`, à chaque image où elle vit ; son lancement est `FUN_006273f0`) dont la cible est
  le joueur : dans `1_02`, celle de `CombatLoco` dans `AI_FightTutorial` (section suivante). `CScriptPedPool::ChoisirGroupeHumiliation`, `SurveillerHumiliation`, `tests/test_scriptpeds`.
- **Les branches de `Humiliate_Me`** (`tools/cat.py --mots`) : `CharacterSize` puis `HumiliationGroupIs`. Un Medium
  (rang 1) joue `5-8_NoogieSpit` (groupe 0, ou sans groupe : les deux `NOT` passent), `5-8_HitSelf` (1) ou
  `5-8_IndianBurn` (2) ; chaque branche a `Success/Load` (l'attente du chargement) puis `RCV`.
- **Les conditions propres d'une `Opportunity`** (+0x3c). Le champ 60 du fichier vaut 0, mais la seconde table de
  renvois le cite, comme le +4 d'une `ConditionGroup` : il renvoie à une liste de conditions après la fin de l'arbre.
  951 des 3 080 `Opportunity` des arbres du démarrage en ont (la supposition « nulles » venait du mot lu, pas du
  renvoi). `FUN_005f7d70` les juge avant de chercher le nœud visé ; faute de les passer, la piste n'est pas prise et sa
  fermeture se juge comme d'habitude. Exemples : l'attente d'une humiliation (`LoadWait/LoadSpawn`) n'abandonne vers
  `../../ABORT` qu'après 6 s (`ActionNodePlayTime` 6, comparateur 5) ; `WaitForVictim` passe à `EmergencyAnim` après 5 s ;
  un piéton à terre se relève sur `ActionNodePlayTimePast`. `tools/cat.py --pistes` les affiche (« [si …] »).
- **`ActionNodePlayTimePast`** (`FUN_005fbfc0`) : le temps du nœud atteint le seuil +0x50 de l'objet +0x10 du contexte,
  -1 par défaut (0x9009ac, `FUN_005f3ab0`), posé par une piste `Damage` dont le +0x1c est positif (`FUN_00609400`) ;
  aucune des pistes relevées n'en a : vraie dans l'outil.
- **`AnimationGroupLoaded`** (`FUN_006024d0`) : +4 le nom du groupe (cité), +8 un octet. Faux sans piéton ou pour un
  groupe inconnu (`FUN_0042e200` cherche par le nom, sans la casse, dans les descripteurs de 0x3c octets) ; vrai si le
  groupe est prêt (+0x34) ; sinon, l'octet posé, le groupe est demandé au streaming (`FUN_00531ed0`). `LoadAnimationGroup`
  (0x5c08a0) demande puis charge tout (`LoadAllRequestedModels`) ; `HasAnimGroupLoaded` (0x5bf6e0) répond l'octet +4 de
  l'entrée de streaming. `CAnimGroups` (`src/anim/AnimGroups`, les 550 `.agr` de `Stream/World.img`), `tests/test_animgroups`.
- **La parole retenue** : `SpeechTrack` (0x932ed8, lancement `FUN_006195a0`) porte l'événement +0x18, la ligne +0x20 si
  l'octet +0x1c, la tenue +0x28 ; une tenue non nulle retient la ligne dans une des cinq places de 0x1b655d0 (le rappel
  0x619550 la garde jusqu'à l'échéance, 12 s si la cible du piéton est le joueur, puis la joue (1) ou l'annule (2)) ;
  `SpeechStartTrack` (`FUN_00619950`) la lâche. `IsSpeechPreloaded` (`FUN_00602620`) cherche l'événement, la ligne (-1 :
  toutes) et, si l'octet +0xc, le piéton dans les deux places de préchargement du son (0xcf458c, `FUN_005a0cc0` : état 2
  et données prêtes). L'outil ne joue pas les `SpeechTrack` sans tenue (une seule ligne à la fois).
- **`TargetRelativePosition`** (`FUN_005feed0`) : la cible de genre +0x14 (`FUN_00477ab0` : 0 la cible verrouillée,
  1 le partenaire de prise +0x155c, 7 +0x544…) à au plus +4 mètres, dans la direction +8 (radians depuis l'avant, vers la
  droite ; à gauche l'angle est 2π moins l'angle) à +0xc près ; l'octet +0x10 en fait aussi des partenaires de prise.
  Pour `RCV` : 1 m, droit devant, π/4.
- **`FaceTarget`** (0x934b00, mise à jour `FUN_00620dc0`) : vers la cible de genre +0x20, décalée de +0x28 degrés.
  **`SeekEntityPosition`** (0x933634, `FUN_0062a720` / `FUN_0062a940`) : vers la cible de genre +0x1c (le point de
  `FUN_0061d530` : sa position, sans os) jusqu'à moins de +0x28 m (0,75 dans `WaitForVictim`). Dans le jeu, la seconde
  passe par la recherche de chemin (`FUN_004a4eb0`) ; l'outil va tout droit, à sa vitesse de course. Une piste **`spawn`**
  fait jouer un nœud à côté (un second contrôleur, `FUN_005f8860`) : l'outil n'en garde que ces deux pistes.
  `SetTargetAsGrappleTarget` fait des deux piétons des partenaires de prise (`PedSetGrappleTarget` 0x474360, +0x155c).
- **Un `Execute` ne retient pas son nœud** : sa mise à jour (`FUN_005f70f0`) rend +0x31, posé seulement pour une durée
  de vie positive ; seules les pistes du nœud exécuté le font. Un nœud qui n'a que des `Execute` et des `Sequence`, sans
  piste exécutée, finit tout de suite. C'est le cas de la charge d'une prise sans le drapeau 70
  (`HitC/Charge/GIVE/#542875ee`, « `Not PedFlags` ») : son `Execute` vise `GrappleMoves/ExecuteNodes/StanceTransition/Give`,
  qui n'existe pas (`ExecuteNodes` est sous `GrappleStrikes`), et elle passe aussitôt à `../Give`. La régression notée
  le 23.10 venait de là, pas d'un chemin à résoudre : `PedFlags` est jugée.
- **La pile du contexte** (+0x44 / +0x48) : une piste qui vise un nœud (`NodeReferenceTrack`) a sa référence en +0x18,
  résolue d'avance (+0xc = le nœud, `FUN_005f4e30`). Quand un `Execute` ajoute des pistes (`FUN_005f3e20`), celles de la
  classe vérifiée par l'emplacement 0 sont résolues de nouveau avec, en `DAT_00b0a604`, le haut de cette pile : la clé
  qui choisit le parent d'une référence montée à plusieurs endroits (`FUN_005f6290`, `ActionNodeMultiParentReference`).
  Non suivi : l'outil résout depuis le nœud qui porte la piste.
- **Ce qui restait supposé ce soir-là** (levé ou précisé dans la section suivante) : les arbres de l'IA n'étaient pas
  joués (`ManageTargets` se lançait dans l'outil à chaque image où le geste `PedAttack` visait le joueur) ; le piéton qui
  fuit (`HumiliateFlee`, `Locomotion`) ne bouge pas, et les collisions `FRIENDLYHIT` de sa chute (`KnockOuts/Rear/Rear`,
  0 dégât) touchaient le joueur resté à côté.

## Les arbres de l'IA (23.10.2026, nuit)

- **Les classes et leurs fonctions** : la table d'enregistrement des conditions (`FUN_0064b090`) range, par classe, son
  nom, son créateur et sa table virtuelle ; la fonction qui juge est l'emplacement 1 de la table (`rtti.json` dans
  l'export). Pour une piste, l'emplacement 2 est le lancement, 3 la mise à jour (à chaque image), 4 l'arrêt.
- **Le composant d'actions du piéton** (+0x2e0, son piéton en +0x308) : le contrôleur d'actions en +0x258 et son contexte
  en +0x300 (la racine, l'arbre de `PedSetActionTree`, en +0x340 ; `FUN_00484860`), le contrôleur de l'IA en +0x290 et
  son contexte en +0x358 (`PedSetAITree` 0x5caac0 → `FUN_00484290` : l'arbre en +0x4b4, racine du contexte par
  `FUN_005f36d0`), l'état des boutons et l'**intention** (+0x254) que `FUN_005f3720` efface à chaque image.
  `FUN_00484490` fait avancer le contrôleur de l'IA (`FUN_005f49a0`) après l'avoir fait redescendre depuis la racine
  s'il ne joue rien (`FUN_00483ec0`). L'outil fait avancer l'IA avant les actions : l'intention posée se lit à la même
  image.
- **La file des objectifs** (+0x5f0 : 20 places en +8, le nombre en +0x58, l'objectif courant est le premier,
  `FUN_004701f0`). `PedAttack` (0x5c9ca0) y met un `AttackPedObjective` (`FUN_00470c10` : type 7, style +0x10, cible
  +0x14), au bout (`CPed::PushObjective`, `FUN_00471240`) ou devant (`FUN_00471320`) ; une attaque de la même cible n'est
  pas doublée. `FleeObjective` (type 22, la cible fuie en +0x10) vient de la piste du même nom. `FUN_00470110` retire
  les objectifs d'un type (en sautant celui qui prend la place d'un retiré).
- **La cible** : `PedGetTargetPed` (`FUN_00496fb0`) rend, pour un piéton qui n'est pas le joueur, +0x538 (le +0x17c du
  composant de visée en +0x3bc), que `FUN_00497d20` pose (`PedLockTarget`, `ManageTargets`, `CombatLoco`) ; la cible
  d'un objectif d'attaque passe en +0x540 quand la condition `Objective` le voit.
- **Les zones de combat** (`PedJoinCombatZone`, `FUN_00487a70`) : trois zones autour d'une cible, chacune avec sa
  capacité (+0x670 : `AIButes` +0x18, +0x1c, +0x20 = 1, 2, 4) et ses combattants (+0x67c + 0x2c × zone) ; l'attaquant
  garde la sienne en +0x66c et son masque (`PedSetCombatZoneMask`, `FUN_00486110`). Les rayons sont ceux d'`AIButes`
  (+0x28 à +0x38 : 0,9 / 1,5 / 3 / 6 / 8 m), la portée et le cosinus du cône d'attaque +0x54 / +0x58 (1,5 m, 0,76).
- **Conditions lues** :

| Classe | Fonction | Ce qu'elle juge |
|---|---|---|
| `Objective` | `FUN_00606c00` | l'objectif courant est du type +4 ; pour 7 et 16, sa cible passe en +0x540 (`FUN_0048c330`) |
| `SetPedFlags` (condition) | `FUN_005fd720` | pose le drapeau +8 à l'octet +4, toujours vraie (la banque `Combat` lève le drapeau 54) |
| `AttacksDisabled` | `FUN_006094f0` | toujours faux |
| `Print` | `FUN_00609b30` | toujours vrai (un message de mise au point) |
| `AcquireTarget` | `FUN_00601760` → `FUN_00499320` | le piéton a une cible (le choix de `FUN_004992a0` n'est pas lu en détail) |
| `JoinCombatZone` | `FUN_00601550` | la zone +8 (ou l'une des trois) autour de la cible +0x538 est prenable, en essai |
| `IsTargetOutOfReach` | `FUN_00602c40` → `FUN_00487560` | la cible hors d'atteinte (escalade, maillage de navigation) ; l'outil : faux |
| `Interval` | `FUN_006033b0` | le tirage de période +4 de `FUN_00465910` pour le piéton |
| `Random` | `FUN_00600a50` | une valeur tirée comparée à +4 par +8 |
| `WeightedRandom` | `FUN_00600b00` | faux si le seuil +8 est sous le tirage du contexte (+0x24), sinon vrai et le tirage est renouvelé |
| `RangeToTarget` | `FUN_00600780` | la distance à la cible comparée à +4 par +8 |
| `InCombatZone` | `FUN_006015e0` | la distance à la cible dans la zone +4, élargie de +8 |
| `ActionNodeRequest` | `FUN_005fd620` (`ConditionIntentionActionNodeRequest`) | l'intention (+0x254) est le nom du nœud jugé (le +0x34 du contexte) |
| `OpeningBranchChildHasChild` | `FUN_00602710` | la racine de l'arbre d'actions a un enfant +4 qui a un enfant +8 (`Offense/Short`) |
| `TargetConditionGroup` | `FUN_00601cc0` | le groupe +4 jugé pour la cible (le partenaire de prise si l'octet +8) |
| `InAttackCone` | `FUN_004865a0` | la cible à portée et devant (`AIButes` +0x54, +0x58) |
| `TargetRelativeHeight` | `FUN_00603170` | les hauteurs se recouvrent (l'outil : 1,8 m depuis les pieds, supposé) |
| `LuaCond` | `FUN_006a74d0` → `FUN_005d80a0` | la fonction +0x10 du script +0xc (« mission » : celui de la mission en cours ; sinon par nom, `FUN_005dbb60`, ou main), appelée par `NSCall` dans son espace de noms (`FUN_005d8000`) ; le résultat lu par `lua_tonumber` (0x73b0d0), positif ; `0.0` est faux |

- **Pistes lues** : `ManageTargets` (0x934d14 : `FUN_006273f0`, `FUN_00627420`) ; `CombatLoco` (0x9349e0 : `FUN_0062e9e0`,
  `FUN_0062ea40` : la zone prise par `FUN_006252d0`, ses rayons par `FUN_006253e0`, l'intention de mouvement par
  `FUN_0062e120` / `FUN_0061b430`) ; `ActionNodeRequest` (0x9334dc : `FUN_0061bb40`, `FUN_006262a0` → `FUN_006261f0` :
  l'intention tant que le nœud nommé n'est pas joué par le contrôleur d'actions, puis la vie selon +0x1c, +0x1d, +0x1e) ;
  `spawn` (0x92de80 : `FUN_005f8860`, `FUN_005f72e0`, `FUN_005f7390` : un contrôleur propre, qui redescend dans le nœud
  visé quand son nœud finit si l'octet +0x29) ; `ClearCurrentObjective` (0x936094 : `FUN_0062a180` → `FUN_00470110`) ;
  `FleeObjective` (0x935578 : `FUN_00628ce0`) ; `TargetPlayer` (0x9357d0 : `FUN_00629ba0`, la cible devient le joueur ; lue, pas jouée par l'outil).
- **Ce qui retient un nœud** (`FUN_005f4230`, dans `FUN_005f49a0`) : chaque image, chaque piste vivante est mise à jour ;
  elle meurt si sa mise à jour rend faux ou si sa durée de vie (+0x10) est passée ; le nœud vit tant qu'une piste vivante
  a l'**octet +8**. Un nœud qu'aucune piste ne retient finit donc à sa première mise à jour (`GV/END` d'une humiliation) ;
  l'outil applique la règle, sauf aux nœuds que sa manette fait jouer (ils tiennent lieu d'un bouton tenu), et fait
  finir un nœud de l'IA sans autre fin quand ses pistes `ManageTargets`, `CombatLoco`, `ActionNodeRequest` et
  `Opportunity` qui le retiennent sont mortes.
- **Dans `1_02`** : `PedSetAITree(1, "/Global/AI_FightTutorial", …)` puis `PedAttack(1, 0, 3)` ; à 30,06 s le bully
  descend dans `Combat/#7bfb37aa/CombatLoco` (`Special` refusée : `Fight_Tutorial` n'a pas d'`Offense/Special` ;
  `WaitForTargetLoco` : à portée). `CombatLoco` le mène à 1,5 m du joueur (la zone 0) ; son `spawn` fait jouer
  `CombatActions` : le `ControlNode` (retenu 1,03 s), puis `PostHit/OnGround` quand il est à terre et `PostHit/Hit` sous
  les jabs ; `Offense` attend `F_CanBullyAttack` (0.0 pendant l'entraînement). `ManageTargets` lui donne le groupe 0 à
  76,20 s. À 89,43 s, `HumiliateFlee/#575eb303` lui ôte son objectif d'attaque (`ClearCurrentObjective` 7) et lui donne
  un `FleeObjective` : la banque `Combat` ne passe plus.
- **Ce qui reste supposé ou non lu** : le choix d'une cible par `AcquireTarget` (l'outil prend celle de l'objectif) ;
  l'atteinte (escalade, maillage) ; la hauteur relative ; le déplacement de `CombatLoco` va tout droit, à l'allure de
  course, sans la recherche de chemin ni le recul ; `CollisionStart` et `AttackPhase` ne sont pas jugées (la parade de
  `Block` ne passe pas). L'**intention** ne fait pas encore jouer `Offense/Short` à l'arbre d'actions : dans
  `Fight_Tutorial`, la descente tombe toujours dans `Default_KEY` (la banque sans condition), et les passages vers
  `Offense` viennent des `ActionOpps` que les nœuds de `Locomotion.act` exécutent (`../../ActionOpps`), relatifs à
  l'endroit où la référence `r` est montée : c'est la pile du contexte, ci-dessous. La course de `RunStart` (la piste
  `Locomotion`, `FUN_00613480`, exécute l'intention de mouvement +0 / +0x1c du composant +0x2e0) n'est pas jouée :
  `AI_FightTutorial` n'a aucune branche pour l'objectif 22, seules `CombatLoco` et la piste `flee` des `Reactions`
  posent cette intention. L'outil fait donc manquer la collision d'un coup quand la cible est dans le dos de l'attaquant
  (supposé, faute des sphères des os de `FUN_005f9e70`) : le bully, retourné de 180 degrés, tombe sans toucher le joueur.
- **La pile du contexte**, lue sans être recréée : `FUN_005f36d0` pose la racine (+0x40), remet la pile à zéro (+0x44) et
  y met d'abord `FUN_005f4e40(racine)` : le nom haché de l'ancêtre de la racine placé juste sous la racine absolue
  (`DAT_01b649dc`, en remontant par l'emplacement +0x34 avec -1). Une référence montée à plusieurs endroits
  (`ActionNodeMultiParentReference`) rend son parent par `FUN_005f6290` : avec la clé -1, celle de `DAT_00b0a604` ; si
  elle vaut aussi -1, le parent par défaut (+4) ; sinon le parent rangé pour cette clé dans sa table (+0x30, +0x38 paires).
  Établi le 26.10 (« Les références montées » ci-dessous) : `FUN_005f6960` remplit cette table au chargement, et la clé est
  le nom de l'arbre de tête (`DAT_01b649dc` est `Global`), `Fight_Tutorial` pour le bully.

## Les références montées (26.10.2026)

- **`DAT_01b649dc` est la banque `Global` elle-même** (`FUN_005f6f20` la crée sous ce nom et la range là) ; « / » seul la
  désigne. `FUN_005f4e40(n)` remonte donc jusqu'à l'ancêtre dont le parent est `Global` : l'**arbre de tête** (`Fight_Tutorial`,
  `B_Striker_A`, `GlobalLoco`…), et rend son nom haché. Ce n'est pas toujours `Global` : la supposition du 23.10 tombe.
- **La clé des parents** (`DAT_00b0a604`) : `FUN_005f6c50`, quand il greffe un fichier sous `Global`, la pose au nom de
  l'arbre (`FUN_005f4e40`) puis résout ses références (`FUN_005f6bb0`).
- **Une référence 'r'** (`ActionNodeFileReference`, l'octet +0x28 à 0, `FUN_005fafb0` ; nom +0x18, dossier +0x1c) se résout
  par `FUN_005f6960` : clé = `FUN_005f4e40(référence)` ; si `/Global/<nom>` (« Locomotion.act ») existe et que son premier
  enfant est une `ActionNodeMultiParentReference`, elle sert ; sinon une neuve (`FUN_005f66b0`, 0x40 octets : la table des
  parents en +0x30, sa place +0x34, son nombre +0x38) reçoit l'arbre du fichier (`FUN_005f6c50` avec un porteur : pas greffé
  sous `Global`), dont les enfants prennent la référence multiple pour parent (vtable +0x1c) ; la banque `/Global/<nom>` est
  créée au besoin (`FUN_005f5e10`), la référence multiple y est ajoutée (`FUN_005f6650` : son parent par défaut +4 devient
  la banque) et `FUN_005f6800(banque, nom de la banque)`. Puis, toujours : `FUN_005f6800(parent de la référence, clé)`, et la
  référence prend l'arbre pour cible (+0xc, `FUN_005f5450`). Vérifié à l'assembleur (0x5f6a7e, 0x5f6b16 : `ecx` = la
  référence multiple).
- **`FUN_005f6800(parent, clé)`** : clé -1, le parent par défaut (+4) ; sinon une paire de la même clé change de parent, une
  paire du même parent change de clé, ou la paire s'ajoute (la table double, `FUN_005f56b0`). **`FUN_005f6290(clé)`**, le
  parent (vtable +0x34) d'une référence multiple : -1 prend `DAT_00b0a604` ; -1 encore, ou aucune paire, le parent par défaut.
- **Une référence vaut sa cible** : son nom (+8, `FUN_005f5da0`, « UNRESOLVED_NODE » sans cible), ses enfants (+0x2c, +0x30 :
  `FUN_005f5340`, `FUN_005f5360`), ses conditions (+0x50, `FUN_005f5320`) et sa descente (+0x4c, `FUN_005f5470`) sont ceux de
  l'arbre visé. Le chemin `SimpleLoco/Default_KEY/Locomotion/GlobalLoco/Locomotion/Idle/Turn/FacingIntended` (une piste
  `SPAWN` de `Locomotion.act`) passe ainsi par la référence `GlobalLoco.act`, nommée `GlobalLoco`.
- **Une référence dans un arbre monté ou inclus** se résout avant que l'arbre ait un parent : `FUN_005f4e40` y tombe sur un
  parent nul et rend -1, donc le parent par défaut. `FUN_005f5050` (qui refait les clés de tous les arbres de tête) n'est
  appelé que par le chargement du ballon prisonnier (`FUN_0067c140`).
- **`FUN_005f59b0(chemin, départ)`** : avec un nœud de départ, chaque « .. » prend le parent pour `DAT_00b0a604`
  (vtable +0x34) ; sans départ, la clé -1. `FUN_005f4f30` / `FUN_005f4f80` (`PedMePlaying`, `PedIsPlaying` avec « sous »)
  remontent les parents à la clé du haut de la pile du contexte.
- **Locomotion.act** est référencé par 42 fichiers (`Fight_Tutorial`, `B_Striker_A`, `Authority`, `CV_Female_A`…), et
  n'existe qu'une fois, sous `/Global/Locomotion.act` (28 parents une fois les 68 arbres globaux et `Fight_Tutorial`
  chargés). Ses `ActionOpps` (`../../../Offense`, `../../../Defense`, `../../ExecuteNodes/CustomActionOps`) visent donc
  l'arbre de la clé : pour `Fight_Tutorial`, `/Global/Fight_Tutorial/Offense`.
- **La pile du contexte** (+0x44 le haut, +0x48 les clés) : `FUN_005f36d0` pose la racine (+0x40), remet la pile à zéro et y
  met `FUN_005f4e40(racine)` ; `FUN_005f3700` empile une seconde clé (deux places au plus), quand un partenaire de prise
  joue un nœud de l'arbre de son meneur (`FUN_005f7c70`, si sa clé diffère). `FUN_005f3e20` (les pistes d'un nœud exécuté)
  et `FUN_005f70f0` (la mise à jour d'un `Execute`) recopient le haut de la pile dans `DAT_00b0a604` avant de résoudre les
  chemins des pistes (vtable +0x14, `FUN_005f5df0` : `FUN_005f59b0(chemin, porteur)` rangé en +0xc ; `FUN_005f4e30` le rend).
  Dans l'outil, `CActionController::pile`, `Racine` (posée par `Poser`, `PedSetAITree` et le lancement d'un `spawn`,
  `FUN_005f8860`), `Empiler` ; la clé est posée le temps de `Jouer`, `Entrer`, `Poser` et `Avancer`.
- **Dans l'outil** (`CActionBank::Monter`, `PoserParent`, `ParentMonte`, `CleDe`, `Lignee`) : l'arbre lu tient lieu de la
  référence multiple, rangé sous `/Global/<nom>` ; la lignée d'un nœud remplace sa banque par le parent de la clé `cle` ;
  `Trouver`, `Relatif`, `Choisir`, `Sous` traversent les références. Dans `1_02`, la descente de `Fight_Tutorial/Default_KEY`
  ne s'arrête plus sur `idle` mais entre dans `Locomotion/Free/idle`, dont l'`Execute` mène aux `ActionOpps` ; le chrono ne
  change pas (119,03 s).

## Les conditions lues le 26.10.2026

Par le registre des conditions (`FUN_0064b090` : pour chaque nom, son créateur, qui pose la table virtuelle ; la fonction
qui juge est l'emplacement 1), `lancer_lua` dresse à la fin le bilan des classes qu'il juge encore -1.

| Classe | Fonction | Ce qu'elle juge |
|---|---|---|
| `IsFemale` | `FUN_005ffb50` | l'octet +0x50 du modelinfo du piéton (la colonne FEMALE) |
| `IsAuthority` | `FUN_005ffb20` | le type du piéton (+0x1310) est PREFECT, COP ou TEACHER (0, 7, 8) |
| `PedType` | `FUN_005ffac0` | le type du piéton est +4 |
| `PedModelID` | `FUN_00602050` → `FUN_0050e7f0` | le nom haché du modelinfo (+4) est le `HashString` de la chaîne +4 |
| `IsElderly` | `FUN_006069a0` → `FUN_0044e010` | l'entrée de pedstats.dat du modelinfo (+0x54, nom haché +0x18) est `STAT_CV_MALE_OLD` ou `STAT_CV_FEMALE_OLD` |
| `HasGrappleTarget` | `FUN_00606490` | un partenaire de prise (+0x155c) |
| `IsStealthMissionChar` | `FUN_00602cf0` | +0x1398 non nul (`PedSetIsStealthMissionPed`, 0x5d0110 → `FUN_005d0080`) |
| `MotionIntentionSpeed` | `FUN_005fd810` | la vitesse de l'intention de mouvement (+0x1c du composant +0x2e0) comparée à +4 par +8 (`FUN_005fb750`) ; Locomotion.act : walk ≥ 0,1, RUN ≥ 0,5, Sprint ≥ 1,05 |
| `MotionIntention` | `FUN_005fd760` (`ConditionMotionIntentionDirection`) | le devant du piéton à plat, tourné de +4 radians (`FUN_005fbe50`), et la direction de l'intention (+0, +4, +8) : produit scalaire ≥ cos(+8) |

| `TargetRelativeOrientation` | `FUN_005ff420` | l'autre (le partenaire de prise si l'octet +4, sinon +0x434 du piéton, sinon la cible du composant de visée +0x1c8, `FUN_004d7630`) : 2 × |acos(devant · devant de l'autre) − (+8)| ≤ +0xc |
| `Sprint` | `FUN_005fda70` | la réserve de sprint (+0x18d0 : à la création la colonne R de pedstats.dat, +0x5c, `FUN_00478e30` ; remplie par `FUN_0047cde0`, dépensée par `FUN_0047cee0`) comparée à +4 par +8 |
| `IsPlayerTired` | `FUN_005fc3b0` | l'état physique du joueur (`DAT_00c1af4c`, `PlayerGetPhysicalState` 0x5d1ee0) vaut 2 |
| `WeaponDesired` | `FUN_0065b4b0` (`ConditionWeaponDesired`) | sans arme voulue (+0x18 du composant +0x2e0 nul), vrai si l'octet +8 est nul ; sinon l'arme voulue est celle du nom +4 (`FUN_0051ad50`), ou « LastWeapon » |
| `WeaponIsInventory` | `FUN_0065b7e0` | l'objet arme du piéton (+0x1d0) a un modèle (+0x10e ≠ -1) et `FUN_0055ba80` le trouve dans l'inventaire |
| `Season` | `FUN_005fc8d0` | avec l'octet +4, la zone visible est dehors (`FUN_004301d0`, l'octet par zone de 0x901dd0) ; la saison du chapitre (`DAT_00a147ec[DAT_00a147e0]` : 1, 1, 2, 3, 3, 3, 0) est +8 |

**L'intention de mouvement** (+0, +4, +8 la direction ; +0xc … +0x14 le cap voulu ; +0x1c la vitesse, dans le composant
+0x2e0) : la navigation la pose à chaque image (`FUN_004a4a50`) et en remet la vitesse à 0 sans but (`FUN_004a45c0`) ;
`SetMotionIntention` (lancement `FUN_00611c50`) n'en pose que la direction (le devant tourné de +0x18 degrés). Les allures
des scripts (`PedMoveToXYZ` 0x5c7c90, `PedMoveToPoint`) passent par `FUN_005c6180` : 0 → 0,3, 1 → 0,7, 2 → 1,1, 3 → 1,5,
4 → -1, sans argument 0,7 (le `MoveToPosObjective` de `FUN_00470460` la porte). Dans l'outil : la vitesse de l'allure pour
un piéton qui marche, 0 arrivé ou arrêté, 0,7 vers la cible dans `CombatLoco` (supposé), 0 à chaque image pour un piéton
de l'IA qui n'a pas de geste.

## Les conditions lues le 27.10.2026

| Classe | Fonction | Ce qu'elle juge |
|---|---|---|
| `CollisionStart` | `FUN_00602260` | l'octet +0x4e5 du piéton : posé à 1 par le lancement d'une piste `Collision` (`FUN_005f9480`, 0x5f94aa) ou `WeaponCollision` (`FUN_00661660`, 0x661694) ; aucune instruction ne le remet à 0 (la seule autre écriture, 0x6f2d68, est dans `FUN_006f2d10`, sur un autre objet) : vrai pour tout piéton qui a déjà lancé un coup |
| `AttackPhase` | `FUN_00601fc0` | la phase de l'attaque (+0x71c) est +4 ; si +8 est positif, l'échéance (+0x720, en ms de `DAT_00c1a9b4`) est posée et tombe plus de +8 s après maintenant |
| `Controller` | `FUN_006006f0` | l'octet `DAT_00a45a4c` (1 dans le .data, aucune instruction ne l'écrit) laisse passer ; avec l'octet +5, le piéton est mené par l'une des deux manettes (`DAT_00c2b120`, `DAT_00c2b124`) ; sinon par la manette 1 si l'octet +4, la 0 sinon. La manette 0 mène le joueur (le constructeur de `CPlayerPed`, `FUN_0049b1b0`) ; `PedSetControllerID` (`FUN_005c9fc0`) et la piste `Controller` (`FUN_0061d860`, arrêt `FUN_0061a890`) les changent |
| `WeaponDesiredIsEquiped` | `FUN_0065b5b0` | sans arme voulue (+0x18 du composant +0x2e0), vrai ; sinon l'objet arme (+0x1d0) en a le modèle (+0x10e), et une arme voulue « dernière » (-1) est effacée |
| `WeaponAmmo` | `FUN_0065b290` | les munitions (`FUN_00486130`, 0 pour une arme sous 1) de l'arme tenue (`FUN_00474aa0`), ou avec l'octet +0xc de l'arme du HUD (`HUDWeapons`, +0xc0) pour le joueur seul, comparées à +4 par +8 |
| `IsWeaponFiredWhenDropped` | `FUN_0065bbe0` | l'octet +0x2b du type de l'arme tenue (`WeaponGetType`) ; faux sans arme |
| `CollisionWeaponType` | `FUN_0065aa30` | sur le relevé du dernier coup reçu (le +0x10 du contexte, +0x408 du composant +0x2e0) : l'arme du coup (+0x5e, le mot +0x466, -1 à mains nues) et son genre (+0x60, +0x468), les 5e et 19e arguments de la méthode du coup (`FUN_0047b810`). Avec l'octet +8, l'arme du nom +0xc (`FUN_0051ad50` : -1 pour « unarmed » ou un nom hors des 146 armes de `ms_weaponModelStore`, `DAT_00c735e8`) est celle du coup ; sans lui, une arme est requise, puis le genre +4 (0 à 3) juge son type (+0x68, +0x69) et le genre du coup |
| `IsSocialStateWait` | `FUN_00603b80` | l'état social du piéton (+0xbb0, le +0x6c du composant social +0xb44) est 2, 4 ou 6 et il a un partenaire (+0xbb4, +0x70) |
| `IsSocializing` | `FUN_006039d0` | un état social et un partenaire ; avec l'octet +4, pour le joueur : un autre piéton du pool (`DAT_00c0f5f0`, du dernier au premier) a le joueur pour partenaire, sans le bit 0 de son octet +0xd68 |
| `HudComponentVisible` | `FUN_0065aee0` | l'emplacement 4 (+0x10) du composant +4 de la table du HUD (`DAT_00c674a0`, 48 places remplies par la fabrique `FUN_0053cee0` depuis `FUN_00514920`) : l'octet +8 de `HUDComponent` (`FUN_00538f80`), sauf `HUDMoney` (13, `FUN_00542740`) et `HUDClockWarnings` (15, `FUN_0053c820`) |

**La phase de l'attaque** (`CActionController::PhasesCollision`) : le lancement de la piste `Collision` (`FUN_005f9480`) pose la
phase 1, l'octet +0x4e5, +0x4e8 et +0xe4 à 1, et l'échéance à maintenant + (+0x18) s ; sa mise à jour (`FUN_005f9e70`), une fois
le temps du nœud à +0x18, la phase 2 et l'échéance à maintenant + (+0x1c) s (la fenêtre, ou +0x1c négatif), puis après +0x1c la
phase 3 et l'échéance à maintenant + la durée de vie (+0x10) si elle est positive, sinon + ce qui reste de l'animation
(`FUN_006b1df0` ; l'outil prend la fin du nœud) ; l'arrêt (`FUN_005f8240`) remet la phase et +0xe4 à 0. Les valeurs +0x18 et
+0x1c sont des temps du nœud, ajoutés tels quels à l'horloge (pas ce qui reste à attendre). Dans `AI_FightTutorial`, `BLOCK`
demande `CollisionStart` à la cible et `F_CanBlock`, et la contre-attaque `AttackPhase[3, 0,567]` à la cible ; dans `1_02`, les
1 171 jugements de `CollisionStart` se décident ; `F_CanBlock` rend 1 par moments (85 fois sur 435 à partir de 69 s), mais le bully
ne pare pas : la feuille `BLOCK` demande que le joueur joue `LightAttacks` ou `HeavyAttacks`, et la banque `CombatActions` du
bully ne redescend qu'à la fin de son `ControlNode` (1,03 s), jamais pendant le jab du joueur.

**La vitesse de `CombatLoco`** (levée le 27.10) : la mise à jour (`FUN_0062ea40`) appelle, après la zone et les rayons, le choix du
point (`FUN_0062e120`) puis le pas (`FUN_00625970`), qui passe à la navigation (`FUN_004a4eb0`) le point et une vitesse tirée du
mode que `FUN_0061b430` a rangé (+0x24) : 2 et 4 (rester sur place) 0 ; 12 (`FUN_0061b820`) 1 − (1 − t)⁸ + 0,5 avec t = (d² −
AIButes +0x2c) × 0,04 borné ; 7 et 9 (`FUN_0061b8e0`) min(d² / 64, 1) × 1,3 + 0,2 ; tout autre mode 1,5 (`_DAT_0090fb9c`). L'approche
(`FUN_0062c740`, sous-état 0 hors de l'image de tirage, l'octet +0x78 de `FUN_00465910(8)`) est le mode 11 vers la cible, 10 sur
place dans le petit rayon (+0x6c) ; à l'image de tirage, un point entre les deux rayons (+0x6c, +0x70 : `FUN_006253e0`), le mode 8,
puis le sous-état 1 (`FUN_0061b700`) et l'attente (`FUN_00625740`), non suivis. Le 0,7 supposé tombe : dans `1_02`, le bully qui gagne
sa zone joue `Locomotion/Free/Sprint` (30,10 s) au lieu de `RUN` (`ScriptPedVitesseDuPas`).

**La course de `RunStart`** (le nœud de `HumiliateFlee` dans `Player.act`) : `Locomotion` (lancement `FUN_0060c5b0`, mise à jour
`FUN_00613480`) lit l'intention (la direction +0, ou le cap voulu +0xc si l'octet +0x39 ; la vitesse +0x1c), ou vise la cible à
0,3 (`_DAT_00911e30`) si l'octet +0x38 ; celle de `RunStart` a +0x38 et +0x39 nuls. Aucune piste de `RunStart` (`Locomotion`,
`LocomotionAnimationNew`, `LocomotionEvent`, `TargetPlayer`, `FaceTarget` à 180 degrés, l'`Opportunity` de 0,833 s vers
`KnockOuts/Rear`) n'écrit la vitesse : les 35 écritures de +0x1c du composant (relevées à l'assembleur) sont dans la navigation
(`FUN_004a45c0`, `FUN_004a47c0`, `FUN_004a4a50`, `FUN_004a4eb0`), les pistes de locomotion de l'IA (`PursuitLoco`, `KeepDistanceLoco`,
`JumpLoco`, `FollowPath`, `FaceObjective`, `FollowFocus`, `EvadeProjectile`…) et `CPed` (emplacement 12, `FUN_004812d0`, 0x481603 :
0, sur une branche). La vitesse est donc celle que la dernière piste de navigation a laissée (l'arrêt de `CombatLoco`,
`FUN_00625230`, ne la remet pas à 0) : ce qu'elle vaut à 89,43 s n'est pas établi, l'outil la met à 0 (le bully tourne et tombe sur
place).

**Le HUD** (`src/hud/HudComponents`) : `HUDComponent` (`FUN_0053cc70`) naît caché ; l'emplacement 5 (`FUN_0053cd60`) le montre
ou le cache (caché, il ne clignote plus) ; `ToggleHUDComponentVisibility` (0x5bd500), `ToggleHUDComponentLocked` (0x5bd560,
l'octet +0x15), `HUDComponentGetVisibility` (0x5bd590), `HUDClearAllElements` (0x5bdb80), `HUDSaveVisibility` /
`HUDRestoreVisibility` (`FUN_00514080`, `FUN_005140c0`, la copie `DAT_00c66d98`) sont réels dans `lancer_lua`. Les arbres
testent les invites sociales (23 à 36 : `HUDSocialApologize` … `HUDSocialBurn`), `HUDSocial` (37), `HUDClothing` (14),
`HUDBarber` (18), `HUDTattoo` (19) ; dans `1_02`, `main` ne fait que cacher la santé, le radar et la retenue (4, 11, 0) et
montrer l'historique des boutons (21) : les 1 306 jugements se décident, faux.

## Le chemin d'un nœud (18.10.2026)

Les scripts désignent un nœud par un chemin, `PedSetActionNode(ped, "/Global/1_01/Talking",
"Act/Conv/1_01.act")`. Dans le moteur :

- `FUN_005f6f20` crée la banque **`Global`** et la range en `DAT_01b649dc` (« / » seul la désigne ; l'outil
  la met sous une racine sans nom) ; `FUN_005f6c50("Act/Conv/1_01.act")` retire un `/` de tête, écrit `.cat` à
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
