# Séquence de démarrage (0x0042ec80)

La fonction 0x42ec80 est l'équivalent de `CGame::InitialiseOnceAfterRW` de
reVC, en plus étoffé. Elle est jalonnée d'appels à 0x49a500, une fonction
vide, avec des noms d'étapes en argument : des marqueurs de profilage
compilés à vide, qui nomment les étapes pour nous.

| Étape (marqueur) | Fonction | Nom retenu |
|---|---|---|
| `EffectSys` | 0x669da0, 0x5f2580 (`LoadingIcon`, `TXD/liNA.NFT`), 0x578130 (`TXD/Effects.NFT`) | système d'effets |
| `DefCntFile`, `StoreFile` | 0x666a20 (`Config/FxDefCnt.ct`), 0x66b5f0 (`Config/FxStore.st`) | définitions d'effets |
| | 0x667940 (`Config/Effects.bft` ou `Config/Extra/Effects2.bft` selon 0xbf3808) | |
| `LogoMovies` | 0x411220 puis slot 1 de l'objet retourné avec `Movies/RockStar.wmv` | lecteur vidéo |
| | 0x43c3a0(`"Loading the Game"`, `"stuff after the legal screen"`, 0) | `LoadingScreen(titre, sous-titre, splash)`, texte identique à reVC |
| `WorldSurfTableFile` | 0x4642d0 | table `surftbl.DAT` |
| `HardnessFile` | 0x45b120(`Config\Dat\Hardness.dat`) | duretés : 67 entrées (0x204..0x310 par pas de 4) résolues par hachage contre 4 noms (0x45b100) |
| `CollisionStepEffectFile` | 0x72e240(`Config/Dat/ColStpEf.dat`) | effets de pas par surface |
| `SpecialFX` | 0x529230 | `CSpecialFX::Init` |
| `TextManager` | 0x68faa0 | textes |
| | 0x44d1d0, 0x45b750, 0x5712f0 | à lire |
| `HandlingManager` | 0x4ca5b0 | `cHandlingDataMgr::Initialise` (reVC : `mod_HandlingManager.Initialise()`) |
| `SurfaceTable` | 0x45b250(`Config\Dat\SURFACE.DAT`) | `CSurfaceTable::Initialise`, corps identique à reVC, 6 groupes, table en 0xc1a8f8 |
| | 0x6aee10 | à lire |
| `Minigame` | 0x705940 | mini-jeux |

## PEDSTATS.DAT

Deux fonctions lisent `Config\Dat\PEDSTATS.DAT` : 0x49a0a0 compte les lignes
utiles (ni vides ni commençant par `#`), alloue autant d'entrées de 0x11c
octets avec `GameMalloc`, revient au début et fait analyser chaque ligne par
0x499d80 ; 0x49a180 relit seulement (rechargement). Contrairement à reVC, la
table n'est pas de taille fixe et chaque entrée commence par le nom puis son
hachage (+0x18). C'est `CPedStats::Initialise` / `CPedStats::Reload`.

## Primitives fichier

| Adresse | Nom | Notes |
|---|---|---|
| 0x42d110 | `CFileMgr::OpenFile(path, mode, flags)` | mode `&0x8ff548` = "r" |
| 0x42d030 | `CFileMgr::LoadFile(path, buf, size, mode)` | |
| 0x42d1e0 | `CFileMgr::Read(fd, buf, size)` | |
| 0x42d1b0 | `CFileMgr::Seek(fd, off, origin)` | |
| 0x42d300 | `CFileMgr::CloseFile(fd)` | |
| 0x429ac0 | `CFileLoader::LoadLine(fd)` | tampon 0xbd0a08 de 0x15e octets ; contrôles et virgules → espaces |
| 0x5eef40 / 0x5eefa0 | `CMemoryHeap::Push(id)` / `Pop()` | identifiants de zone, comme PUSH_MEMID |
