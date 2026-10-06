// La synchronisation labiale (World.img, lipFile_000.lip … lipFile_492.lip), docs/lip.md.
//
// bully.exe : le streaming nomme l'entrée 0x5b54 + n « lipfile_%03d.lip »
// (FUN_0052e990) ; FUN_0068df50 la charge : un u16 (nombre de répliques), un
// u16 (taille des données), les répliques (24 octets chacune, copiées telles
// quelles), puis les données, 2 bits par image. FUN_0068dd30 lit l'ouverture
// de la bouche à un instant : image i = u × (n − 1) tronqué, deux bits par
// image, poids forts d'abord (l'image 0 dans les bits 7-6), interpolation
// linéaire entre i et i + 1, divisée par 3 (constante 0x9145d0 = 1/3).
#pragma once
#include "../common.h"
#include <vector>

struct CLipReplique {
	uint32 id;              // petit numéro (11783 pour la première de lipFile_000)
	float duree;            // en secondes
	uint16 images;          // n : environ 33,5 images par seconde
	uint16 depart;          // en octets dans les données
	uint32 sonDecalage;     // le son de la réplique dans sa banque (multiples de 0x800)
	uint32 sonTaille;
	uint32 hachage;         // le nom de la réplique, haché (non identifié)
};

class CLipFile
{
public:
	std::vector<CLipReplique> repliques;
	std::vector<uint8> donnees;

	// Faux si l'en-tête ne tient pas dans le tampon ou si une réplique déborde des données.
	bool Load(const uint8 *buf, uint32 n);
	// Le niveau (0 bouche fermée … 3 grande ouverte) de l'image k de la réplique r.
	int32 Niveau(const CLipReplique &r, int32 k) const;
	// L'ouverture à l'instant u (0 = début, 1 = fin de la réplique), entre 0 et 1 :
	// FUN_0068dd30.
	float Ouverture(const CLipReplique &r, float u) const;
};
