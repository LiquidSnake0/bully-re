// Accès fichiers, hérités de GTA (re3 : core/FileMgr, core/FileLoader).
// Adresses vues dans le chargement initial (0x42ec80) et les chargeurs de
// données : ouverture 0x42d110, lecture d'un bloc 0x42d030, ligne 0x429ac0,
// positionnement 0x42d1b0, fermeture 0x42d300.
#pragma once
#include "../common.h"

class CFileMgr
{
public:
	static int32 OpenFile(const char *path, const char *mode, int32 flags);       // 0x42d110
	static int32 LoadFile(const char *path, uint8 *buf, int32 size, const char *mode);   // 0x42d030
	static bool Read(int32 fd, void *buf, int32 size);                              // 0x42d1e0
	static bool ReadExact(int32 fd, void *buf, int32 size);                         // 0x42d150 : lecture binaire, faux si incomplet
	static void Seek(int32 fd, int32 offset, int32 origin);                         // 0x42d1b0
	static void CloseFile(int32 fd);                                                // 0x42d300
};

class CFileLoader
{
public:
	// 0x429ac0 : lit une ligne dans le tampon global 0xbd0a08 (0x15e octets),
	// remplace les caractères de contrôle et les virgules par des espaces,
	// coupe au premier retour à la ligne, saute les blancs de tête.
	// Retourne nil en fin de fichier.
	static char *LoadLine(int32 fd);
	static uint8 ms_lineBuffer[0x15e];     // 0x00bd0a08

	static void LoadLevel(const char *filename);   // 0x42cd30, src/core/FileLoader.cpp
};

// Tampon de travail des chargeurs de texte : 0x00bd7800, 0x1c000 octets.
extern uint8 work_buff[0x1c000];
