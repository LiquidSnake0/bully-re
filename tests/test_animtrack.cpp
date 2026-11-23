// Une couche du lecteur d'animations et ses places, sur des enregistrements construits à la main : aucun fichier du jeu.
#include "../src/anim/AnimTrack.h"
#include "../src/anim/MotionTrack.h"
#include "../src/anim/Agr.h"
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

static void
Couches(void)
{
	CHxdAnim a; a.duree = 2.0f;
	const float dt = 1.0f / 30.0f;

	// L'entrée : +0x10 = +0x14 = -0,3, le poids à 0. Il monte de dt / 0,3 par image, puis se pose à 1, +0x10 et +0x14 à zéro.
	CAnimTrack c; c.reste = c.fondu = -0.3f;
	c.places[0].anim = &a; c.places[0].vitesse = 1.0f; c.places[0].drapeaux = 3;
	VERIF(c.Avancer(dt));
	VERIF(Pres(c.poids, 1.0f / 9.0f) && Pres(c.reste, -0.3f + dt) && c.fondu == -0.3f);
	VERIF(c.places[0].t == 0.0f && c.places[0].drapeaux == 7u);       // la première image de la place
	printf("entrée sur 0,3 s : %.3f", c.poids);
	int32 images = 1;
	while(c.reste != 0.0f && images < 100){ VERIF(c.Avancer(dt)); images++; if(images <= 4) printf(", %.3f", c.poids); }
	printf(" ... installée à la %de image, poids %.3f\n", images, c.poids);
	VERIF((images == 9 || images == 10) && c.poids == 1.0f && c.reste == 0.0f && c.fondu == 0.0f);
	VERIF(Pres(c.places[0].t, (images - 1) * dt));
	// Installée : le poids ne bouge plus, la place avance.
	VERIF(c.Avancer(dt) && c.poids == 1.0f && Pres(c.places[0].t, images * dt));

	// Un poids déjà à 1 pendant l'entrée : +0x10 et +0x14 à zéro, sans autre calcul.
	CAnimTrack d; d.poids = 1.0f; d.reste = d.fondu = -0.3f;
	VERIF(d.Avancer(dt) && d.poids == 1.0f && d.reste == 0.0f && d.fondu == 0.0f);

	// La sortie : +0x10 = +0x14 = 0,23, le poids descend de dt / 0,23 par image ; à la fin tout à zéro et faux.
	CAnimTrack s; s.poids = 1.0f; s.reste = s.fondu = 0.23f;
	s.places[0].anim = &a; s.places[0].vitesse = 1.0f; s.places[0].drapeaux = 7;
	VERIF(s.Avancer(dt));
	VERIF(Pres(s.poids, 1.0f - dt / 0.23f) && Pres(s.reste, 0.23f - dt) && Pres(s.places[0].t, dt));
	images = 1;
	float t = 0;
	for(;;){ t = s.places[0].t; images++; if(!s.Avancer(dt) || images >= 100) break; }
	printf("sortie sur 0,23 s : finie à la %de image\n", images);
	VERIF(images == 7 || images == 8);
	VERIF(s.poids == 0.0f && s.reste == 0.0f && s.fondu == 0.0f);
	VERIF(s.places[0].t == t);                                      // l'image qui rend faux n'avance pas les places

	// Une sortie qui commence sous 0,001 de poids : faux tout de suite, le poids n'est pas remis à zéro.
	CAnimTrack f; f.poids = 0.0005f; f.reste = f.fondu = 0.3f;
	VERIF(!f.Avancer(dt) && f.poids == 0.0005f && f.reste == 0.0f && f.fondu == 0.0f);

	// Les deux places : la place 1 n'est active que si le mélange est positif, la place 0 que s'il est sous 1 ; aucune
	// si le poids de la couche est nul. Les deux avancent quand même.
	int32 vus[2] = {0, 0};
	auto suite = [&](int32 i, float){ vus[i]++; };
	CAnimTrack m; m.poids = 1.0f;
	for(int32 i = 0; i < 2; i++){ m.places[i].anim = &a; m.places[i].vitesse = 1.0f; m.places[i].drapeaux = 7; }
	m.Avancer(dt, suite);
	VERIF(vus[0] == 1 && vus[1] == 0);
	m.melange = 0.25f; m.Avancer(dt, suite);
	VERIF(vus[0] == 2 && vus[1] == 1);
	m.melange = 1.0f; m.Avancer(dt, suite);
	VERIF(vus[0] == 2 && vus[1] == 2);
	m.poids = 0.0f; m.melange = 0.5f; m.Avancer(dt, suite);
	VERIF(vus[0] == 2 && vus[1] == 2);
	VERIF(Pres(m.places[0].t, 4 * dt) && Pres(m.places[1].t, 4 * dt));
	printf("mélange : la place 0 active %d fois, la place 1 %d fois sur quatre images\n", vus[0], vus[1]);
}

static void
Lancements(void)
{
	CHxdAnim a; a.duree = 2.0f; a.fondu = 0.3f;
	const float dt = 1.0f / 30.0f;

	// Un fondu négatif prend celui de l'enregistrement : l'entrée sur 0,3 s, le poids à 0, la place à 0 qui joue et boucle.
	CAnimTrack c;
	c.Lancer(&a, 2, 0.0f, 0, 1.0f, -1.0f);
	VERIF(c.poids == 0.0f && c.reste == -0.3f && c.fondu == -0.3f);
	VERIF(c.places[0].anim == &a && c.places[0].t == 0.0f && c.places[0].vitesse == 1.0f && c.places[0].drapeaux == 3u);
	// La première image : le poids monte déjà, l'instant attend ; la deuxième : l'instant vaut un pas.
	c.Avancer(dt);
	VERIF(Pres(c.poids, 1.0f / 9.0f) && c.places[0].t == 0.0f);
	c.Avancer(dt);
	VERIF(Pres(c.poids, 2.0f / 9.0f) && Pres(c.places[0].t, dt));
	printf("lancement, fondu de l'enregistrement : poids %.3f puis %.3f, instant 0 puis %.4f\n", 1.0f / 9.0f, c.poids, c.places[0].t);

	// Le bit 0 annule le fondu : installée tout de suite. Sans le bit 1 la place ne boucle pas ; le bit 3 passe.
	CAnimTrack d;
	d.Lancer(&a, 1 | 8, 0.0f, 0, 1.0f, 0.5f);
	VERIF(d.poids == 1.0f && d.reste == 0.0f && d.fondu == 0.0f && d.places[0].drapeaux == 9u);
	// Un fondu donné : l'entrée sur ce fondu.
	d.Lancer(&a, 0, 0.0f, 0, 1.0f, 0.5f);
	VERIF(d.poids == 0.0f && d.reste == -0.5f && d.fondu == -0.5f && d.places[0].drapeaux == 1u);
	// Une vitesse négative part de la fin.
	d.Lancer(&a, 2, 0.0f, 0, -1.0f, 0.0f);
	VERIF(d.places[0].t == 2.0f && d.places[0].vitesse == -1.0f && d.poids == 1.0f);
	d.Avancer(dt); d.Avancer(dt);
	VERIF(Pres(d.places[0].t, 2.0f - dt));

	// La place 1 reçoit le mélange, la place 0 non ; la place 1 relance aussi le fondu de la couche.
	CAnimTrack m;
	m.Lancer(&a, 3, 0.7f, 0, 1.0f, 0.0f);
	VERIF(m.melange == 0.0f && m.poids == 1.0f);
	m.Lancer(&a, 2, 0.7f, 1, 1.5f, 0.2f);
	VERIF(m.melange == 0.7f && m.places[1].anim == &a && m.places[1].vitesse == 1.5f && m.places[1].drapeaux == 3u);
	VERIF(m.poids == 0.0f && m.reste == -0.2f);
	printf("place 1 : mélange %.1f, la couche repart du poids %.0f sur %.1f s\n", m.melange, m.poids, -m.fondu);
}

static void
Lecteur(void)
{
	CHxdAnim a; a.duree = 2.0f; a.fondu = 0.3f;
	const float dt = 1.0f / 30.0f;

	// Deux couches : le rang 0 installé, le rang 2 qui entre sur 0,3 s.
	CLecteurAnim l;
	l.posee[0] = true; l.couches[0].Lancer(&a, 3, 0.0f, 0, 1.0f, 0.0f);
	l.posee[2] = true; l.couches[2].Lancer(&a, 2, 0.0f, 0, 1.0f, -1.0f); l.couches[2].masque = 4;
	l.compte = 2;
	for(int32 i = 0; i < 12; i++) l.ParImage(dt);
	VERIF(l.couches[2].poids == 1.0f && Pres(l.couches[0].places[0].t, 11 * dt) && Pres(l.couches[2].places[0].t, 11 * dt));

	// La sortie du rang 2 au fondu de son masque (0,3 s) : la couche reste, son poids descend, puis elle est retirée ;
	// une copie est gardée sur son rang à l'image du retrait, et vidée à l'image suivante (plus de couche sur ce rang).
	l.Sortir(2, -1.0f, 0.3f);
	VERIF(l.posee[2] && l.couches[2].reste == 0.3f && l.couches[2].fondu == 0.3f && l.couches[2].poids == 1.0f);
	int32 images = 0;
	while(l.posee[2] && images < 100){ l.ParImage(dt); images++; }
	printf("sortie du rang 2 sur 0,3 s : retirée à la %de image, %d couche(s) posée(s), %d gardée(s)\n",
		images, l.compte, (int32)l.gardees[2].size());
	VERIF((images == 9 || images == 10) && l.compte == 1 && l.gardees[2].size() == 1);
	VERIF(l.gardees[2][0].poids == 1.0f && l.gardees[2][0].places[0].anim == &a);
	VERIF(l.couches[2].places[0].anim == nil && l.couches[2].places[0].vitesse == 1.0f && l.couches[2].masque == -1);
	l.ParImage(dt);
	VERIF(l.gardees[2].empty());

	// Le rang 0 sort toujours sans fondu, et sans copie gardée.
	l.Sortir(0, 0.5f);
	VERIF(!l.posee[0] && l.compte == 0 && l.gardees[0].empty());
	// Une couche sans masque garde le -1, écrit tel quel (négatif : FUN_006b26f0 le lit comme une entrée) ; un fondu sous
	// 0,001 retire aussitôt.
	l.posee[3] = true; l.couches[3].Lancer(&a, 3, 0.0f, 0, 1.0f, 0.0f); l.compte = 1;
	l.Sortir(3, -1.0f, 0.3f);
	VERIF(l.posee[3] && l.couches[3].reste == -1.0f);
	l.Sortir(3, 0.0005f);
	VERIF(!l.posee[3] && l.gardees[3].size() == 1 && l.compte == 0);

	// Les gardées d'un rang : au plus neuf ; la dixième chasse la plus légère, la première des plus légères.
	CLecteurAnim g;
	g.posee[1] = true;
	for(int32 i = 0; i < 9; i++){ g.couches[1].poids = i == 3 || i == 6 ? 0.25f : 0.5f + 0.01f * i; g.couches[1].rang = i; g.Garder(1); }
	VERIF(g.gardees[1].size() == 9 && g.gardees[1][0].rang == 8 && g.gardees[1][8].rang == 0);
	g.couches[1].poids = 0.9f; g.couches[1].rang = 9; g.Garder(1);
	bool reste3 = false, reste6 = false;
	for(const CAnimTrack &c : g.gardees[1]){ if(c.rang == 3) reste3 = true; if(c.rang == 6) reste6 = true; }
	VERIF(g.gardees[1].size() == 9 && g.gardees[1][0].rang == 9 && reste3 && !reste6);
	printf("gardées : %d sur le rang 1 après dix copies, la plus légère chassée\n", (int32)g.gardees[1].size());
	// Une gardée reste figée pendant que sa couche avance.
	float t = g.gardees[1][0].places[0].t, w = g.gardees[1][0].poids;
	g.couches[1].Lancer(&a, 2, 0.0f, 0, 1.0f, 0.3f);
	g.ParImage(dt); g.ParImage(dt);
	VERIF(g.gardees[1].size() == 9 && g.gardees[1][0].places[0].t == t && g.gardees[1][0].poids == w);
}

static void
Poses(void)
{
	CHxdAnim a; a.duree = 2.0f; a.fondu = 0.3f;
	CHxdAnim b; b.duree = 1.0f; b.fondu = 0.2f;
	const float dt = 1.0f / 30.0f;
	CLecteurAnim l;
	// Une première animation sur le rang 0 : le compte monte, rien à garder.
	l.Poser(0, &a, 2, 0.0f, 0, 1.0f, 0.0f);
	VERIF(l.compte == 1 && l.posee[0] && l.gardees[0].empty() && l.couches[0].poids == 1.0f);
	// Remplacée avant sa première image : aucune copie gardée.
	l.Poser(0, &b, 2, 0.0f, 0, 1.0f, 0.0f);
	VERIF(l.compte == 1 && l.gardees[0].empty() && l.couches[0].places[0].anim == &b);
	// Après quelques images, l'animation remplacée est gardée, figée à son instant, et la nouvelle entre sur son fondu.
	for(int32 i = 0; i < 4; i++) l.ParImage(dt);
	float t = l.couches[0].places[0].t;
	VERIF(Pres(t, 3 * dt));
	l.Poser(0, &a, 2, 0.0f, 0, 1.0f, -1.0f);
	VERIF(l.compte == 1 && l.gardees[0].size() == 1 && l.gardees[0][0].places[0].anim == &b && l.gardees[0][0].places[0].t == t);
	VERIF(l.couches[0].places[0].anim == &a && l.couches[0].poids == 0.0f && l.couches[0].rang == 0);
	l.ParImage(dt); l.ParImage(dt);
	VERIF(Pres(l.couches[0].poids, 2.0f / 9.0f) && l.gardees[0][0].places[0].t == t && l.gardees[0][0].poids == 1.0f);
	printf("pose sur le rang 0 : la nouvelle au poids %.3f après deux images, la remplacée gardée à %.4f s\n", l.couches[0].poids, l.gardees[0][0].places[0].t);
	// Un rang libre : le compte monte, la couche porte son rang.
	l.Poser(3, &b, 0, 0.0f, 0, 1.0f, 0.5f);
	VERIF(l.compte == 2 && l.couches[3].rang == 3 && l.couches[3].places[0].drapeaux == 1u && l.couches[3].fondu == -0.5f);
}

static void
Synchros(void)
{
	// Deux animations aux marques à la même part de leur tour (en secondes : le double pour la cible deux fois plus longue) :
	// la même phase, à l'échelle de la durée de la cible.
	const float m[4] = { 0.0f, 0.25f, 0.5f, 0.75f }, m2[4] = { 0.0f, 0.5f, 1.0f, 1.5f };
	VERIF(Pres(CMotionTrack::Synchro(0.3f, 1.0f, m, 2.0f, m2), 0.6f));
	VERIF(Pres(CMotionTrack::Synchro(0.9f, 1.0f, m, 2.0f, m2), 1.8f));
	// Les mêmes marques en secondes sur une cible deux fois plus longue : sa dernière marque est à 0,375 de son tour.
	VERIF(Pres(CMotionTrack::Synchro(0.3f, 1.0f, m, 2.0f, m), 0.3f));
	// La source part de sa première marque (0,2 s) et sa dernière (0,1 s) est à 0,9 de son tour ; la cible a sa dernière à 0,25.
	const float s1[4] = { 0.2f, 0.4f, 0.6f, 0.1f }, c1[4] = { 0.0f, 0.1f, 0.2f, 0.5f };
	VERIF(Pres(CMotionTrack::Synchro(0.5f, 1.0f, s1, 2.0f, c1), 0.3f / 0.9f * 0.25f * 2.0f));
	VERIF(Pres(CMotionTrack::Synchro(0.15f, 1.0f, s1, 2.0f, c1), 1.25f));       // après la dernière marque : 0,25 + 0,5 × 0,75
	// La première marque de la cible décale le tout, et le tour se referme passé 1.
	const float c2[4] = { 1.0f, 0.1f, 0.2f, 0.5f };
	VERIF(Pres(CMotionTrack::Synchro(0.5f, 1.0f, s1, 2.0f, c2), 1.5f));         // 0,3 / 0,9 × 0,75 + 0,5
	VERIF(Pres(CMotionTrack::Synchro(0.15f, 1.0f, s1, 2.0f, c2), 0.75f));       // 0,75 + 0,5 × 0,25 + 0,5 − 1
	// Un instant passé la durée de la source est rendu tel quel ; une marque passée la durée y est ramenée.
	VERIF(CMotionTrack::Synchro(1.5f, 1.0f, m, 2.0f, m) == 1.5f);
	const float gr[4] = { 0.0f, 0.25f, 0.5f, 9.0f };
	VERIF(Pres(CMotionTrack::Synchro(0.5f, 1.0f, gr, 2.0f, gr), 1.0f));
	VERIF(Pres(CMotionTrack::Phase(0.5f, 0.0f, 0.5f, 0.25f), 0.25f));           // sur la marque : la branche d'après
	printf("synchro : 0,5 s d'une source d'1 s (marques 0,2 et 0,1) donne %.4f s d'une cible de 2 s (marques 0 et 0,5)\n", CMotionTrack::Synchro(0.5f, 1.0f, s1, 2.0f, c1));
}

static void
Angles(void)
{
	// Le remappage borné : c avant a, d à partir de b, la droite entre les deux.
	VERIF(CMotionTrack::Remapper(0.5f, 1.0f, 3.0f, 0.0f, 1.0f) == 0.0f);
	VERIF(CMotionTrack::Remapper(1.0f, 1.0f, 3.0f, 0.0f, 1.0f) == 0.0f);
	VERIF(Pres(CMotionTrack::Remapper(1.5f, 1.0f, 3.0f, 0.0f, 1.0f), 0.25f));
	VERIF(CMotionTrack::Remapper(3.0f, 1.0f, 3.0f, 0.0f, 1.0f) == 1.0f);
	VERIF(Pres(CMotionTrack::Remapper(2.0f, 1.0f, 3.0f, 10.0f, 20.0f), 15.0f));
	// L'arc cosinus borné.
	VERIF(CMotionTrack::ArcCosinus(2.0f) == 0.0f && CMotionTrack::ArcCosinus(1.0f) == 0.0f);
	VERIF(CMotionTrack::ArcCosinus(-1.0f) == 3.14159274f && CMotionTrack::ArcCosinus(-3.0f) == 3.14159274f);
	VERIF(Pres(CMotionTrack::ArcCosinus(0.0f), 1.5707964f));
	// L'angle de u à v sur un tour entier : le côté négatif passe au complément à 2π.
	const float x[3] = { 1, 0, 0 }, y[3] = { 0, 1, 0 }, my[3] = { 0, -1, 0 }, mx[3] = { -1, 0, 0 }, x5[3] = { 5, 0, 0 };
	VERIF(CMotionTrack::Angle(x, x) == 0.0f);
	VERIF(Pres(CMotionTrack::Angle(my, x), 1.5707964f));
	VERIF(Pres(CMotionTrack::Angle(y, x), 4.712389f));
	VERIF(Pres(CMotionTrack::Angle(mx, x), 3.14159274f));
	VERIF(Pres(CMotionTrack::Angle(my, x5), 1.5707964f));                       // v est normé par la fonction
	const float h[3] = { 0, -1, 7 };
	VERIF(Pres(CMotionTrack::Angle(h, x), 1.5707964f));                         // la hauteur de u n'est pas lue
	printf("angle : de (0, 1) à (1, 0) %.4f rad, de (0, -1) à (1, 0) %.4f rad\n", CMotionTrack::Angle(y, x), CMotionTrack::Angle(my, x));
}

static void
Cadences(void)
{
	// La vitesse naturelle : le trajet à plat sur la durée ; une durée nulle rend 0.
	VERIF(Pres(CMotionTrack::VitesseNaturelle(0.8f, 3.0f, 4.0f), 6.25f));
	VERIF(CMotionTrack::VitesseNaturelle(0.0f, 3.0f, 4.0f) == 0.0f);
	VERIF(CMotionTrack::VitesseNaturelle(0.00001f, 3.0f, 4.0f) == 0.0f);
	VERIF(CMotionTrack::VitesseNaturelle(-2.0f, 0.0f, 4.0f) == -2.0f);
	// La cadence : un piéton à 3 m/s sur une course faite pour 6 m/s la joue à la moitié de sa vitesse, fois sa stat.
	VERIF(Pres(CMotionTrack::Cadence(3.0f, 6.0f, 1.0f), 0.5f));
	VERIF(Pres(CMotionTrack::Cadence(9.0f, 6.0f, 1.2f), 1.8f));
	VERIF(CMotionTrack::Cadence(-1.0f, 6.0f, 1.2f) == 1.2f);                    // une vitesse négative : la stat seule
	VERIF(CMotionTrack::Cadence(3.0f, 0.0f, 1.2f) == 1.2f);                     // sans vitesse naturelle : la stat seule
	VERIF(CMotionTrack::Cadence(0.0f, 6.0f, 1.2f) == 0.0f);                     // à l'arrêt la boucle ne tourne pas
	printf("cadence : à 3 m/s sur une boucle de %.2f m/s, la vitesse de la place est %.3f\n", CMotionTrack::VitesseNaturelle(0.8f, 3.0f, 4.0f), CMotionTrack::Cadence(3.0f, 6.25f, 1.0f));
}

static void
Melanges(void)
{
	// FUN_006bcc60 sans masque : 1 copie la seconde pose, moins d'un millième garde la première, le reste mêle.
	VERIF(CPoseMelange::SansMasque(1.0f) == CPoseMelange::COPIER);
	VERIF(CPoseMelange::SansMasque(0.0005f) == CPoseMelange::GARDER && CPoseMelange::SansMasque(-0.0005f) == CPoseMelange::GARDER);
	VERIF(CPoseMelange::SansMasque(0.001f) == CPoseMelange::MELER && CPoseMelange::SansMasque(0.5f) == CPoseMelange::MELER);
	// Avec un masque : le poids de l'os, -1 au-delà du masque, fois le poids donné s'il n'est pas 1.
	const float masque[2] = {1.0f, 0.25f};
	VERIF(CPoseMelange::PoidsOs(1.0f, masque, 2, 0) == 1.0f && CPoseMelange::PoidsOs(1.0f, masque, 2, 1) == 0.25f);
	VERIF(CPoseMelange::PoidsOs(0.5f, masque, 2, 0) == 0.5f && CPoseMelange::PoidsOs(0.5f, masque, 2, 1) == 0.125f);
	VERIF(CPoseMelange::PoidsOs(1.0f, masque, 2, 5) == -1.0f && CPoseMelange::PoidsOs(0.5f, masque, 2, 5) == -0.5f);
	VERIF(CPoseMelange::AvecMasque(1.0f) == CPoseMelange::COPIER && CPoseMelange::AvecMasque(0.0f) == CPoseMelange::GARDER && CPoseMelange::AvecMasque(-1.0f) == CPoseMelange::MELER);
	// L'ordre d'évaluation d'un rang : les gardées de la plus ancienne (la queue) à la plus récente (la tête), puis la couche.
	CHxdAnim a; a.duree = 2.0f; a.fondu = 0.3f;
	CHxdAnim b; b.duree = 1.0f; b.fondu = 0.2f;
	const float dt = 1.0f / 30.0f;
	CLecteurAnim l;
	std::vector<CLecteurAnim::Evaluee> ordre;
	VERIF(l.OrdreDuRang(0, ordre) == 0);
	l.Poser(0, &a, 2, 0.0f, 0, 1.0f, 0.0f);
	for(int32 i = 0; i < 3; i++){ l.ParImage(dt); l.PurgerInstallees(); }
	VERIF(l.OrdreDuRang(0, ordre) == 1 && !ordre[0].gardee && ordre[0].couche == &l.couches[0]);
	l.Poser(0, &b, 2, 0.0f, 0, 1.0f, -1.0f);
	l.ParImage(dt); l.PurgerInstallees();
	l.ParImage(dt); l.PurgerInstallees();
	l.Poser(0, &a, 2, 0.0f, 0, 1.0f, -1.0f);
	l.ParImage(dt); l.PurgerInstallees();
	VERIF(l.OrdreDuRang(0, ordre) == 3);
	VERIF(ordre.size() == 3 && ordre[0].gardee && ordre[0].couche == &l.gardees[0][1] && ordre[0].couche->places[0].anim == &a);
	VERIF(ordre.size() == 3 && ordre[1].gardee && ordre[1].couche == &l.gardees[0][0] && ordre[1].couche->places[0].anim == &b);
	VERIF(ordre.size() == 3 && !ordre[2].gardee && ordre[2].couche == &l.couches[0] && ordre[2].poids == l.couches[0].poids);
	printf("mélange : trois évaluations au rang 0, la gardée la plus ancienne (poids %.3f), la plus récente (poids %.3f), puis la couche (poids %.3f)\n",
	       ordre.size() == 3 ? ordre[0].poids : -1.0f, ordre.size() == 3 ? ordre[1].poids : -1.0f, ordre.size() == 3 ? ordre[2].poids : -1.0f);
	// L'évaluation d'une couche : une AM_AnimTrack par sa place 0, une AM_MotionTrack par ses deux places, la seconde au poids +0x40.
	CAnimTrack::Evaluation ev[2];
	VERIF(l.couches[0].Evaluations(ev) == 1 && ev[0].place == 0 && ev[0].poids == 1.0f);
	CAnimTrack mt = CAnimTrack::Motion();
	mt.melange = 0.25f;
	VERIF(mt.Evaluations(ev) == 2 && ev[0].place == 0 && ev[0].poids == 0.75f && ev[1].place == 1 && ev[1].poids == 0.25f);
	// Installée (poids 1), la couche est évaluée seule.
	for(int32 i = 0; i < 12; i++) l.ParImage(dt);
	VERIF(l.OrdreDuRang(0, ordre) == 1 && !ordre[0].gardee);
}

static void
EvaluationsDesPlaces(void)
{
	// FUN_006b6e50 : une place vide n'est pas évaluée ; une place qui pèse moins d'un millième perd son bit 2.
	CHxdAnim a; a.duree = 0.09f; a.fondu = 0.0f; a.drapeaux = 4;
	CAnimTrack mt = CAnimTrack::Motion();
	VERIF(!mt.PlaceEvaluee(0) && !mt.PlaceEvaluee(1));
	mt.places[0].anim = &a; mt.places[0].drapeaux = 3;
	mt.places[1].anim = &a; mt.places[1].drapeaux = 7;
	mt.melange = 0.0f;
	VERIF(mt.Evaluer() == 1 && (mt.places[0].drapeaux & CPlaceAnim::PARTIE) && !(mt.places[1].drapeaux & CPlaceAnim::PARTIE));
	mt.melange = 0.5f;
	VERIF(mt.Evaluer() == 2 && (mt.places[1].drapeaux & CPlaceAnim::PARTIE));
	mt.melange = 1.0f;
	VERIF(mt.Evaluer() == 1 && !(mt.places[0].drapeaux & CPlaceAnim::PARTIE));
	// L'échelle du bassin : la norme du vecteur de l'os dans le squelette sur le réel du groupe ; l'os 2 sous le drapeau 0x200.
	VERIF(CAnimTrack::OsDuBassin(0) == 1 && CAnimTrack::OsDuBassin(0x200) == 2);
	VERIF(Pres(CAnimTrack::EchelleDuBassin(1.2f, 1.0f), 1.2f) && CAnimTrack::EchelleDuBassin(1.2f, 0.0005f) == 1.0f);
	// Une boucle dont l'enregistrement porte le drapeau 4 perd son bit 2 à chaque tour : sans évaluation, l'image d'après
	// n'avance pas ; l'évaluation le lui rend et l'image d'après avance.
	const float dt = 1.0f / 30.0f;
	CLecteurAnim l;
	l.Poser(0, &a, 2, 0.0f, 0, 1.0f, 0.0f);
	int32 n = 0;
	l.ParImage(dt); l.Evaluer();
	while((l.couches[0].places[0].drapeaux & CPlaceAnim::PARTIE) && n < 20){ l.ParImage(dt); n++; }
	VERIF(n == 3 && !(l.couches[0].places[0].drapeaux & CPlaceAnim::PARTIE));
	float t0 = l.couches[0].places[0].t;
	CLecteurAnim sans = l;
	sans.ParImage(dt);
	VERIF(sans.couches[0].places[0].t == t0);
	l.Evaluer();
	VERIF((l.couches[0].places[0].drapeaux & CPlaceAnim::PARTIE) != 0);
	l.ParImage(dt);
	VERIF(l.couches[0].places[0].t > t0);
	printf("évaluation : la boucle de 0.09 s fait son tour à la %de image (instant %.4f s) ; sans évaluation l'image suivante reste à %.4f s, avec elle avance à %.4f s\n",
	       n, t0, sans.couches[0].places[0].t, l.couches[0].places[0].t);
}

static void
Purges(void)
{
	CHxdAnim a; a.duree = 2.0f; a.fondu = 0.3f;
	CHxdAnim b; b.duree = 1.0f; b.fondu = 0.2f;
	const float dt = 1.0f / 30.0f;
	CLecteurAnim l;
	l.Poser(0, &a, 2, 0.0f, 0, 1.0f, 0.0f);
	for(int32 i = 0; i < 3; i++){ l.ParImage(dt); l.PurgerInstallees(); }
	l.Poser(0, &b, 2, 0.0f, 0, 1.0f, -1.0f);                                   // fondu de 0,2 s : six images
	l.ParImage(dt); l.PurgerInstallees();
	l.ParImage(dt); l.PurgerInstallees();
	// Remplacée pendant son fondu : deux gardées, que la purge laisse tant que la couche du rang est sous 1.
	l.Poser(0, &a, 2, 0.0f, 0, 1.0f, -1.0f);                                   // fondu de 0,3 s : neuf images
	int32 n = 0;
	while(l.couches[0].poids < 1.0f && n < 50){
		VERIF(l.gardees[0].size() == 2 && l.gardees[0][0].places[0].anim == &b);
		l.ParImage(dt); l.PurgerInstallees(); n++;
	}
	VERIF((n == 9 || n == 10) && l.gardees[0].empty() && l.compte == 1);
	printf("purge : deux gardées pendant le fondu, aucune à la %de image, la couche au poids %.3f\n", n, l.couches[0].poids);
	// Une couche sans fondu est installée aussitôt : rien ne reste gardé.
	l.Poser(0, &b, 2, 0.0f, 0, 1.0f, 0.0f);
	VERIF(l.gardees[0].size() == 1);
	l.PurgerInstallees();
	VERIF(l.gardees[0].empty());
	// Un rang sans couche n'est pas touché par cette purge (FUN_006c05b0 le vide de son côté).
	l.gardees[4].push_back(CAnimTrack());
	l.PurgerInstallees();
	VERIF(l.gardees[4].size() == 1);
	l.ParImage(dt);
	VERIF(l.gardees[4].empty());
}

static void
Phases(void)
{
	// L'instant d'une phase demandée (FUN_0072e690) : une boucle de 1 s, appuis à 0,2 s et 0,6 s. La phase 0 est le premier
	// appui, 0,5 le second, 0,25 le milieu des deux ; passé 0,5 le retour au premier appui en passant par la fin de la boucle.
	VERIF(Pres(CMotionTrack::InstantDeLaPhase(0.0f, 1.0f, 0.2f, 0.6f), 0.2f));
	VERIF(Pres(CMotionTrack::InstantDeLaPhase(0.5f, 1.0f, 0.2f, 0.6f), 0.6f));
	VERIF(Pres(CMotionTrack::InstantDeLaPhase(0.25f, 1.0f, 0.2f, 0.6f), 0.4f));
	VERIF(Pres(CMotionTrack::InstantDeLaPhase(0.75f, 1.0f, 0.2f, 0.6f), 0.9f));
	VERIF(Pres(CMotionTrack::InstantDeLaPhase(1.0f, 1.0f, 0.2f, 0.6f), 0.2f));   // le tour entier revient au premier appui
	// Le second appui avant le premier dans le fichier (les marques de C_PLAYER\RUN : 0,633 et 0,3) : la part du tour est
	// comptée en passant par la fin de la boucle.
	VERIF(Pres(CMotionTrack::InstantDeLaPhase(0.0f, 0.8f, 0.633f, 0.3f), 0.633f));
	VERIF(Pres(CMotionTrack::InstantDeLaPhase(0.5f, 0.8f, 0.633f, 0.3f), 0.3f));
	VERIF(Pres(CMotionTrack::InstantDeLaPhase(0.25f, 0.8f, 0.633f, 0.3f), 0.0665f));
	// La comparaison à une tolérance près (FUN_00432910) : l'écart égal à la tolérance passe.
	VERIF(CMotionTrack::Proche(1.0f, 1.05f, 0.06f) && CMotionTrack::Proche(1.05f, 1.0f, 0.06f));
	VERIF(!CMotionTrack::Proche(1.0f, 1.07f, 0.06f));
	VERIF(CMotionTrack::Proche(2.0f, 2.5f, 0.5f));
	// Le vecteur tourné autour de la verticale (FUN_005fbe50) : un quart de tour porte x sur y, la hauteur reste.
	float v[3] = { 1.0f, 0.0f, 7.0f };
	CMotionTrack::Tourner(1.5707964f, v);
	VERIF(Pres(v[0], 0.0f) && Pres(v[1], 1.0f) && v[2] == 7.0f);
	CMotionTrack::Tourner(-1.5707964f, v);
	VERIF(Pres(v[0], 1.0f) && Pres(v[1], 0.0f));
	printf("phase : sur C_PLAYER\\RUN (0,8 s supposé, appuis 0,633 et 0,3) la phase 0,25 est à %.4f s, la phase 0,75 à %.4f s\n",
	       CMotionTrack::InstantDeLaPhase(0.25f, 0.8f, 0.633f, 0.3f), CMotionTrack::InstantDeLaPhase(0.75f, 0.8f, 0.633f, 0.3f));
}

static void
Trajets(void)
{
	// Le trajet d'une animation (FUN_006073a0) : la dernière position de l'os 1 moins la première ; l'os 35 n'est pas lu.
	AgrAnim a; a.decodee = true; a.duree = 0.8f;
	AgrPosition p0{0.0f, CVector(0.1f, 0.2f, 0.9f)}, p1{0.4f, CVector(0.1f, 1.5f, 0.95f)}, p2{0.8f, CVector(0.1f, 3.4f, 0.9f)};
	a.positions[1] = { p0, p1, p2 };
	a.positions[35] = { p0, AgrPosition{0.8f, CVector(9.0f, 9.0f, 9.0f)} };
	CVector t = AgrTrajetRacine(a, 0);
	VERIF(Pres(t.x, 0.0f) && Pres(t.y, 3.2f) && Pres(t.z, 0.0f));
	// La vitesse naturelle qui en sort (FUN_00730340) : 3,2 m en 0,8 s.
	VERIF(Pres(CMotionTrack::VitesseNaturelle(a.duree, t.x, t.y), 4.0f));
	// Avec le drapeau 0x200 l'os 1 n'est pas la racine : trajet nul, vitesse naturelle nulle, la cadence est la stat.
	CVector n = AgrTrajetRacine(a, 0x200);
	VERIF(n.x == 0.0f && n.y == 0.0f && n.z == 0.0f);
	VERIF(CMotionTrack::Cadence(3.0f, CMotionTrack::VitesseNaturelle(a.duree, n.x, n.y), 1.1f) == 1.1f);
	// Une seule position, aucune, ou une animation non décodée : trajet nul.
	a.positions[1] = { p0 };
	VERIF(AgrTrajetRacine(a, 0).y == 0.0f);
	a.positions[1].clear();
	VERIF(AgrTrajetRacine(a, 0).y == 0.0f);
	a.positions[1] = { p0, p2 }; a.decodee = false;
	VERIF(AgrTrajetRacine(a, 0).y == 0.0f);
	{
		// Le déplacement d'une place (FUN_006b45d0). L'os : 1, 2 avec le drapeau 0x200, le dernier avec le drapeau 0x40.
		VERIF(AgrOsDuDeplacement(0, 36) == 1 && AgrOsDuDeplacement(0x200, 36) == 2 && AgrOsDuDeplacement(0x40, 36) == 35 && AgrOsDuDeplacement(0x240, 36) == 35);
		CVector ici(1.0f, 2.0f, 0.9f), av(0.5f, 1.0f, 0.8f), der(4.0f, 8.0f, 1.0f), pre(0.25f, 0.5f, 0.7f);
		// Sans tour de boucle : la différence ; la racine ne rend pas de hauteur.
		CVector d = AgrDeplacementPlace(ici, av, false, der, pre, true);
		VERIF(Pres(d.x, 0.5f) && Pres(d.y, 1.0f) && d.z == 0.0f);
		d = AgrDeplacementPlace(ici, av, false, der, pre, false);
		VERIF(Pres(d.x, 0.5f) && Pres(d.y, 1.0f) && Pres(d.z, 0.1f));
		// Au tour de boucle, la racine : (dernière − avant) + ici, la première image n'est pas retirée.
		d = AgrDeplacementPlace(ici, av, true, der, pre, true);
		VERIF(Pres(d.x, 4.5f) && Pres(d.y, 9.0f) && d.z == 0.0f);
		// Un autre os (le drapeau 0x40) : la première image est retirée de la dernière.
		d = AgrDeplacementPlace(ici, av, true, der, pre, false);
		VERIF(Pres(d.x, 4.25f) && Pres(d.y, 8.5f) && Pres(d.z, 0.4f));
		// Sur une animation : l'os 1 va de y = 0,2 à 3,4 en 0,8 s (p0, p1, p2 plus haut).
		AgrAnim b; b.decodee = true; b.duree = 0.8f; b.numOs = 36;
		b.positions[1] = { p0, p1, p2 };
		b.positions[35] = { AgrPosition{0.0f, CVector(0.0f, 1.0f, 0.0f)}, AgrPosition{0.8f, CVector(0.0f, 5.0f, 0.5f)} };
		CVector e;
		VERIF(AgrDeplacementImage(b, 0, 0.4f, 0.0f, true, &e) && Pres(e.x, 0.0f) && Pres(e.y, 1.3f) && e.z == 0.0f);
		// Le tour de boucle, de 0,7 s à 0,1 s : (3,4 − 2,925) + 0,525 ; sans boucle, la différence 0,525 − 2,925.
		VERIF(AgrDeplacementImage(b, 0, 0.1f, 0.7f, true, &e) && Pres(e.y, 1.0f));
		VERIF(AgrDeplacementImage(b, 0, 0.1f, 0.7f, false, &e) && Pres(e.y, -2.4f));
		// Le drapeau 0x40 : l'os 35, de y = 1 à 5 ; à mi-course 2 m et 0,25 m de haut ; au tour (5 − 1 − 4,5) + 1,5 = 1.
		VERIF(AgrDeplacementImage(b, 0x40, 0.4f, 0.0f, true, &e) && Pres(e.y, 2.0f) && Pres(e.z, 0.25f));
		VERIF(AgrDeplacementImage(b, 0x40, 0.1f, 0.7f, true, &e) && Pres(e.y, 1.0f) && Pres(e.z, 0.125f));
		// Le drapeau 0x200 : l'os 2, sans position ici.
		VERIF(!AgrDeplacementImage(b, 0x200, 0.4f, 0.0f, true, &e) && e.y == 0.0f);
		printf("déplacement d'une place : 1,30 m de 0 à 0,4 s ; au tour de boucle, de 0,7 à 0,1 s, %.2f m pour la racine comme pour l'os du drapeau 0x40\n", e.y);
	}
	printf("trajet : l'os 1 avance de %.2f m en %.1f s, vitesse naturelle %.2f m/s\n", t.y, 0.8f, CMotionTrack::VitesseNaturelle(0.8f, t.x, t.y));
}

static void
Directions(void)
{
	// La couche de locomotion neuve (FUN_00731470) : pas d'entrée, aucun instant précédent, aucune phase demandée.
	CAnimTrack c = CAnimTrack::Motion();
	VERIF(c.motion && c.nEntrees == 0 && c.courante == -1 && !c.synchro && c.precedent == -1.0f && c.demande == -1.0f);
	VERIF(c.places[0].vitesse == 1.0f && c.places[1].vitesse == 1.0f && c.places[0].anim == nil && c.poids == 0.0f);
	VERIF(!CAnimTrack().motion);
	// La direction lissée (FUN_00730110) : la première fois elle prend l'intention, mise à plat et normée.
	CMotionPieton p;
	p.intention[0] = 3.0f; p.intention[1] = 0.0f; p.intention[2] = 5.0f; p.intentionVitesse = 0.5f;
	p.avant[0] = 0.0f; p.avant[1] = 1.0f; p.avant[2] = 0.0f;
	c.Direction(0.033f, p);
	VERIF(Pres(c.dir[0], 1.0f) && Pres(c.dir[1], 0.0f) && c.dir[2] == 0.0f);
	VERIF(c.avant[1] == 1.0f);
	VERIF(Pres(c.vitesse, 1.5f));                                               // sans allure : la vitesse de l'intention × 3
	p.allure = 2.0f;
	c.Direction(0.033f, p);
	VERIF(c.vitesse == 2.0f);
	// Un pas de 300 × dt plus grand que l'écart : la direction prend l'intention d'un coup.
	p.intention[0] = 0.0f; p.intention[1] = 1.0f;
	c.Direction(0.033f, p);
	VERIF(Pres(c.dir[0], 0.0f) && Pres(c.dir[1], 1.0f));
	// Un pas plus petit que l'écart (dt de 0,001 s : 0,3 radian pour un quart de tour) : elle tourne vers l'intention, du
	// bon côté, dans les deux sens.
	p.intention[0] = 1.0f; p.intention[1] = 0.0f;
	c.Direction(0.001f, p);
	VERIF(Pres(c.dir[0], std::sin(0.3f)) && Pres(c.dir[1], std::cos(0.3f)));
	p.intention[0] = -1.0f;
	c.Direction(0.001f, p);
	VERIF(Pres(c.dir[0], 0.0f) && Pres(c.dir[1], 1.0f));
	c.Direction(0.001f, p);
	VERIF(Pres(c.dir[0], -std::sin(0.3f)) && Pres(c.dir[1], std::cos(0.3f)));
	// La mesure que la mise à jour compare aux seuils : l'angle de la direction à l'avant du piéton.
	p.intention[0] = 1.0f; p.intention[1] = 0.0f;
	c.Direction(0.033f, p);
	printf("direction : intention (1, 0), avant (0, 1) : direction (%.2f, %.2f), angle %.4f rad\n", c.dir[0], c.dir[1], CMotionTrack::Angle(c.dir, c.avant));
	VERIF(Pres(CMotionTrack::Angle(c.dir, c.avant), 1.5707964f));
}

static void
Meneuses(void)
{
	static CHxdAnim marche, course;
	marche.duree = 1.0f; course.duree = 0.8f;
	CLecteurAnim l;
	VERIF(l.Couche(0) == nil && l.Couche(7) == nil && l.Couche(-1) == nil && l.Meneuse(0) == nil);
	// Une couche de locomotion installée (FUN_006bcb70) sur un rang vide : le compte monte, rien n'est gardé.
	CAnimTrack m = CAnimTrack::Motion();
	m.entrees[0].hache = 1; m.entrees[0].enr = &marche; m.nEntrees = 1;
	l.Installer(0, m);
	VERIF(l.compte == 1 && l.posee[0] && l.Couche(0) == &l.couches[0] && l.couches[0].motion && l.couches[0].rang == 0);
	VERIF(l.gardees[0].empty());
	VERIF(l.Meneuse(0) == &l.couches[0] && l.Meneuse(1) == nil);
	// Une seconde couche de locomotion sur le même rang : la première est gardée, sa phase demandée n'est pas copiée.
	l.couches[0].poids = 1.0f; l.couches[0].demande = 0.25f; l.couches[0].precedent = 0.4f; l.couches[0].synchro = true;
	l.couches[0].places[0].t = 0.6f; l.ReleverMeneuses();
	CAnimTrack c = CAnimTrack::Motion();
	c.entrees[0].hache = 2; c.entrees[0].enr = &course; c.nEntrees = 1;
	c.reste = -0.5f; c.fondu = -0.5f;
	l.Installer(0, c);
	VERIF(l.compte == 1 && l.gardees[0].size() == 1 && l.gardees[0][0].motion && l.gardees[0][0].entrees[0].hache == 1);
	VERIF(l.gardees[0][0].demande == -1.0f && l.gardees[0][0].precedent == 0.4f && l.gardees[0][0].synchro && l.gardees[0][0].garde == 0.6f);
	VERIF(l.couches[0].entrees[0].hache == 2 && l.couches[0].poids == 0.0f);
	// La meneuse du rang (FUN_00731ae0) : la couche gardée tant que le poids de la nouvelle est sous 0,5, puis la nouvelle.
	VERIF(l.Meneuse(0) == &l.gardees[0][0]);
	l.couches[0].poids = 0.49f; VERIF(l.Meneuse(0) == &l.gardees[0][0]);
	l.couches[0].poids = 0.5f;  VERIF(l.Meneuse(0) == &l.couches[0]);
	// Une couche ordinaire sur le rang : la meneuse est la couche de locomotion gardée, s'il y en a une en tête.
	static CHxdAnim geste; geste.duree = 2.0f;
	l.couches[0].places[0].anim = &course; l.couches[0].places[0].drapeaux = 7;
	l.Poser(0, &geste, 0, 0.0f, 0, 1.0f, 0.2f);
	VERIF(!l.couches[0].motion && l.gardees[0].size() == 2 && l.gardees[0][0].entrees[0].hache == 2);
	VERIF(l.Meneuse(0) == &l.gardees[0][0]);
	l.gardees[0].insert(l.gardees[0].begin(), CAnimTrack());
	VERIF(l.Meneuse(0) == nil);
	printf("meneuse : couche gardée sous un poids de 0,5, puis la couche du rang\n");
}

static void
MisesAJour(void)
{
	static CHxdAnim marche, course, recul;
	marche.duree = 1.0f; course.duree = 0.8f; recul.duree = 1.0f;
	auto pres = [](float a, float b){ return std::fabs(a - b) < 1e-4f; };
	// Le même instant à un tour de boucle près (la synchro rend la fin de la boucle pour son début, à un arrondi près).
	auto tour = [](float a, float b, float d){ return std::fabs(std::remainder(a - b, d)) < 1e-4f; };
	const float dt = 0.1f;
	// Une couche d'une seule entrée, sans vitesse de piéton (−1) : la boucle tourne à la stat, en mode 7, sans mélange.
	CMotionPieton p;
	p.stat = 1.0f; p.intention[1] = 1.0f; p.intentionVitesse = 1.0f; p.depuisPas[0] = p.depuisPas[1] = 1.0f;
	CAnimTrack m = CAnimTrack::Motion();
	m.poids = 1.0f;
	m.entrees[0].hache = 1; m.entrees[0].enr = &marche; m.entrees[0].naturelle = 2.0f;
	m.entrees[0].marques[0] = m.entrees[0].marques[1] = 0.35f; m.entrees[0].marques[2] = m.entrees[0].marques[3] = 0.85f;
	m.nEntrees = 1;
	CLecteurAnim l;
	l.Installer(0, m);
	CAnimTrack &c = l.couches[0];
	l.ParImage(dt, p);
	VERIF(c.synchro && c.courante == 0 && c.places[0].anim == &marche && c.places[0].drapeaux == 7 && c.melange == 0.0f);
	VERIF(c.places[1].anim == nil && c.places[0].vitesse == 1.0f && tour(c.places[0].t, 0.1f, 1.0f) && tour(c.precedent, 0.0f, 1.0f) && tour(c.garde, 0.1f, 1.0f));
	VERIF(pres(c.dir[1], 1.0f) && c.vitesse == -1.0f);
	// Les appuis : la marque 0 (0,35 s) passée pose le pied 0, la marque 2 (0,85 s) le pied 1, une fois par tour.
	int32 pieds[2] = {0, 0}; float quand[2] = {-1, -1};
	for(int32 i = 1; i < 10; i++){
		l.ParImage(dt, p);
		for(int32 k = 0; k < 2; k++){
			p.depuisPas[k] += dt;
			if(p.pas[k]){ pieds[k]++; quand[k] = c.precedent; p.Appui(k, false); }
		}
	}
	VERIF(pieds[0] == 1 && pieds[1] == 1 && tour(quand[0], 0.4f, 1.0f) && tour(quand[1], 0.9f, 1.0f));
	VERIF(tour(c.places[0].t, 0.0f, 1.0f));                                      // un tour en dix images
	printf("mise à jour : boucle de 1 s à la stat, appui du pied 0 relevé à %.2f s (marque 0,35), du pied 1 à %.2f s (marque 0,85)\n", quand[0], quand[1]);
	// Un appui de moins de 0,1 s n'est pas reposé (FUN_005f3610).
	p.depuisPas[0] = 0.05f; p.Appui(0, true); VERIF(!p.pas[0]);
	p.depuisPas[0] = 0.1f;  p.Appui(0, true); VERIF(p.pas[0] && p.depuisPas[0] == 0.0f);
	p.Appui(0, false); VERIF(!p.pas[0]);
	// La cadence : le piéton à 3 m/s sur une marche faite pour 2 m/s, stat 1,2 : la place tourne à 1,8.
	p.vitesse = 3.0f; p.stat = 1.2f;
	l.ParImage(dt, p);
	VERIF(pres(c.places[0].vitesse, 1.8f) && c.vitesse == 3.0f);
	// Une phase demandée : la couche saute à la marque 2 pour la phase 0,5, et la demande retombe à −1.
	c.demande = 0.5f; p.vitesse = -1.0f; p.stat = 1.0f;
	l.ParImage(dt, p);
	VERIF(c.demande == -1.0f && tour(c.places[0].t, 0.95f, 1.0f) && tour(c.precedent, 0.85f, 1.0f));
	// Deux entrées, seuils 0 et π : une direction à un quart de tour de l'avant mélange les deux à parts égales ; à
	// trois quarts de tour, le tour revient à la première entrée (son seuil plus 2π).
	CAnimTrack d = CAnimTrack::Motion();
	d.poids = 1.0f; d.nEntrees = 2;
	d.entrees[0] = m.entrees[0];
	d.entrees[1] = m.entrees[0]; d.entrees[1].hache = 3; d.entrees[1].enr = &recul; d.entrees[1].seuil = 3.14159274f;
	CLecteurAnim l2;
	l2.Installer(0, d);
	CMotionPieton q;
	q.intention[0] = 1.0f; q.avant[0] = 0.0f; q.avant[1] = 1.0f;                 // direction (1, 0), avant (0, 1) : π/2
	l2.ParImage(dt, q);
	CAnimTrack &e = l2.couches[0];
	VERIF(e.courante == 0 && e.places[0].anim == &marche && e.places[1].anim == &recul && e.places[1].drapeaux == 7);
	VERIF(pres(e.melange, 0.5f) && tour(e.places[1].t, e.places[0].t, 1.0f) && e.places[1].vitesse == e.places[0].vitesse);
	q.intention[0] = -1.0f;                                                      // direction (−1, 0) : 3π/2
	l2.ParImage(dt, q);
	VERIF(e.courante == 1 && e.places[0].anim == &recul && e.places[1].anim == &marche && pres(e.melange, 0.5f));
	q.intention[0] = 0.0f; q.intention[1] = 1.0f;                                // droit devant : l'entrée 0 seule
	l2.ParImage(dt, q);
	VERIF(e.courante == 0 && e.melange == 0.0f);
	printf("mise à jour : deux entrées aux seuils 0 et π, direction à un quart de tour : mélange %.2f\n", 0.5f);
	// Une course installée sur la marche (fondu de 0,5 s) : la marche gardée mène tant que le poids de la course est sous
	// 0,5, la course se cale sur elle (ses marques aux mêmes parts du tour : la même part de sa durée) ; puis la course mène.
	CAnimTrack r = CAnimTrack::Motion();
	r.reste = -0.5f; r.fondu = -0.5f; r.nEntrees = 1;
	r.entrees[0].hache = 2; r.entrees[0].enr = &course; r.entrees[0].naturelle = 5.0f;
	r.entrees[0].marques[0] = r.entrees[0].marques[1] = 0.28f; r.entrees[0].marques[2] = r.entrees[0].marques[3] = 0.68f;
	float tm = c.places[0].t;
	l.Installer(0, r);
	VERIF(l.gardees[0].size() == 1 && l.Meneuse(0) == &l.gardees[0][0]);
	l.ParImage(dt, p);
	VERIF(tour(l.gardees[0][0].places[0].t, tm + 0.1f, 1.0f));                   // la marche gardée continue
	VERIF(l.couches[0].synchro && l.couches[0].places[0].anim == &course && pres(l.couches[0].poids, 0.2f));
	VERIF(tour(l.couches[0].precedent, tm * 0.8f, 0.8f));                              // calée sur l'instant relevé de la marche
	for(int32 i = 0; i < 2; i++) l.ParImage(dt, p);
	VERIF(pres(l.couches[0].poids, 0.6f) && l.Meneuse(0) == &l.couches[0]);
	float tc = l.couches[0].places[0].t;
	l.ParImage(dt, p);
	VERIF(tour(l.gardees[0][0].precedent, tc / 0.8f, 1.0f));                           // la marche gardée suit maintenant la course
	printf("mise à jour : la course se cale sur la marche gardée (%.3f s pour %.3f s), puis mène à partir du poids 0,5\n", tm * 0.8f, tm);
	// Sans entrée : rien n'avance.
	CAnimTrack v = CAnimTrack::Motion();
	CLecteurAnim l3; l3.Installer(0, v);
	VERIF(l3.AvancerMotion(l3.couches[0], dt, p) && !l3.couches[0].synchro);
}

static void
SansPose(void)
{
	// FUN_006b48c0 : le déplacement d'une couche sans pose.
	CHxdAnim avec, sans; avec.drapeaux = 4; sans.drapeaux = 0x40;
	CAnimTrack c;
	CVector d(9.0f, 9.0f, 9.0f);
	auto image = [](const CPlaceAnim &p){ return CVector(p.t, 2.0f * p.t, 0.0f); };
	VERIF(!c.Deplacement(false, image, &d) && d.x == 9.0f);              // pas d'animation : faux
	c.places[0].anim = &sans; c.places[0].t = 0.5f;
	VERIF(c.Deplacement(false, image, &d) && d.x == 9.0f);               // sans le bit 2 de l'enregistrement : vrai, rien d'écrit
	c.places[0].anim = &avec;
	VERIF(c.Deplacement(true, image, &d) && d.x == 9.0f);                // le lecteur gelé (bit 2 de +0x98) : vrai, rien d'écrit
	VERIF(c.Deplacement(false, image, &d) && Pres(d.x, 0.5f) && Pres(d.y, 1.0f) && d.z == 0.0f);
	// FUN_006bd150 : le rang 0 remplace, un rang plus haut mêle par le premier poids de son masque.
	CHxdFile h;
	h.poidsMasques = { {0.25f, 1.0f}, {1.0f, 0.0f}, {} };
	float w = 7.0f;
	VERIF(h.PremierPoids(-1, &w) && w == 0.25f);     // sans masque : le premier du dictionnaire
	VERIF(h.PremierPoids(1, &w) && w == 1.0f);
	VERIF(h.PremierPoids(2, &w) && w == -1.0f);      // un masque sans poids : −1
	VERIF(!h.PremierPoids(3, &w));
	CHxdFile vide;
	VERIF(!vide.PremierPoids(-1, &w));
	CLecteurAnim l;
	auto dep = [](const CAnimTrack &t, CVector *v){ *v = CVector(t.poids, 0.0f, 0.0f); };   // le « déplacement » d'essai : le champ poids
	auto pp = [&h](int32 m, float *x){ return h.PremierPoids(m, x); };
	CVector r = l.EvaluerSansPose(dep, pp);
	VERIF(r.x == 0.0f);                                                   // aucun rang
	l.posee[0] = true; l.couches[0].poids = 2.0f; l.gardees[0].push_back(CAnimTrack());
	r = l.EvaluerSansPose(dep, pp);
	VERIF(r.x == 2.0f && l.gardees[0].empty());                           // le rang 0 seul ; ses gardées sont vidées
	l.posee[3] = true; l.couches[3].poids = 6.0f; l.couches[3].masque = -1; l.gardees[3].push_back(CAnimTrack());
	r = l.EvaluerSansPose(dep, pp);
	VERIF(Pres(r.x, 2.0f * 0.75f + 6.0f * 0.25f) && l.gardees[3].empty()); // sans masque : le premier du dictionnaire, 0,25
	l.couches[3].masque = 1;
	VERIF(Pres(l.EvaluerSansPose(dep, pp).x, 6.0f));                      // poids 1 : le rang remplace
	l.couches[3].masque = 2;
	VERIF(Pres(l.EvaluerSansPose(dep, pp).x, 2.0f * 2.0f - 6.0f));        // masque sans poids : −1
	auto aucun = [&vide](int32 m, float *x){ return vide.PremierPoids(m, x); };
	VERIF(Pres(l.EvaluerSansPose(dep, aucun).x, 2.0f));                   // aucun masque : le rang est sauté
	l.posee[0] = false;
	VERIF(Pres(l.EvaluerSansPose(dep, pp).x, -6.0f));                     // sans rang 0 : mêlé à zéro
	printf("évaluation sans pose : le rang 0 à 2 et le rang 3 à 6 donnent %.2f par le premier masque (0,25), 6 au poids 1, -2 au masque sans poids\n", 2.0f * 0.75f + 6.0f * 0.25f);
}

static void
DeplacementDeLaPose(void)
{
	// Le déplacement dans 006be730 (CLecteurAnim::DeplacementDesRangs).
	typedef CLecteurAnim::DeplacementRang Rang;
	typedef CLecteurAnim::DeplacementGardee Gardee;
	Rang r[CLecteurAnim::RANGS];
	VERIF(CLecteurAnim::DeplacementDesRangs(r).x == 0.0f);
	// Une couche installée au rang 0, sans masque : son déplacement, fois son poids 1.
	r[0].posee = true; r[0].d = CVector(0.0f, 4.0f, 0.0f); r[0].poids = 1.0f;
	VERIF(Pres(CLecteurAnim::DeplacementDesRangs(r).y, 4.0f));
	// En fondu d'entrée sans gardée : le déplacement est multiplié par le poids.
	r[0].poids = 0.25f;
	VERIF(Pres(CLecteurAnim::DeplacementDesRangs(r).y, 1.0f));
	// Une gardée qui compte, sans masque, de poids 1 : G = D, puis V = G × 0,75 + V × 0,25.
	Gardee g; g.d = CVector(0.0f, 2.0f, 0.0f); g.poids = 1.0f; g.compte = true;
	r[0].gardees.push_back(g);
	VERIF(Pres(CLecteurAnim::DeplacementDesRangs(r).y, 2.0f * 0.75f + 4.0f * 0.25f));
	// La même gardée qui ne compte pas : S nul, drapeau baissé, elle est perdue et V = V × p.
	r[0].gardees[0].compte = false;
	VERIF(Pres(CLecteurAnim::DeplacementDesRangs(r).y, 1.0f));
	// Deux gardées, la plus ancienne d'abord : G = (D0 × 1) × (1 − 0,5) + D1 × 0,5.
	r[0].gardees[0].compte = true;
	Gardee h; h.d = CVector(0.0f, 6.0f, 0.0f); h.poids = 0.5f; h.compte = true;
	r[0].gardees.push_back(h);
	VERIF(Pres(CLecteurAnim::DeplacementDesRangs(r).y, (2.0f * 0.5f + 6.0f * 0.5f) * 0.75f + 4.0f * 0.25f));
	// Une gardée à masque : G = G × (1 − p) + D × (m × g), avec le poids p de la couche du rang.
	r[0].gardees.pop_back();
	r[0].gardees[0].masque = true; r[0].gardees[0].poidsOs = 0.5f;
	VERIF(Pres(CLecteurAnim::DeplacementDesRangs(r).y, (0.0f * 0.75f + 2.0f * 0.5f) * 0.75f + 4.0f * 0.25f));
	// Une couche installée (poids 1) ne lit pas ses gardées.
	r[0].poids = 1.0f;
	VERIF(Pres(CLecteurAnim::DeplacementDesRangs(r).y, 4.0f));
	// Un rang plus haut sans masque remplace tout ; avec un masque, il pèse le poids de l'os du bassin fois son poids.
	r[2].posee = true; r[2].d = CVector(1.0f, 0.0f, 0.0f); r[2].poids = 1.0f;
	CVector a = CLecteurAnim::DeplacementDesRangs(r);
	VERIF(Pres(a.x, 1.0f) && Pres(a.y, 0.0f));
	r[2].masque = true; r[2].poidsOs = 0.0f;
	a = CLecteurAnim::DeplacementDesRangs(r);
	VERIF(Pres(a.x, 0.0f) && Pres(a.y, 4.0f));                 // un masque du haut du corps (bassin à 0) : la marche reste
	r[2].poidsOs = 1.0f; r[2].poids = 0.5f;
	a = CLecteurAnim::DeplacementDesRangs(r);
	VERIF(Pres(a.x, 0.5f * 0.5f) && Pres(a.y, 4.0f * 0.5f));   // V = (1 ; 0) × 0,5 puis k = 0,5
	printf("déplacement de la pose : une couche à 4 m et sa gardée à 2 m au poids 0,25 donnent %.2f m ; un rang masqué sans le bassin laisse 4,00 m\n", 2.0f * 0.75f + 4.0f * 0.25f);
}

int
main(void)
{
	Places();
	Couches();
	Lancements();
	Lecteur();
	Poses();
	Synchros();
	Angles();
	Cadences();
	Phases();
	Trajets();
	SansPose();
	DeplacementDeLaPose();
	Directions();
	Meneuses();
	MisesAJour();
	Purges();
	Melanges();
	EvaluationsDesPlaces();
	printf("test_animtrack : %d échec(s)\n", echecs);
	return echecs ? 1 : 0;
}
