// CFileLoader::LoadLevel, 0x0042cd30, recréé depuis bully.exe. Le fichier
// de niveau de GTA, réduit : IMAGEPATH, TEXDICTION (dictionnaire de textures
// chargé dans un nouvel emplacement), IMGIDE (définitions de modèles dans
// une archive .img/.dir), SKY_DOME, EXIT. COLFILE, MODELFILE et HIERFILE
// sont reconnus mais ignorés : les modèles de Bully arrivent en binaire
// (shared.bin, IMGIDE), pas en IDE texte.
#include "FileMgr.h"
#include "ModelInfo.h"
#include <cstring>

extern void LoadingScreen(const char *a, const char *b, const char *c);   // 0x43c3a0
extern int32 CTxdStore_GetCurrentSlot(void);        // 0x5f2450 (via 0x1b64658)
extern void CTxdStore_SetCurrentSlot(int32 slot);   // 0x5f0920
extern void CFileMgr_SetImagePath(const char *p);   // 0x5f0a30
extern int32 CTxdStore_AddTxdSlot(const char *name);// 0x429c70
extern void CFileLoader_SetTxdParent(int32 parent, int32 slot);   // 0x42cc90
extern void CTxdStore_LoadTxd(int32 slot, int32 flag);            // 0x5f1650
extern void CFileLoader_LoadImgIde(const char *name);             // 0x42caf0
extern void CSkyDome_Load(const char *name);                      // 0x50ee20
extern void CFileLoader_AfterLevel(void);                         // 0x463e10

void
CFileLoader::LoadLevel(const char *filename)
{
	char txdname[64];
	int32 savedSlot = CTxdStore_GetCurrentSlot();
	if(savedSlot == 0){
		savedSlot = CTxdStore_GetCurrentSlot();      // le binaire relit la globale puis la pousse
		CTxdStore_SetCurrentSlot(savedSlot);
	}
	int32 fd = CFileMgr::OpenFile(filename, "r", 1);
	char *line;
	while((line = CFileLoader::LoadLine(fd)) != nil){
		if(*line == '#') continue;
		if(strncmp("EXIT", line, 4) == 0){
			LoadingScreen(filename, "Done loading level", nil);
			break;
		}
		LoadingScreen(filename, line, nil);
		if(strncmp("IMAGEPATH", line, 9) == 0){
			CFileMgr_SetImagePath(line + 10);
		}else if(strncmp("TEXDICTION", line, 10) == 0){
			CMemoryHeap::Push(0x17);
			strcpy(txdname, line + 11);
			int32 slot = CTxdStore_AddTxdSlot(txdname);
			CFileLoader_SetTxdParent(savedSlot, slot);
			CTxdStore_LoadTxd(slot, 1);
			CMemoryHeap::Pop();
		}else if(strncmp("COLFILE", line, 7) == 0 || strncmp("MODELFILE", line, 9) == 0 || strncmp("HIERFILE", line, 8) == 0){
			// ignorés
		}else if(strncmp("IMGIDE", line, 6) == 0){
			CFileLoader_LoadImgIde(line + 7);
		}else if(strncmp("SKY_DOME", line, 8) == 0){
			CMemoryHeap::Push(0x2d);
			CSkyDome_Load(line + 9);
			CMemoryHeap::Pop();
		}
	}
	CFileMgr::CloseFile(fd);
	CTxdStore_SetCurrentSlot(savedSlot);
	CFileLoader_AfterLevel();
}
