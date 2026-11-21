// Une couche du lecteur d'animations et ses places, sur des enregistrements construits à la main : aucun fichier du jeu.
#include "../src/anim/AnimTrack.h"
#include <cmath>
#include <cstdio>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC ligne %d : %s\n", __LINE__, #cond); echecs++; } }while(0)
static bool Pres(float a, float b){ return std::fabs(a - b) < 1e-5f; }

static void
Places(void)
{
	CHxdAnim a; a.duree = 1.0f;
	const float dt = 0.25f;

	// La première image allume le bit 2 et n'avance pas ; les suivantes avancent de vitesse × dt.
	CPlaceAnim p; p.anim = &a; p.vitesse = 1.0f; p.drapeaux = CPlaceAnim::JOUE | CPlaceAnim::BOUCLE;
	p.Avancer(dt, false);
	VERIF(p.t == 0.0f && p.drapeaux == 7u);
	p.Avancer(dt, false);
	VERIF(Pres(p.t, 0.25f) && p.avant == 0.0f);
	p.Avancer(dt, false); p.Avancer(dt, false); p.Avancer(dt, false);
	VERIF(Pres(p.t, 1.0f) && Pres(p.avant, 0.75f));      // la durée juste atteinte n'est pas passée
	p.Avancer(dt, false);
	VERIF(Pres(p.t, 0.25f) && p.drapeaux == 7u);        // la boucle retire la durée
	printf("boucle de 1 s au pas de 0,25 s : 0, 0,25, 0,5, 0,75, 1, puis %.2f\n", p.t);

	// Sans boucle : l'instant s'arrête à la durée et le bit 0 tombe ; ensuite plus rien ne bouge.
	CPlaceAnim q; q.anim = &a; q.vitesse = 2.0f; q.drapeaux = CPlaceAnim::JOUE;
	q.Avancer(dt, false); q.Avancer(dt, false); q.Avancer(dt, false);
	VERIF(Pres(q.t, 1.0f) && (q.drapeaux & CPlaceAnim::JOUE));
	q.Avancer(dt, false);
	VERIF(q.t == 1.0f && q.drapeaux == (uint32)CPlaceAnim::PARTIE);
	q.Avancer(dt, false);
	VERIF(q.t == 1.0f && Pres(q.avant, 1.0f));
	printf("sans boucle, vitesse 2 : arrêt à %.2f, drapeaux %u\n", q.t, q.drapeaux);

	// Une vitesse négative : la boucle ajoute la durée ; sans boucle, 0 et le bit 0 tombe.
	CPlaceAnim r; r.anim = &a; r.vitesse = -1.0f; r.drapeaux = 7; r.t = 0.1f;
	r.Avancer(dt, false);
	VERIF(Pres(r.t, 0.85f) && Pres(r.avant, 0.1f));
	CPlaceAnim s; s.anim = &a; s.vitesse = -1.0f; s.drapeaux = 5; s.t = 0.1f;
	s.Avancer(dt, false);
	VERIF(s.t == 0.0f && s.drapeaux == 4u);
	printf("à l'envers depuis 0,1 s : la boucle à %.2f, sans boucle à %.2f\n", r.t, s.t);

	// Un pas plus long que deux durées : la boucle retombe à 0 (une seule durée est retirée) ; à l'envers, à la durée.
	CPlaceAnim g; g.anim = &a; g.vitesse = 1.0f; g.drapeaux = 7;
	g.Avancer(2.5f, false);
	VERIF(g.t == 0.0f);
	CPlaceAnim h; h.anim = &a; h.vitesse = -1.0f; h.drapeaux = 7;
	h.Avancer(2.5f, false);
	VERIF(h.t == 1.0f);

	// Une durée sous 0,001 s : le bit 0 tombe à la première image, l'instant à zéro.
	CHxdAnim court; court.duree = 0.0005f;
	CPlaceAnim c; c.anim = &court; c.vitesse = 1.0f; c.drapeaux = 3; c.t = 0.0003f;
	c.Avancer(dt, false);
	VERIF(c.t == 0.0f && c.drapeaux == 2u);

	// Une place qui ne joue pas : ni bit 2, ni avance ; un instant d'avant négatif remis à zéro.
	CPlaceAnim n; n.anim = &a; n.vitesse = 1.0f; n.t = 0.4f;
	n.Avancer(dt, false);
	VERIF(n.t == 0.4f && n.drapeaux == 0u);
	n.avant = -1.0f; n.Avancer(dt, false);
	VERIF(n.t == 0.0f && n.avant == 0.0f);
	CPlaceAnim vide; vide.t = 0.3f; vide.drapeaux = 1;
	vide.Avancer(dt, true);
	VERIF(vide.t == 0.3f && vide.drapeaux == 1u);

	// Le bit 2 de l'enregistrement : à chaque tour, la place repasse par une image d'attente.
	CHxdAnim b; b.duree = 1.0f; b.drapeaux = 4;
	CPlaceAnim w; w.anim = &b; w.vitesse = 1.0f; w.drapeaux = 7; w.t = 0.9f;
	w.Avancer(dt, false);
	VERIF(Pres(w.t, 0.15f) && w.drapeaux == 3u);
	w.Avancer(dt, false);
	VERIF(Pres(w.t, 0.15f) && w.drapeaux == 7u);
	w.Avancer(dt, false);
	VERIF(Pres(w.t, 0.40f));
	printf("enregistrement au bit 2 : le tour à %.2f, une image d'attente, puis %.2f\n", 0.15f, w.t);

	// Le crochet (006b20c0) : seulement pour une place qui joue, passée sa première image, et que l'appelante dit active.
	int appels = 0; float recu = 0;
	CPlaceAnim e; e.anim = &a; e.vitesse = 1.0f; e.drapeaux = 3;
	auto suite = [&](float x){ appels++; recu = x; };
	e.Avancer(dt, true, suite);
	VERIF(appels == 0);
	e.Avancer(dt, false, suite);
	VERIF(appels == 0);
	e.Avancer(dt, true, suite);
	VERIF(appels == 1 && recu == dt);
	printf("crochet : %d appel sur trois images (la première attend, la deuxième n'est pas active)\n", appels);
}

int
main(void)
{
	Places();
	printf("test_animtrack : %d échec(s)\n", echecs);
	return echecs ? 1 : 0;
}
