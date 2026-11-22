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
	printf("trajet : l'os 1 avance de %.2f m en %.1f s, vitesse naturelle %.2f m/s\n", t.y, 0.8f, CMotionTrack::VitesseNaturelle(0.8f, t.x, t.y));
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
	Purges();
	printf("test_animtrack : %d échec(s)\n", echecs);
	return echecs ? 1 : 0;
}
