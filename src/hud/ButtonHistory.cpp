#include "ButtonHistory.h"

void
CButtonHistory::Effacer(void)
{
	for(int i = 0; i <= kPas; i++){
		CPasBouton &p = pas[i];
		p.bouton = -1; p.tenir = false; p.fait = false;
		p.delai = delaiPas;
		p.charge1 = -1; p.charge2 = -1;
	}
	for(int32 &b : ignores) b = -1;
	nombre = 0; tenus = 0; nombreIgnores = 0;
	octetA4 = false; courant = 0;
	octetC9 = false; evenement = false;
	masque = 0;
	texte.clear();
	temps = 0;
	echecAnnonce = true;
}

void
CButtonHistory::Ajouter(int32 bouton, bool tenir, int32 delai, int32 charge1, int32 charge2)
{
	if(nombre >= kPas) return;
	CPasBouton &p = pas[nombre];
	p.delai = delai; p.charge1 = charge1; p.bouton = bouton; p.tenir = tenir; p.charge2 = charge2;
	nombre++;
	masque |= 1u << (bouton & 0x1f);
	if(tenir) tenus++;
}

void
CButtonHistory::IgnorerManette(bool b, uint32 maintenant)
{
	manetteIgnoree = b;
	dernier = maintenant;
	depart = maintenant;
	if(nombre > 0 && pas[nombre - 1].tenir) pas[nombre] = pas[nombre - 1];
}

void
CButtonHistory::Rapporter(int32 bouton, int32 etat, uint32 maintenant)
{
	if(!parArbre) return;
	boutonRapporte = bouton;
	etatRapporte = etat;
	dernier = maintenant;
}

void
CButtonHistory::Remettre(void)
{
	for(int i = 0; i < nombre; i++) pas[i].fait = false;
	courant = 0;
	temps = 0;
	octetA4 = false; octetC9 = false; echecAnnonce = false;
	boutonRapporte = 0; etatRapporte = 0;
}

CButtonHistory::Evenements
CButtonHistory::Avancer(uint32 maintenant, bool rappelReussie, bool rappelRatee, bool rappelBonBouton)
{
	Evenements e;
	if(nombre < 1 || manetteIgnoree) return e;
	// FUN_0053b370, le mode de l'arbre (+0xb0) : le pas courant attend le bouton que l'arbre rapporte ; un autre bouton (ou aucun)
	// au-delà du délai de pas (le +0xc du pas d'indice courant >> 1, tel quel) fait échouer la séquence ; le bon, avec l'état voulu
	// (3 pour un pas tenu, 1 sinon), passe au pas suivant.
	if(parArbre){
		CPasBouton &p = pas[courant];
		delaiPas = pas[courant >> 1].delai;
		if(boutonRapporte != p.bouton){
			if(maintenant > dernier + (uint32)delaiPas){
				int32 b = p.bouton;
				Remettre();
				dernier = maintenant;
				boutonEchoue = b;
				evenement = true;
				echouee = true;
			}
		}else{
			boutonRapporte = 0;
			echouee = false;
			p.fait = true;
			if(p.tenir ? etatRapporte == 3 : etatRapporte == 1){
				courant++;
				dernier = maintenant;
				evenement = true;
				octetA4 = false;
				etatRapporte = 0;
			}
		}
	}
	// FUN_0053c250 : la durée de la séquence (+0xb4) écoulée depuis son départ (+0xbc) la fait échouer ; dans le mode de l'arbre, son
	// temps (+0xb8) n'avance pas et elle n'expire pas.
	bool expiree = duree >= 1 && (int32)temps >= (int32)(depart + (uint32)duree);
	if(expiree){
		echouee = true;
		boutonEchoue = pas[courant].bouton;
		Remettre();
		dernier = maintenant; depart = maintenant;
		evenement = true;
	}
	if(!echouee){
		if(nombre == courant){
			if(!evenement) return e;
			if(rappelReussie){ e.reussie = true; manetteIgnoree = true; }
		}
		if(evenement && rappelBonBouton){ e.bonBouton = true; e.bouton = boutonEchoue; }
	}else if(evenement && rappelRatee && (expiree || !echecAnnonce)){
		e.ratee = true; e.bouton = boutonEchoue; e.expiree = expiree;
		echecAnnonce = true;
	}
	return e;
}
