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
//  - à la fin d'une animation qui ne boucle pas : la piste « sequence », sinon
//    un enfant, sinon le parent ;
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
		static const uint32 kNot = ActionHash("Not"), kAleatoire = ActionHash("WeightedRandom"),
		                    kRequete = ActionHash("ActionRequest"), kScripte = ActionHash("IsScriptedAmbient");
		bool inverser = false;
		for(int32 c : noeuds[i].n->conditions){
			uint32 t = f.TypeCondition(c);
			if(t == kNot){ inverser = !inverser; continue; }
			bool v = true;
			if(t == kRequete || t == kScripte) v = false;
			else if(t == kAleatoire) v = hasard < 0.5f;
			if(inverser) v = !v;
			inverser = false;
			if(!v) return false;
		}
		return true;
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
	int32 racine = -1, noeud = -1;      // racine : le nœud d'où l'on repart quand on en sort
	float t = 0;                         // temps du nœud (s)
	float duree = -1, fin = -1, vitesse = 1, depart = 0; int32 mode = 0;
	float finPiste = -1;                 // champ 16 de la piste Animation : sa durée de vie dans le nœud (s), même en boucle
	bool animEnCours = false;
	struct Evenement { float t; uint32 point; bool fait; };
	std::vector<Evenement> props;
	struct Occasion { float t; std::string chemin; };
	std::vector<Occasion> occasions;
	std::string suite;                   // la piste « sequence »
	static constexpr float CHANCE = 0.15f;
	int32 transitions = 0;

	void Entrer(int32 i, const ArbreSorties &s, int profondeur = 0){
		if(!arbre || i < 0 || profondeur > 16) return;
		// On ne sort pas de la racine : on y revient.
		if(!Dans(i)) i = racine;
		noeud = i; t = 0; animEnCours = false; duree = -1; fin = -1; vitesse = 1; depart = 0; mode = 0; finPiste = -1;
		props.clear(); occasions.clear(); suite.clear(); transitions++;
		static const uint32 kAnim = ActionHash("Animation"), kSeq = ActionHash("sequence"), kOcc = ActionHash("Opportunity"),
			kCible = ActionHash("PlayOnTarget"), kAtt = ActionHash("PropAttach"), kAttEx = ActionHash("PropAttachEx"),
			kDet = ActionHash("PropDetach"), kDetEx = ActionHash("PropDetachEx");
		std::vector<CActionTrack> pistes = arbre->Pistes(i);
		for(const CActionTrack &p : pistes){
			float t12 = Flottant(p, 12, 0);
			if(p.type == kAnim && !animEnCours){
				mode = (int32)p.Mot(32); depart = Flottant(p, 40, 0); fin = Flottant(p, 44, -1); vitesse = Flottant(p, 48, 1); finPiste = Flottant(p, 16, -1);
				if(vitesse <= 0) vitesse = 1;
				duree = s.jouer ? s.jouer(p.Mot(24) & 0x7fffffff, mode, depart, fin, vitesse, Flottant(p, 52, -1)) : -1;
				animEnCours = duree > 0;
			}else if(p.type == kSeq) suite = arbre->Chaine(p, 32);
			else if(p.type == kOcc){ std::string c = arbre->Chaine(p, 32); if(!c.empty()) occasions.push_back({t12, c}); }
			else if(p.type == kCible){ std::string c = arbre->Chaine(p, 32); int32 j = arbre->Resoudre(i, c); if(j >= 0 && s.partenaire) s.partenaire(j); }
			// PropAttachEx : le point en 28 ; PropAttach : en 24 ; Detach : 0.
			else if(p.type == kAttEx) props.push_back({t12, p.Mot(28), false});
			else if(p.type == kAtt) props.push_back({t12, p.Mot(24), false});
			else if(p.type == kDet || p.type == kDetEx) props.push_back({t12, 0, false});
		}
		if(!animEnCours) Descendre(s, profondeur);
	}
	void Avancer(float dt, const ArbreSorties &s){
		if(!arbre || noeud < 0) return;
		t += dt;
		for(Evenement &e : props) if(!e.fait && t >= e.t){ e.fait = true; if(s.accrocher) s.accrocher(e.point); }
		// Les occasions, une fois ouvertes.
		for(const Occasion &o : occasions){
			if(t < o.t) continue;
			if(s.hasard() >= CHANCE * dt) continue;
			int32 j = arbre->Resoudre(noeud, o.chemin);
			if(j >= 0 && j != noeud && !arbre->passif[j] && arbre->Conditions(j, s.hasard())){ Entrer(j, s); return; }
		}
		// La fin de l'animation : une boucle (mode 2) ne finit pas ; le mode 1 fige
		// la dernière pose (AnimationTrack, FUN_006c0c60) et attend le partenaire ;
		// un nœud passif n'en sort que par sa propre « sequence ».
		bool expiree = animEnCours && finPiste > 0 && t >= finPiste;
		if(animEnCours && ((mode != 2 && mode != 1) || expiree)){
			if(expiree || depart + t * vitesse >= Bout()){
				if(!suite.empty()){ int32 j = arbre->Resoudre(noeud, suite); if(j >= 0){ Entrer(j, s); return; } }
				if(arbre->passif[noeud]) return;
				// Un nœud qui lâche l'objet ressort au-dessus du nœud qui l'avait pris.
				if(arbre->lache[noeud]){
					for(int32 k = arbre->noeuds[noeud].parent; k >= 0 && Dans(k); k = arbre->noeuds[k].parent)
						if(arbre->accroche[k]){ int32 p = arbre->noeuds[k].parent; Entrer(p >= 0 && Dans(p) ? p : racine, s); return; }
				}
				Descendre(s, 0);
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
	void Descendre(const ArbreSorties &s, int profondeur){
		std::vector<int32> ok;
		for(int32 e : arbre->Enfants(noeud)) if(!arbre->passif[e] && arbre->Conditions(e, s.hasard())) ok.push_back(e);
		if(!ok.empty()){ Entrer(ok[(size_t)(s.hasard() * ok.size()) % ok.size()], s, profondeur + 1); return; }
		int32 p = arbre->noeuds[noeud].parent;
		Entrer(p >= 0 && Dans(p) && p != noeud ? p : racine, s, profondeur + 1);
	}
};

}
