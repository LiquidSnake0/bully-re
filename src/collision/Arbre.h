// L'arbre de partition d'un modèle de collision (le bloc 0xef5dcb33 des .col, l'objet que pointe le +0x14 des données d'un modèle) et
// son parcours, celui que le test général de bully.exe (FUN_0056c770, CCollisionBalayee::TestGeneral) demande pour n'essayer que les
// triangles proches d'une sphère : FUN_005616a0 donne le premier triangle candidat, FUN_00561750 le suivant, 0xffff à la fin. Le
// parcours tient son état dans des mots globaux (de 0xcf2168 à 0xcf2190, puis une pile de cases de 0x28 octets en 0xcf2190) ; ici,
// les champs d'un objet.
//
// Un nœud fait 12 octets (dans l'ordre de la mémoire, voir FUN_00561790) : trois octets en +0, +1 et +2 qui coupent le min de l'une des deux boîtes filles, l'octet des drapeaux en
// +3, trois octets en +4, +5 et +6 qui coupent leur max, l'octet des comptes en +7, et deux mots de 16 bits en +8 et +10. Une
// coupe vaut min + octet × (max − min) / 255 (le double 255 en 0x8ff1e8) dans la boîte du nœud. Les bits 0 à 2 des drapeaux
// disent si la coupe du min en x, y, z va à la première boîte (bit à 1) ou à la seconde, les bits 3 à 5 de même pour le max. La
// première boîte (la case marquée du seul rang du nœud) prend le bit 6 (feuille), le mot +8 et les quatre bits bas de +7 ; la
// seconde (le rang | 0x8000) le bit 7, le mot +10 et les quatre bits hauts. Une feuille donne son mot comme premier triangle et
// son compte comme nombre de triangles ; sinon le mot est le rang du nœud fils.
#pragma once
#include "../common.h"
#include <cstring>
#include <vector>

struct CArbreCollision {
	// L'objet de l'arbre (0x28 octets, le +0x14 des données) : la boîte de la racine (le min en +0x0, le max en +0x10), le nombre de
	// nœuds en +0x20 et les nœuds en +0x24 (sur l'hôte, un vecteur que l'objet garde).
	struct Donnees {
		float min[3]; float pad0; float max[3]; float pad1; int32 nNoeuds; const uint8 *noeuds;
		std::vector<uint8> stock;
		// FUN_00561690 (lue à l'assembleur de 0x561690 à 0x56169a, 11 octets) : le constructeur, le compte et les nœuds à nul.
		Donnees() : pad0(0.0f), pad1(0.0f), nNoeuds(0), noeuds(nullptr) { for(int i = 0; i < 3; i++) min[i] = max[i] = 0.0f; }
		Donnees(const Donnees &o) : pad0(0.0f), pad1(0.0f), nNoeuds(o.nNoeuds), stock(o.stock) {
			for(int i = 0; i < 3; i++) { min[i] = o.min[i]; max[i] = o.max[i]; }
			noeuds = stock.empty() ? nullptr : stock.data();
		}
		// FUN_00561790(nTriangles, min, max, n, nœuds du fichier) (méthode, ret 0x14, lue à l'assembleur de 0x561790 à 0x5618a8,
		// 281 octets) : la boîte de la racine copiée (le quatrième mot de chaque ligne reçoit un mot de sa pile, ici 0 ; le nombre de
		// triangles n'est pas lu), le compte, les anciens nœuds libérés (operator delete) et n × 12 octets alloués (GameMalloc, le débordement
		// de la multiplication donne −1), puis chaque nœud remis dans l'ordre que lit le parcours : le fichier a les drapeaux en +0,
		// les comptes en +1, les coupes du min en +2..+4 et du max en +5..+7, les deux mots en +8 et +10 ; la mémoire les drapeaux en
		// +3, les comptes en +7, les coupes en +0..+2 et +4..+6, les mots aux mêmes places. Enfin le mot de nœud des 32 cases de la
		// pile globale (de 0xcf21b8 à 0xcf26b8, pas de 0x28) passe à 0xffff : la pile du parcours a donc 32 cases (la case 0 ne sert
		// pas). CColLoader garde le bloc 0xef5dcb33 tel quel (CCollisionData::kdTree) : le chargeur du binaire (0x42bb60, à 0x42beaf)
		// appelle cette fonction avec la boîte en +0x4 et +0x14 du bloc, le compte en +0x24 et les nœuds en +0x28.
		void Poser(int32 nTriangles, const float *mn, const float *mx, int32 n, const uint8 *fichier) {
			for(int i = 0; i < 3; i++) { min[i] = mn[i]; max[i] = mx[i]; }
			pad0 = pad1 = 0.0f;
			nNoeuds = n;
			stock.assign(n > 0 ? (size_t)n * 12 : 0, 0);
			for(int32 j = 0; j < n; j++) {
				const uint8 *f = fichier + (size_t)j * 12; uint8 *m = &stock[(size_t)j * 12];
				m[3] = f[0]; m[0] = f[2]; m[1] = f[3]; m[2] = f[4]; m[4] = f[5]; m[5] = f[6]; m[6] = f[7]; m[7] = f[1];
				m[10] = f[10]; m[11] = f[11]; m[8] = f[8]; m[9] = f[9];
			}
			noeuds = stock.empty() ? nullptr : stock.data();
		}
	};
	// L'objet depuis le bloc du fichier, comme le chargeur (voir Poser).
	static void Lire(const uint8 *bloc, int32 nTriangles, Donnees *d) {
		float mn[3], mx[3]; int32 n;
		memcpy(mn, bloc + 0x4, 12); memcpy(mx, bloc + 0x14, 12); memcpy(&n, bloc + 0x24, 4);
		d->Poser(nTriangles, mn, mx, n, bloc + 0x28);
	}
	// Une case de la pile (0x28 octets) : le rang du nœud, marqué de 0x8000 pour la seconde boîte, puis la boîte en +0x4 (un CColBox,
	// posé par CColBox::Set, 0x564750, avec une surface et une pièce nulles).
	struct Case { uint16 noeud; uint16 pad; float min[3]; float pad0; float max[3]; float pad1; uint8 surface, piece, pad2[2]; };

	const Donnees *arbre = nullptr;   // 0xcf2168
	uint16 rang = 0;                  // 0xcf216c : le rang du prochain triangle dans la feuille
	uint16 compte = 0;                // 0xcf2170 : le nombre de triangles de la feuille
	uint16 premier = 0;               // 0xcf2174 : le premier triangle de la feuille
	uint16 haut = 0;                  // 0xcf2178 : le nombre de cases de la pile (la case 0 ne sert pas)
	int32 mode = 0;                   // 0xcf217c : 1 une sphère, 3 une sphère vue en x et y, 0 fini
	float sphere[4] = {0, 0, 0, 0};   // 0xcf2180 : la sphère (un CColSphere, posé par CColSphere::Set, 0x56ea50)
	Case pile[33] = {};               // 0xcf2190 : 32 cases utiles, de 1 à 32 (FUN_00561790)

	// FUN_00564a00(sphère, min, max) (cdecl, lue à l'assembleur de 0x564a00 à 0x564b3c, 316 octets) : la sphère touche-t-elle la
	// boîte ? Axe par axe, x, y puis z : min ≤ centre + rayon (fcompp, test ah,0x41, je : un min plus grand refuse), puis max ≥ centre
	// − rayon (test ah,5, jnp : un max plus petit refuse) ; la somme et la différence restent sur la pile du x87, sans arrondi à
	// float. Une comparaison avec un NaN passe.
	static bool SphereBoite(const float *s, const float *mn, const float *mx) {
		for(int i = 0; i < 3; i++) {
			if((double)mn[i] > (double)s[i] + s[3]) return false;
			if((double)mx[i] < (double)s[i] - s[3]) return false;
		}
		return true;
	}
	// 00561080 (fastcall : le min de la boîte du nœud dans eax, le max dans ecx, le rang du nœud dans bx ; lue à l'assembleur en
	// entier, jusqu'à 0x56159f, 1 311 octets ; recréée pour le mode 1 seulement, d'où sa forme sans préfixe) : les deux boîtes filles du nœud, chacune la boîte du nœud avec ses coupes (les longueurs
	// max − min arrondies à float, la coupe en double puis arrondie à float), puis chaque boîte que touche la sphère va sur la pile :
	// la seconde d'abord (le rang | 0x8000), puis la première (le rang). Seul le mode 1 est recréé (une sphère, FUN_00564a00) : le
	// mode 2 essaie la seconde forme posée en 0xcf2194 par une fonction non lue (00564d40 sur la boîte), le mode 3 la sphère en x et
	// y seulement puis la distance de 005641c0 (1 001 octets, non lue) contre le carré du rayon ; le test général ne demande que le
	// mode 1 (son 005616a0 reçoit un dernier argument nul, à 0x56d1a9). Dans ces deux modes, l'outil n'empile rien.
	void Descendre(uint16 n, const float *pmn, const float *pmx) {
		float d[3], b1mn[3], b1mx[3], b2mn[3], b2mx[3];
		for(int i = 0; i < 3; i++) {
			d[i] = (float)((double)pmx[i] - pmn[i]);
			b1mn[i] = b2mn[i] = pmn[i]; b1mx[i] = b2mx[i] = pmx[i];
		}
		const uint8 *x = arbre->noeuds + (size_t)n * 12;
		uint8 f = x[3];
		for(int i = 0; i < 3; i++) {
			float c = (float)((double)pmn[i] + ((double)x[i] * d[i]) / 255.0);
			if(f & (1 << i)) b1mn[i] = c; else b2mn[i] = c;
		}
		for(int i = 0; i < 3; i++) {
			float c = (float)((double)pmn[i] + ((double)x[4 + i] * d[i]) / 255.0);
			if(f & (8 << i)) b1mx[i] = c; else b2mx[i] = c;
		}
		if(mode != 1) return;
		if(SphereBoite(sphere, b2mn, b2mx)) Empiler((uint16)(n | 0x8000), b2mn, b2mx);
		if(SphereBoite(sphere, b1mn, b1mx)) Empiler(n, b1mn, b1mx);
	}
	void Empiler(uint16 n, const float *mn, const float *mx) {
		if(haut >= 32) return;   // le binaire écrirait au-delà de 0xcf26b8 (supposé : jamais)
		Case &c = pile[(size_t)haut + 1];
		c.noeud = n; c.pad = 0;
		for(int i = 0; i < 3; i++) { c.min[i] = mn[i]; c.max[i] = mx[i]; }
		c.pad0 = c.pad1 = 0.0f; c.surface = 0; c.piece = 0;
		haut++;
	}
	// FUN_005615a0 (lue à l'assembleur de 0x5615a0 à 0x56168b, 236 octets) : la case du haut de la pile sort ; pour une feuille, le
	// premier triangle (le mot +8 ou +10 du nœud), le compte (les quatre bits bas ou hauts de +7), le rang 1, et le premier triangle
	// est rendu ; sinon 00561080 descend dans le nœud fils (le même mot) avec la boîte de la case, et l'on recommence. Pile vide :
	// le mode passe à 0 et 0xffff est rendu.
	uint16 Depiler() {
		for(;;) {
			if(haut == 0) { mode = 0; return 0xffff; }
			Case c = pile[haut];
			const uint8 *x = arbre->noeuds + (size_t)(c.noeud & 0x7fff) * 12;
			bool second = (int16)c.noeud < 0;
			bool feuille = second ? (x[3] >> 7) != 0 : ((x[3] >> 6) & 1) != 0;
			haut--;
			uint16 v;
			memcpy(&v, x + (second ? 10 : 8), 2);
			if(feuille) {
				premier = v;
				compte = second ? (uint16)(x[7] >> 4) : (uint16)(x[7] & 0xf);
				rang = 1;
				return v;
			}
			Descendre(v, c.min, c.max);
		}
	}
	// FUN_005616a0(centre, rayon, a3) (méthode de l'objet de l'arbre, ret 0xc, lue à l'assembleur de 0x5616a0 à 0x5616ff, 96
	// octets) : le mode 3 si a3, 1 sinon ; la pile vide ; l'arbre retenu ; la sphère posée (CColSphere::Set, une surface et une
	// pièce nulles) ; le rang et le compte à 0 ; 00561080 sur la racine (le rang 0, la boîte de l'objet) ; puis FUN_005615a0, dont
	// elle rend le premier triangle candidat.
	uint16 Debut(const Donnees *a, const float *centre, float rayon, bool a3) {
		mode = a3 ? 3 : 1;
		haut = 0;
		arbre = a;
		sphere[0] = centre[0]; sphere[1] = centre[1]; sphere[2] = centre[2]; sphere[3] = rayon;
		rang = 0; compte = 0;
		Descendre(0, a->min, a->max);
		return Depiler();
	}
	// FUN_00561750 (lue à l'assembleur de 0x561750 à 0x56178a, 59 octets) : le candidat suivant. Mode 0 : 0xffff ; un rang sous le
	// compte (comparaison non signée, jae) : le premier triangle plus le rang, qui monte de 1 ; sinon FUN_005615a0.
	uint16 Suivant() {
		if(mode == 0) return 0xffff;
		if(rang < compte) { uint16 k = (uint16)(premier + rang); rang++; return k; }
		return Depiler();
	}
};
