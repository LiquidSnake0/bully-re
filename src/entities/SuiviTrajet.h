// Les entités qui suivent un trajet (PAnimFollowPath et les fonctions Lua voisines) : l'objet PathReference, qui avance le long des
// nœuds d'un trajet des .dat (le pool DAT_020c7c50, les nœuds chaînés par la table DAT_00bc5d14), et le gestionnaire 0xd13730 qui
// les tient (32 entrées de 0x2c octets, leur nombre en +0x580). Lu dans l'index le 07.11.2026.
#pragma once
#include "../common.h"
#include <cmath>
#include <vector>

// PathReference (vftable PathReference::vftable, 0x34 octets : GameMalloc(0x34) dans FUN_005ed390, le constructeur FUN_005ed8d0) :
// +4 le trajet (ses nœuds : FUN_005ec510 en rend le nombre, FUN_005ec760(n) la position du nœud n, le dernier si n déborde ou vaut
// -1), l'octet +0xc fini, +0x10 le segment (du nœud segment − 1 au nœud segment), +0x14 la vitesse signée (en m par seconde),
// +0x18 la distance parcourue sur le segment, +0x1c … +0x24 sa direction normée, +0x28 et +0x2c ses angles (FUN_005ed570 et
// FUN_00433340, que l'outil ne garde pas : il ne suit pas l'orientation des entités), +0x30 sa longueur. Un trajet de moins de deux
// nœuds ne fait rien bouger.
struct CReferenceTrajet {
	std::vector<CVector> noeuds;
	int32 trajet = -1;   // la poignée du trajet de +4 (FUN_005ed430 la rend au rappel, par son nom haché : FUN_005ed270)
	bool fini = false;
	int32 segment = 0;
	float vitesse = 0.0f, distance = 0.0f, longueur = 0.0f;
	CVector direction{0.0f, 0.0f, 0.0f};

	bool Valable() const { return noeuds.size() > 1; }
	// FUN_005ec760(n) : la chaîne des nœuds parcourue n fois depuis le premier ; au-delà (ou pour -1), le dernier.
	const CVector &Noeud(int32 n) const { return n < 0 || n >= (int32)noeuds.size() ? noeuds.back() : noeuds[n]; }
	// FUN_005ed620 : la direction du segment (le nœud segment moins le nœud segment − 1, FUN_00414a20), sa longueur (la racine,
	// FUN_0085c4d0), la direction normée (FUN_00414cc0).
	void ChangerSegment() {
		if(!Valable()) return;
		const CVector &a = Noeud(segment - 1), &b = Noeud(segment);
		direction = CVector(b.x - a.x, b.y - a.y, b.z - a.z);
		longueur = sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
		if(longueur > 0.0f){ direction.x /= longueur; direction.y /= longueur; direction.z /= longueur; }
	}
	// FUN_005ed6f0 (aussi la fin du constructeur) : la vitesse, la distance et l'octet fini à 0 ; le segment 1 et son calcul, ou, sans
	// trajet valable, le segment, la direction, les angles et la longueur à 0.
	void Remettre() {
		vitesse = 0.0f; distance = 0.0f; fini = false;
		if(Valable()){ segment = 1; ChangerSegment(); return; }
		segment = 0; direction = CVector(0.0f, 0.0f, 0.0f); longueur = 0.0f;
	}
	// FUN_005ed400 : la vitesse posée (vrai), sauf sans trajet valable (faux).
	bool PoserVitesse(float v) { if(!Valable()) return false; vitesse = v; return true; }
	// FUN_005ed750(pas, &atteint) : rien sans trajet valable, ni à l'arrêt une fois fini. En avant (vitesse > 0), tant que la distance
	// est sous la longueur : la distance plus vitesse × pas ; au-delà de la longueur, le segment suivant (la distance moins l'ancienne
	// longueur, plus fini) ou, au dernier, la distance à la longueur, fini et la vitesse à 0 ; le segment recalculé et `atteint` vrai.
	// Une distance déjà au bout (≥ la longueur) ne bouge plus (lu tel quel). En arrière (vitesse < 0), tant que la distance est
	// positive : sous 0, le segment précédent si le segment dépasse 1 (la distance sa nouvelle longueur plus le reste) ; au segment 1,
	// le segment recalculé, la vitesse à 0, fini et la distance à 0 ; `atteint` vrai dans les deux cas (et aussi, lu tel quel, au
	// segment 0, sans rien recalculer).
	bool Avancer(float pas) {
		bool atteint = false;
		if(!Valable() || (fini && vitesse == 0.0f)) return false;
		if(0.0f < vitesse){
			if(!(distance < longueur)) return false;
			float d = vitesse * pas + distance;
			if(longueur < d){
				if(segment < (int32)noeuds.size() - 1){ d -= longueur; segment++; fini = false; }
				else{ d = longueur; fini = true; vitesse = 0.0f; }
				ChangerSegment();
				atteint = true;
			}
			distance = d;
		}else if(vitesse < 0.0f && 0.0f < distance){
			float d = vitesse * pas + distance;
			if(d < 0.0f){
				if(segment > 1){ segment--; ChangerSegment(); fini = false; distance = longueur + d; return true; }
				if(segment == 1){ ChangerSegment(); vitesse = 0.0f; fini = true; d = 0.0f; }
				atteint = true;
			}
			distance = d;
		}
		return atteint;
	}
	// FUN_005ed470 : le nœud segment − 1 plus la direction × la distance ; (0, 0, 0) sans trajet valable (l'appelant FUN_005eb910 part
	// d'un vecteur nul).
	CVector Position() const {
		if(!Valable()) return CVector(0.0f, 0.0f, 0.0f);
		const CVector &a = Noeud(segment - 1);
		return CVector(a.x + distance * direction.x, a.y + direction.y * distance, a.z + distance * direction.z);
	}
};

// Le gestionnaire 0xd13730 : 32 entrées de 0x2c octets, leur nombre en +0x580 (0x160 mots). Une entrée : +0 l'entité, +4 sa boucle
// de son (PAnimFollowPathSoundLoop ; l'emplacement +0x34 de sa vftable l'arrête), +8 la référence, l'octet +0xc (le quatrième
// argument de PAnimFollowPath : l'entité prend les angles du segment, FUN_005ed520 dans FUN_005eb910), l'octet +0xd, +0x10 le rappel
// des nœuds atteints (FUN_005eb8d0 en rend l'adresse), +0x14 et +0x18 le cap et le tangage de l'entité à la prise (rendus quand elle
// est lâchée, FUN_0046a540), +0x1c … +0x28 une rotation que l'outil ne suit pas. L'outil garde l'entité par sa poignée et la
// référence ; le rappel est un numéro de référence Lua (-1 sans rappel).
struct CSuiveurTrajet {
	int32 entite = 0;   // 0 : libre
	CReferenceTrajet reference;
	bool aReference = false, orienter = false;
	int rappel = -1;
};
struct CSuiveursTrajet {
	enum { kTaille = 32 };
	CSuiveurTrajet entrees[kTaille];
	int32 nombre = 0;

	// La boucle des recherches (FUN_005ebfd0, FUN_005ebb10, FUN_005ebb50, FUN_005ebbf0, FUN_005ebf80, FUN_005eb8d0) : la première des
	// `nombre` entrées dont l'entité est celle-ci.
	CSuiveurTrajet *Trouver(int32 e) {
		for(int32 i = 0; i < nombre; i++) if(entrees[i].entite == e) return &entrees[i];
		return nil;
	}
	// FUN_005eb880 : la première entrée libre parmi les `nombre` ; sinon une de plus (au plus 32) ; nil quand les 32 servent.
	CSuiveurTrajet *Libre() {
		for(int32 i = 0; i < nombre; i++) if(entrees[i].entite == 0) return &entrees[i];
		if(nombre > 31) return nil;
		return &entrees[nombre++];
	}
	// FUN_005ebee0 : la référence détruite, l'entité lâchée (son cap et son tangage rendus si `rendre` : FUN_0046a540), l'entité, le
	// cap, le tangage, la rotation et l'octet +0xd à 0, son son arrêté. L'octet +0xc et le rappel +0x10 restent (lu tel quel).
	static void Vider(CSuiveurTrajet &s) { s.reference = CReferenceTrajet(); s.aReference = false; s.entite = 0; }
	// FUN_005ebfd0(entité, référence, orienter) : l'entrée de l'entité, sinon une libre (rien quand les 32 servent) ; FUN_005ebd60 y
	// remplace la référence (l'ancienne détruite), garde le cap et le tangage d'une entité nouvelle, pose l'entité, l'octet +0xc et la
	// référence, l'octet +0xd à 0, arrête le son, et place l'entité au point de la référence (FUN_005eb910, FUN_0046dd40). Le rappel
	// d'une entrée reprise est gardé (lu tel quel : FUN_005ebd60 ne touche pas +0x10). Rend l'entrée.
	CSuiveurTrajet *Suivre(int32 e, const CReferenceTrajet *r, bool orienter) {
		CSuiveurTrajet *s = Trouver(e);
		if(s == nil) s = Libre();
		if(s == nil) return nil;
		s->entite = e; s->orienter = orienter;
		s->aReference = r != nil;
		s->reference = r ? *r : CReferenceTrajet();
		return s;
	}
	// FUN_005ebf80 : FUN_005ebee0 sur l'entrée de l'entité.
	void Arreter(int32 e) { if(CSuiveurTrajet *s = Trouver(e)) Vider(*s); }
	// FUN_005ebbf0 : la vitesse de la référence de l'entité (FUN_005ed400), si elle a un trajet valable.
	void PoserVitesse(int32 e, float v) { CSuiveurTrajet *s = Trouver(e); if(s && s->aReference) s->reference.PoserVitesse(v); }
	// FUN_005ebb50 : la référence de l'entité remise au départ (FUN_005ed6f0).
	void Remettre(int32 e) { CSuiveurTrajet *s = Trouver(e); if(s && s->aReference) s->reference.Remettre(); }
	// FUN_005ec3a0 (appelée par FUN_0045f960) : chaque entrée occupée avance (FUN_005ec0a0 : la référence de _DAT_00aeb94c (0,02) ×
	// ms_fTimeStep secondes, FUN_005ed750), puis `apres(entrée, atteint)` place l'entité (FUN_005eb910, FUN_0046dd40) et, si un nœud
	// est atteint, appelle le rappel. Sans aucune entrée occupée, les 32 sont vidées (FUN_005ebee0(1)) et le nombre revient à 0.
	template<class F> void Avancer(float pasTimer, F apres) {
		if(nombre <= 0) return;
		bool vide = true;
		for(int32 i = 0; i < nombre; i++){
			CSuiveurTrajet &s = entrees[i];
			if(s.entite == 0) continue;
			vide = false;
			bool atteint = s.aReference && s.reference.Avancer((float)0.02 * pasTimer);
			apres(s, atteint);
		}
		if(!vide) return;
		for(int32 i = 0; i < kTaille; i++) Vider(entrees[i]);
		nombre = 0;
	}
};
