// Un exécuteur simplifié des arbres d'actions (Act/*.cat, docs/cat.md), pour
// que les piétons de la visite enchaînent leurs animations comme les arbres le
// disent au lieu de tables écrites à la main.
//
// Ce n'est pas le moteur : c'est une approximation écrite d'après la lecture
// des arbres et des pistes, avec ces règles :
//  - chemins : « . » est le nœud courant, « .. » son parent, un nom désigne un
//    enfant par son HashString ;
//  - entrer dans un nœud qui a une piste Animation la joue (champs 24 à 52) ;
//    son champ 16 (commun à toutes les pistes) borne sa durée dans le nœud,
//    même pour une boucle ;
//    sinon on choisit un enfant parmi ceux dont les conditions passent ;
//  - à la fin d'une animation qui ne boucle pas : la piste « sequence » ; sinon
//    un nœud jouable qui offre des occasions se rejoue (état d'attente), un
//    autre revient au nœud jouable ancêtre le plus proche, à défaut au départ ;
//  - une Opportunity, son instant passé (champ 12), est prise au hasard
//    (CHANCE par seconde) si les conditions du nœud visé passent ;
//  - conditions : WeightedRandom tire au sort, ActionRequest et
//    IsScriptedAmbient sont fausses (elles attendent un script ou le joueur),
//    « Not » inverse la suivante, les autres passent ;
//  - PlayOnTarget envoie le partenaire sur le chemin donné ; les pistes Prop*
//    accrochent ou lâchent l'objet tenu (docs/mxd.md).
#pragma once
#include "commun.h"
#include "../src/core/ActionTree.h"
#include <functional>
#include <memory>
#include <map>
#include <string>
#include <vector>

namespace outil {

class Arbre
{
public:
	std::string nom;
	CActionTreeFile f;
	struct Noeud { const CActionNode *n; int32 parent; };
	std::vector<Noeud> noeuds;

	bool Charger(uint8 *b, uint32 n){
		buf.reset(b);
		if(!f.Load(b, n)) return false;
		Indexer(&f.racine, -1);
		// Les noms en clair : les segments des chemins cités (sequence,
		// Opportunity, PlayOnTarget…) ; les nœuds passifs : ceux que vise un
		// PlayOnTarget (on n'y entre que sur ordre du partenaire) ; les nœuds qui
		// accrochent ou lâchent un objet.
		static const uint32 kCible = ActionHash("PlayOnTarget"), kAttEx = ActionHash("PropAttachEx"), kAtt = ActionHash("PropAttach"),
		                    kDet = ActionHash("PropDetach"), kDetEx = ActionHash("PropDetachEx");
		passif.assign(noeuds.size(), false); accroche.assign(noeuds.size(), false); lache.assign(noeuds.size(), false);
		for(size_t i = 0; i < noeuds.size(); i++){
			for(const CActionTrack &t : Pistes((int32)i)){
				for(const CActionAttribut &a : t.attributs){
					std::string c = f.ChaineCitee(a.decalage);
					size_t p = 0;
					while(!c.empty() && p <= c.size()){
						size_t q = c.find('/', p); if(q == std::string::npos) q = c.size();
						std::string seg = c.substr(p, q - p);
						if(!seg.empty() && seg != "." && seg != "..") noms[ActionHash(seg.c_str())] = seg;
						if(q >= c.size()) break; p = q + 1;
					}
				}
				if(t.type == kCible){ int32 j = Resoudre((int32)i, Chaine(t, 32)); if(j >= 0) passif[j] = true; }
				if(t.type == kAttEx || t.type == kAtt) accroche[i] = true;
				if(t.type == kDet || t.type == kDetEx) lache[i] = true;
			}
		}
		return true;
	}
	std::vector<bool> passif, accroche, lache;
	// Des nœuds qu'on ne visite pas, par nom (le vol d'arme : nos piétons n'en ont pas).
	std::vector<uint32> interdits;
	bool Interdit(int32 i) const {
		for(int32 k = i; k >= 0; k = noeuds[k].parent) for(uint32 h : interdits) if(noeuds[k].n->hachage == h) return true;
		return false;
	}
	// Le premier nœud (en profondeur) qui porte ce nom.
	int32 Chercher(const char *nom) const {
		uint32 h = ActionHash(nom);
		for(size_t i = 0; i < noeuds.size(); i++) if(noeuds[i].n->hachage == h) return (int32)i;
		return -1;
	}
	int32 Enfant(int32 i, uint32 h) const {
		for(size_t k = 0; k < noeuds.size(); k++) if(noeuds[k].parent == i && noeuds[k].n->hachage == h) return (int32)k;
		return -1;
	}
	std::vector<int32> Enfants(int32 i) const {
		std::vector<int32> e;
		for(size_t k = 0; k < noeuds.size(); k++) if(noeuds[k].parent == i) e.push_back((int32)k);
		return e;
	}
	// « ./A/B », « ../A », « ../../ » depuis le nœud i ; -1 si introuvable.
	int32 Resoudre(int32 i, const std::string &chemin) const {
		size_t p = 0;
		while(i >= 0 && p <= chemin.size()){
			size_t q = chemin.find('/', p); if(q == std::string::npos) q = chemin.size();
			std::string s = chemin.substr(p, q - p);
			if(s == "..") i = noeuds[i].parent;
			else if(!s.empty() && s != ".") i = Enfant(i, ActionHash(s.c_str()));
			if(q >= chemin.size()) break;
			p = q + 1;
		}
		return i;
	}
	std::vector<CActionTrack> Pistes(int32 i) const {
		std::vector<CActionTrack> v;
		for(int32 d : noeuds[i].n->pistes){ CActionTrack t; if(f.Piste(d, t)) v.push_back(t); }
		return v;
	}
	std::string Chaine(const CActionTrack &t, uint16 position) const {
		const CActionAttribut *a = t.Champ(position);
		return a ? f.ChaineCitee(a->decalage) : std::string();
	}
	// Les conditions du nœud, selon les règles ci-dessus.
	bool Conditions(int32 i, float hasard) const {
		if(Interdit(i)) return false;
		static const uint32 kNot = ActionHash("Not"), kOu = ActionHash("OR"), kAleatoire = ActionHash("WeightedRandom");
		// Fausses ici : elles attendent un script, le joueur, un modèle précis ou
		// un coup reçu.
		static const uint32 kFausses[] = { ActionHash("ActionRequest"), ActionHash("IsScriptedAmbient"), ActionHash("false"),
			ActionHash("IsPlayer"), ActionHash("IsAuthority"), ActionHash("PedModelID"), ActionHash("Health"),
			ActionHash("DamagePending"), ActionHash("HitTime"), ActionHash("PropTargetInteractive"), ActionHash("TargetRelativeOrientation") };
		bool inverser = false, ou = false, une = false, toutes = true;
		for(int32 c : noeuds[i].n->conditions){
			uint32 t = f.TypeCondition(c);
			if(t == kNot){ inverser = !inverser; continue; }
			if(t == kOu){ ou = true; continue; }
			bool v = true;
			for(uint32 x : kFausses) if(t == x) v = false;
			if(t == kAleatoire) v = hasard < 0.5f;
			if(inverser) v = !v;
			inverser = false;
			une = une || v; toutes = toutes && v;
		}
		return noeuds[i].n->conditions.empty() || (ou ? une : toutes);
	}
	std::string Nom(int32 i) const {
		if(i < 0) return "?";
		const CActionNode *n = noeuds[i].n;
		if(!n->nom.empty()) return n->nom;
		auto it = noms.find(n->hachage); if(it != noms.end()) return it->second;
		char b[16]; snprintf(b, sizeof b, "#%08x", n->hachage); return b;
	}
private:
	std::map<uint32, std::string> noms;
	struct Libere { void operator()(uint8 *p) const { free(p); } };
	std::unique_ptr<uint8, Libere> buf;
	void Indexer(const CActionNode *n, int32 parent){
		int32 moi = (int32)noeuds.size();
		noeuds.push_back({n, parent});
		for(const CActionNode &e : n->enfants) Indexer(&e, moi);
	}
};

// Ce que l'exécuteur demande à la visite.
struct ArbreSorties {
	// Jouer l'animation `hachage` (champ 24) : mode (32), départ (40), fin (44),
	// vitesse (48), fondu (52). Rend la durée de l'animation, ou < 0 si absente.
	std::function<float(uint32 hachage, int32 mode, float depart, float fin, float vitesse, float fondu)> jouer;
	std::function<void(uint32 point)> accrocher;     // 0 : lâcher l'objet
	std::function<void(int32 noeud)> partenaire;     // PlayOnTarget
	std::function<float(void)> hasard;               // dans [0, 1[
};

// Le déroulement d'un arbre pour un piéton.
struct Deroulement {
	const Arbre *arbre = nil;
	int32 racine = -1, noeud = -1;      // racine : le sous-arbre dont on ne sort pas
	int32 redepart = -1;                 // le nœud où l'on reprend quand on revient à la racine
	bool suiveur = false;                // ne bouge que sur ordre du partenaire : à la fin, il fige sa pose
	float t = 0;                         // temps du nœud (s)
	float duree = -1, fin = -1, vitesse = 1, depart = 0; int32 mode = 0;
	float finPiste = -1;                 // champ 16 de la piste Animation : sa durée de vie dans le nœud (s), même en boucle
	bool animEnCours = false;
	struct Evenement { float t; uint32 point; bool fait; };
	std::vector<Evenement> props;
	struct Occasion { float t, tmax; std::string chemin; bool sure; };
	std::vector<Occasion> occasions;
	std::string suite;                   // la piste « sequence »
	static constexpr float CHANCE = 0.15f;
	int32 transitions = 0;
	std::vector<int32> trace;            // les derniers nœuds traversés (diagnostic)

	void Entrer(int32 i, const ArbreSorties &s, int profondeur = 0){
		if(!arbre) return;
		// Une impasse (trop de sauts sans animation) : on reprend au départ.
		if(i < 0 || profondeur > 16){ if(profondeur > 32 || redepart < 0) return; i = redepart; }
		// On ne sort pas de la racine : on y revient, et l'on reprend au départ.
		if(!Dans(i)) i = racine;
		if(i == racine && redepart >= 0 && profondeur < 16) i = redepart;
		// Un suiveur qu'on envoie vers un nœud sans animation en applique les
		// effets mais garde son animation et son nœud.
		Deroulement avant = *this;
		noeud = i; t = 0; animEnCours = false; duree = -1; fin = -1; vitesse = 1; depart = 0; mode = 0; finPiste = -1;
		props.clear(); occasions.clear(); suite.clear(); transitions++;
		trace.push_back(i); if(trace.size() > 24) trace.erase(trace.begin());
		static const uint32 kAnim = ActionHash("Animation"), kSeq = ActionHash("sequence"), kOcc = ActionHash("Opportunity"),
			kCible = ActionHash("PlayOnTarget"), kAtt = ActionHash("PropAttach"), kAttEx = ActionHash("PropAttachEx"),
			kDet = ActionHash("PropDetach"), kDetEx = ActionHash("PropDetachEx"),
			kExec = ActionHash("Execute"), kLatch = ActionHash("OpportunityRandomLatch");
		std::vector<CActionTrack> pistes = arbre->Pistes(i);
		std::vector<std::string> sauts;
		for(const CActionTrack &p : pistes){
			float t12 = Flottant(p, 12, 0);
			if(p.type == kAnim && !animEnCours){
				mode = (int32)p.Mot(32); depart = Flottant(p, 40, 0); fin = Flottant(p, 44, -1); vitesse = Flottant(p, 48, 1); finPiste = Flottant(p, 16, -1);
				if(vitesse <= 0) vitesse = 1;
				duree = s.jouer ? s.jouer(p.Mot(24) & 0x7fffffff, mode, depart, fin, vitesse, Flottant(p, 52, -1)) : -1;
				animEnCours = duree > 0;
			}else if(p.type == kSeq) suite = arbre->Chaine(p, 32);
			else if(p.type == kOcc){ std::string c = arbre->Chaine(p, 32); if(!c.empty()) occasions.push_back({t12, Flottant(p, 16, -1), c, false}); }
			// OpportunityRandomLatch : prise à coup sûr à un instant tiré entre 76 et 80.
			else if(p.type == kLatch){ std::string c = arbre->Chaine(p, 32); float a = Flottant(p, 76, 0), b = Flottant(p, 80, a);
				if(!c.empty()) occasions.push_back({t12 + a + (b - a) * s.hasard(), -1, c, true}); }
			else if(p.type == kExec){ std::string c = arbre->Chaine(p, 32); if(!c.empty()) sauts.push_back(c); }
			else if(p.type == kCible){ std::string c = arbre->Chaine(p, 32); int32 j = arbre->Resoudre(i, c); if(j >= 0 && s.partenaire) s.partenaire(j); }
			// PropAttachEx : le point en 28 ; PropAttach : en 24 ; Detach : 0.
			else if(p.type == kAttEx) props.push_back({t12, p.Mot(28), false});
			else if(p.type == kAtt) props.push_back({t12, p.Mot(24), false});
			else if(p.type == kDet || p.type == kDetEx) props.push_back({t12, 0, false});
		}
		// Avec une animation, un Execute fait jouer au partenaire le PlayOnTarget de
		// la feuille visée (ou du premier enfant dont les conditions passent) :
		// Execute ./TargetOrientation met la cible en RCV/Front.
		if(animEnCours) for(const std::string &c : sauts){
			int32 j = arbre->Resoudre(i, c);
			if(j < 0 || j == i || !Dans(j)) continue;
			if(arbre->noeuds[j].n->genre == 'b')
				for(int32 e : arbre->Enfants(j)) if(arbre->Conditions(e, s.hasard())){ j = e; break; }
			for(const CActionTrack &p : arbre->Pistes(j))
				if(p.type == kCible){ int32 k = arbre->Resoudre(j, arbre->Chaine(p, 32)); if(k >= 0 && s.partenaire) s.partenaire(k); }
		}
		if(!animEnCours && suiveur && avant.animEnCours){
			int32 tr = transitions; std::vector<int32> tc = trace;
			*this = avant; transitions = tr; trace = tc;
			return;
		}
		// Sans animation, un Execute vers un nœud du sous-arbre est un saut.
		if(!animEnCours) for(const std::string &c : sauts){
			int32 j = arbre->Resoudre(i, c);
			if(j >= 0 && j != i && Dans(j) && !arbre->passif[j]){ Entrer(j, s, profondeur + 1); return; }
		}
		if(!animEnCours) Descendre(s, profondeur);
	}
	void Avancer(float dt, const ArbreSorties &s){
		if(!arbre || noeud < 0) return;
		t += dt;
		for(Evenement &e : props) if(!e.fait && t >= e.t){ e.fait = true; if(s.accrocher) s.accrocher(e.point); }
		// Les occasions, une fois ouvertes.
		for(const Occasion &o : occasions){
			if(t < o.t || (o.tmax >= 0 && t > o.tmax)) continue;
			if(!o.sure && s.hasard() >= CHANCE * dt) continue;
			int32 j = arbre->Resoudre(noeud, o.chemin);
			if(j >= 0 && j != noeud && Dans(j) && !arbre->passif[j] && arbre->Conditions(j, s.hasard())){ Entrer(j, s); return; }
		}
		// La fin de l'animation : une boucle (mode 2) ne finit pas ; le mode 1 fige
		// la dernière pose (AnimationTrack, FUN_006c0c60) et attend le partenaire ;
		// un nœud passif n'en sort que par sa propre « sequence ».
		bool expiree = animEnCours && finPiste > 0 && t >= finPiste;
		if(animEnCours && ((mode != 2 && mode != 1) || expiree)){
			if(expiree || depart + t * vitesse >= Bout()){
				if(!suite.empty()){ int32 j = arbre->Resoudre(noeud, suite); if(j >= 0 && Dans(j)){ Entrer(j, s); return; } }
				if(arbre->passif[noeud]) return;
				// Un nœud qui lâche l'objet ressort au-dessus du nœud qui l'avait pris.
				if(arbre->lache[noeud]){
					for(int32 k = arbre->noeuds[noeud].parent; k >= 0 && Dans(k); k = arbre->noeuds[k].parent)
						if(arbre->accroche[k]){ int32 p = arbre->noeuds[k].parent; Entrer(p >= 0 && Dans(p) ? p : racine, s); return; }
				}
				Finir(s);
			}
		}
	}
	// Le temps de l'animation en cours (pour Reposer).
	float TempsAnim(void) const {
		float x = depart + t * vitesse;
		return mode != 2 && animEnCours && x > Bout() - 0.001f ? Bout() - 0.001f : x;
	}
	float Bout(void) const { return fin >= 0 && fin < duree ? fin : duree; }
	// Encore dans le sous-arbre de la racine ?
	bool Dans(int32 i) const {
		for(int32 k = i; k >= 0; k = arbre->noeuds[k].parent) if(k == racine) return true;
		return false;
	}
private:
	static float Flottant(const CActionTrack &p, uint16 pos, float defaut){
		if(!p.Champ(pos)) return defaut;
		uint32 m = p.Mot(pos); float v; memcpy(&v, &m, 4); return v;
	}
	// Un enfant dont les conditions passent (au hasard parmi eux), sinon le parent.
	// Un nœud fini sans « sequence » : un banc a choisi son enfant une fois, il ne
	// rejoue pas ses frères. Un nœud jouable qui offre des occasions est un état
	// d'attente : il se rejoue ; sinon on revient au nœud jouable ancêtre le plus
	// proche, à défaut au départ.
	void Finir(const ArbreSorties &s){
		if(suiveur){ suite.clear(); occasions.clear(); return; }
		if(arbre->noeuds[noeud].n->genre == 'n' && !occasions.empty()){ Entrer(noeud, s); return; }
		for(int32 k = arbre->noeuds[noeud].parent; k >= 0 && Dans(k); k = arbre->noeuds[k].parent)
			if(arbre->noeuds[k].n->genre == 'n' && !arbre->passif[k]){ Entrer(k, s); return; }
		Entrer(racine, s);
	}
	// Choisir un enfant (entrée dans un banc ou un nœud sans animation).
	void Descendre(const ArbreSorties &s, int profondeur){
		int32 ici = noeud;
		for(int32 haut = noeud; haut >= 0 && Dans(haut); ){
			// Au premier tour, les enfants du nœud qu'on quitte ; ensuite, les frères.
			std::vector<int32> ok;
			for(int32 e : arbre->Enfants(haut))
				if((haut == noeud || e != ici) && !arbre->passif[e] && arbre->Conditions(e, s.hasard())) ok.push_back(e);
			if(!ok.empty()){ Entrer(ok[(size_t)(s.hasard() * ok.size()) % ok.size()], s, profondeur + 1); return; }
			if(haut == racine) break;
			ici = haut; haut = arbre->noeuds[haut].parent;
		}
		Entrer(racine, s, profondeur + 1);
	}
};

}
