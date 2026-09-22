#include "CdStream.h"
#include "FileMgr.h"
#include <cstring>
#include <strings.h>

extern void *GameMalloc(size_t size);

CdImage CdStream::ms_images[MAX_CDIMAGES];
int32 CdStream::ms_numImages;

bool
CdImage::LoadDirectory(const char *dirPath)
{
	int32 fd = CFileMgr::OpenFile(dirPath, "rb", 1);
	if(fd == 0) return false;
	// lecture par blocs de 32 octets jusqu'à la fin, comme 0x42caf0 pour IMGIDE
	int32 cap = 256;
	m_entries = (CDirectoryEntry*)GameMalloc(cap * sizeof(CDirectoryEntry));
	m_numEntries = 0;
	CDirectoryEntry e;
	while(CFileMgr::ReadExact(fd, &e, sizeof(e))){
		if(m_numEntries == cap){
			cap *= 2;
			CDirectoryEntry *n = (CDirectoryEntry*)GameMalloc(cap * sizeof(CDirectoryEntry));
			memcpy(n, m_entries, m_numEntries * sizeof(CDirectoryEntry));
			m_entries = n;
		}
		m_entries[m_numEntries++] = e;
	}
	CFileMgr::CloseFile(fd);
	return true;
}

const CDirectoryEntry *
CdImage::Find(const char *name) const
{
	for(int32 i = 0; i < m_numEntries; i++)
		if(strncasecmp(m_entries[i].name, name, CDSTREAM_NAME_LEN) == 0) return &m_entries[i];
	return nil;
}

int32
CdStream::AddImage(const char *imgPath)
{
	if(ms_numImages >= MAX_CDIMAGES) return -1;
	CdImage &img = ms_images[ms_numImages];
	strncpy(img.m_path, imgPath, sizeof(img.m_path) - 1);
	char dirPath[128];
	strncpy(dirPath, imgPath, sizeof(dirPath) - 1);
	char *dot = strrchr(dirPath, '.');
	if(dot) strcpy(dot, ".dir");
	if(!img.LoadDirectory(dirPath)) return -1;
	return ms_numImages++;
}
