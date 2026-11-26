// Lit les dictionnaires d'animations : Anim/*.HXD et les entrées d'Anim/hxds.dat.
//   BULLY_DATA=<racine du jeu> build/tests/test_hxd
#include "../src/core/FileMgr.h"
#include "../src/anim/Hxd.h"
#include "../src/anim/AgrHxd.h"
#include "../src/core/ActionTree.h"
#include "../src/core/CdStream.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC : %s\n", #cond); echecs++; } }while(0)

static uint8 *
Lire(const char *nom, uint32 *n)
{
	const int32 MAX = 4 << 20;                 // le plus gros : Anim/C_Player.agr, 1,3 Mo
	uint8 *b = (uint8*)malloc(MAX);
	int32 k = CFileMgr::LoadFile(nom, b, MAX, "rb");
	if(k <= 0){ free(b); return nil; }
	*n = (uint32)k;
	return b;
}

int
main(void)
{
	static const char *const fichiers[] = { "ANIBBALL", "ANIFOOTY", "BANANA", "BATON", "BBGUN", "BIKE", "BOLTCUTT", "BROCKETL", "COPBIKE",
	                                        "MAINPED", "MOT_CTRL", "RAT_PED", "SCOOTER", "SIAMESE", "SK8BOARD", "SLINGSH", "SPRAYCAN", "SPUDG", "UMBRELLA", "WBALLOON" };
	int lus = 0, exacts = 0, anims = 0, hachagesJustes = 0;
	int groupes = 0, groupesSuivis = 0, rattachees = 0; bool etirementVu = false, deplacementVu = false;
	CHxdFile mainped;
	int32 monde = CdStream::AddImage("Stream\\World.img");
	for(const char *f : fichiers){
		char chemin[64]; snprintf(chemin, sizeof chemin, "Anim\\%s.HXD", f);
		uint32 n; uint8 *b = Lire(chemin, &n);
		VERIF(b != nil);
		if(!b) continue;
		CHxdFile h;
		if(h.Load(b, n)){
			lus++; exacts += h.lus == n; anims += (int)h.anims.size();
			for(const CHxdAnim &a : h.anims) hachagesJustes += a.hachage == ActionHash(a.nom.c_str());
			if(strcmp(f, "MAINPED") == 0) mainped = h;
			// Chaque groupe : son .agr (Anim\ ou World.img) se parcourt d'un trait avec
			// les tailles des animations rattachées, et finit exactement.
			for(size_t g = 0; g < h.groupes.size(); g++){
				char nom[64]; snprintf(nom, sizeof nom, "%s.agr", h.groupes[g].nom.c_str());
				uint32 na = 0; uint8 *agr = nil;
				char chemin2[96]; snprintf(chemin2, sizeof chemin2, "Anim\\%s", nom);
				agr = Lire(chemin2, &na);
				if(!agr && monde >= 0){
					const CDirectoryEntry *d = CdStream::ms_images[monde].Find(nom);
					if(d){
						na = d->size * CDSTREAM_SECTOR_SIZE; agr = (uint8*)malloc(na);
						int32 fd = CFileMgr::OpenFile("Stream\\World.img", "rb", 1);
						CFileMgr::Seek(fd, d->offset * CDSTREAM_SECTOR_SIZE, 0);
						if(!CFileMgr::ReadExact(fd, agr, na)){ free(agr); agr = nil; }
						CFileMgr::CloseFile(fd);
					}
				}
				if(!agr) continue;
				groupes++;
				uint32 q = 0; bool bon = true; int k = 0;
				for(const CHxdAnim &a : h.anims) if(a.groupe == g){
					uint32 m = 0; if(q + 4 <= na) memcpy(&m, agr + q, 4);
					if(m != 0x100){ bon = false; break; }
					q += a.taille - 4; k++;
				}
				for(uint32 r = q; r < na && bon; r++) if(agr[r]) bon = false;    // le reste : du bourrage
				if(bon && k){ groupesSuivis++; rattachees += k; }
				// Les animations se jouent sur la durée du HXD (FUN_006be250) : après
				// HxdEtirer, chaque animation décodée a celle de son enregistrement.
				if(strcmp(f, "MAINPED") == 0 && h.groupes[g].nom == "POI_Smoking"){
					std::vector<AgrAnim> v;
					if(AgrLireGroupe(agr, na, v) && v.size() > 2){
						float avant = v[2].duree;
						int32 n = HxdEtirer(v, h, "POI_Smoking");
						printf("POI_Smoking : SMK_WALL_LIGHT %.2f s dans le .agr, %.2f s après HxdEtirer (%d étirées)\n", avant, v[2].duree, n);
						VERIF(fabsf(avant - 3.0f) < 0.01f && fabsf(v[2].duree - 10.0f) < 0.01f && n >= 4);
						float tmax = 0; for(int32 o = 0; o < AGR_OS; o++) if(!v[2].pistes[o].empty()) tmax = fmaxf(tmax, v[2].pistes[o].back().t);
						VERIF(fabsf(tmax - 10.0f) < 0.05f);
						etirementVu = true;
					}
				}
				// Le déplacement d'une place (FUN_006b45d0) sur C_PLAYER\RUN : trois tours de boucle à 30 images par
				// seconde, contre trois fois le trajet (FUN_006073a0).
				if(strcmp(f, "MAINPED") == 0 && h.groupes[g].nom == "C_Player"){
					std::vector<AgrAnim> v;
					const CHxdAnim *run = h.Chercher(ActionHash("C_PLAYER\\RUN"));
					int32 rang = -1, i = 0;
					for(const CHxdAnim &a : h.anims) if(a.groupe == g){ if(&a == run) rang = i; i++; }
					if(run && rang >= 0 && AgrLireGroupe(agr, na, v) && (int32)v.size() > rang){
						HxdEtirer(v, h, "C_Player");
						const AgrAnim &a = v[rang];
						CVector tr = AgrTrajetRacine(a, run->drapeaux), somme, d;
						float t = 0.0f, pas = 1.0f / 30.0f; int32 tours = 0, images = 0; bool tous = true;
						while(tours < 3){
							float av = t; t += pas;
							if(t > a.duree){ t -= a.duree; tours++; }
							tous = AgrDeplacementImage(a, run->drapeaux, t, av, true, &d) && tous;
							somme.x += d.x; somme.y += d.y; somme.z += d.z; images++;
						}
						CVector p0; AgrPositionOs(a, AgrOsDuDeplacement(run->drapeaux, a.numOs), 0.0f, &p0);
						printf("C_PLAYER\\RUN (drapeaux %x, %.3f s, os %d) : trajet (%.3f ; %.3f) m ; %d images de déplacement sur trois tours : (%.3f ; %.3f ; %.3f) m, soit %.3f m par tour à plat ; première position de l'os (%.3f ; %.3f ; %.3f)\n",
						       run->drapeaux, a.duree, AgrOsDuDeplacement(run->drapeaux, a.numOs), tr.x, tr.y, images, somme.x, somme.y, somme.z,
						       sqrtf(somme.x * somme.x + somme.y * somme.y) / 3.0f, p0.x, p0.y, p0.z);
						VERIF(tous && images == 60 && AgrOsDuDeplacement(run->drapeaux, a.numOs) == 35);
						VERIF(fabsf(somme.y - 3.0f * tr.y) < 0.001f && fabsf(tr.y - 3.023f) < 0.001f);

						deplacementVu = true;
					}
				}
				free(agr);
			}
		}else printf("  illisible : %s\n", chemin);
		free(b);
	}
	printf("%d .HXD lus, %d exactement jusqu'au dernier octet ; %d animations, %d hachages justes\n", lus, exacts, anims, hachagesJustes);
	VERIF(lus == 20 && exacts == 20 && anims == 3913 && hachagesJustes == anims);
	VERIF(deplacementVu);
	{
		// Le poids que l'évaluation sans pose (FUN_006bd150) prend à chaque masque : le premier, celui de l'os 0.
		int32 un = 0, zero = 0, autre = 0, sans = 0; float w = 0.0f, w0 = -2.0f;
		for(size_t m = 0; m < mainped.masques.size(); m++){
			if(!mainped.PremierPoids((int32)m, &w)){ sans++; continue; }
			if(w == 1.0f) un++; else if(w == 0.0f) zero++; else autre++;
		}
		mainped.PremierPoids(-1, &w0);
		printf("premier poids des %zu masques de MAINPED.HXD : 1 pour %d, 0 pour %d, autre pour %d ; sans masque, celui du premier masque (%s) : %.2f\n",
		       mainped.masques.size(), un, zero, autre, mainped.masques.empty() ? "" : mainped.masques[0].c_str(), w0);
		VERIF(sans == 0 && un == 2 && zero == 42 && autre == 2 && w0 == 1.0f);
	}
	{
		// Les drapeaux du déplacement dans MAINPED.HXD : l'os pris (0x40, 0x200) et le vecteur ajouté (0x100).
		int32 n40 = 0, n200 = 0, n100 = 0, nb = 0;
		for(const CHxdAnim &a : mainped.anims){ n40 += (a.drapeaux & 0x40) != 0; n200 += (a.drapeaux & 0x200) != 0; n100 += (a.drapeaux & 0x100) != 0; nb += (a.drapeaux & 4) != 0; }
		printf("déplacement, drapeaux des %zu animations de MAINPED.HXD : 0x40 (le dernier os) %d, 0x200 (l'os 2) %d, 0x100 %d, bit 2 %d\n", mainped.anims.size(), n40, n200, n100, nb);
		VERIF(n40 == 1072 && n200 == 0 && n100 == 0 && nb == 1260);
		// La rotation de l'image : seule 006b6900 (les bits 6 et 7 ensemble, 0xc0) rend un quaternion à l'évaluation d'une
		// place (006b6e50) ; le bit 2 seul passe par 006b47b0, qui ne rend qu'une translation. Combien, et lesquelles.
		int32 nc0 = 0; std::string exemples;
		for(const CHxdAnim &a : mainped.anims) if((a.drapeaux & 0xc0) == 0xc0){ if(nc0 < 6) exemples += " " + a.nom; nc0++; }
		printf("rotation de l'image : %d animations de MAINPED.HXD portent les bits 6 et 7 (0xc0) :%s%s\n", nc0, exemples.c_str(), nc0 > 6 ? " …" : "");
		VERIF(nc0 == 553);

	}
	printf("%d groupes présents, %d parcourus exactement, %d animations rattachées\n", groupes, groupesSuivis, rattachees);
	VERIF(groupes == 423 && groupesSuivis == 423 && rattachees == 3913);
	// MAINPED : 46 masques, 36 os, 3 365 animations, 425 groupes.
	VERIF(mainped.masques.size() == 46 && mainped.os.size() == 36 && mainped.anims.size() == 3365 && mainped.groupes.size() == 425);
	// Les poids des masques (un par os) et leur recherche par nom, sans la casse : « FACIAL » des arbres est « Facial ».
	{
		bool trenteSix = mainped.poidsMasques.size() == 46;
		for(const std::vector<float> &w : mainped.poidsMasques) trenteSix = trenteSix && w.size() == 36;
		VERIF(trenteSix);
		int32 bras = mainped.Masque("RightArm"), visage = mainped.Masque("FACIAL"), defaut = mainped.Masque("default"), haut = mainped.Masque("UBO_Hd_Sp");
		VERIF(visage >= 0 && visage == mainped.Masque("Facial") && mainped.Masque("pas_un_masque") == -1);
		bool brasJuste = bras >= 0, visageJuste = visage >= 0, defautJuste = defaut >= 0, hautJuste = haut >= 0;
		for(int k = 0; k < 36 && brasJuste && visageJuste && defautJuste && hautJuste; k++){
			brasJuste = mainped.poidsMasques[bras][k] == (k >= 27 && k <= 34 ? 1.0f : 0.0f);
			visageJuste = mainped.poidsMasques[visage][k] == (k >= 15 && k <= 18 ? 1.0f : 0.0f);
			defautJuste = mainped.poidsMasques[defaut][k] == 1.0f;
			hautJuste = (k >= 10 && k <= 14) ? mainped.poidsMasques[haut][k] > 0.0f && mainped.poidsMasques[haut][k] < 1.0f : mainped.poidsMasques[haut][k] == 0.0f;
		}
		VERIF(brasJuste && visageJuste && defautJuste && hautJuste);
		// Le fondu de sortie des masques (le flottant après le compte des poids) : 0,3 s, sauf Conv et FlashLightAimHoriz à 1 s et FlashLightAimVert à 0 ;
		// FonduDeSortie (FUN_006bd320) : rien pour le rang 0, le champ 56 de la piste, à -1 le fondu du masque de la couche
		// (le premier du fichier sans nom), et rien sous 0,001 s.
		int trois = 0, un = 0;
		std::string nul;
		for(size_t k = 0; k < mainped.fonduMasques.size(); k++){ float x = mainped.fonduMasques[k]; if(x > 0.299f && x < 0.301f) trois++; else if(x == 1.0f) un++; else if(x == 0.0f) nul += (nul.empty() ? "" : ", ") + (k < mainped.masques.size() ? mainped.masques[k] : std::string("?")); }
		int32 conv = mainped.Masque("Conv"), lampe = mainped.Masque("FlashLightAimHoriz");
		VERIF(mainped.fonduMasques.size() == 46 && trois == 43 && un == 2 && !nul.empty() && nul.find(',') == std::string::npos && conv >= 0 && lampe >= 0 && mainped.fonduMasques[conv] == 1.0f && mainped.fonduMasques[lampe] == 1.0f);
		VERIF(mainped.FonduDeSortie(0, "Facial", 0.5f) == 0.0f && mainped.FonduDeSortie(3, "Facial", 0.5f) == 0.5f
		      && mainped.FonduDeSortie(3, "FACIAL", -1.0f) == mainped.fonduMasques[visage] && mainped.FonduDeSortie(2, "Conv", -1.0f) == 1.0f
		      && mainped.FonduDeSortie(1, "", -1.0f) == mainped.fonduMasques[0] && mainped.FonduDeSortie(1, "", 0.0f) == 0.0f
		      && mainped.FonduDeSortie(1, "", 0.0005f) == 0.0f);
		printf("fondu de sortie des masques : 0,3 s pour %d, 1 s pour %d (Conv, FlashLightAimHoriz), nul pour « %s »\n", trois, un, nul.c_str());
		printf("masques : RightArm sur les os 27 à 34, Facial (« FACIAL ») sur 15 à 18, UBO_Hd_Sp en partie sur 10 à 14, default partout\n");
	}
	// FUN_0068e380 : un os par son nom haché ; les deux que le binaire cherche (« Jaw », « Eyes ») et ceux des six sphères de Jimmy.
	VERIF(mainped.OsParNom("Jaw") == 15 && mainped.OsParNom("Eyes") == 18 && mainped.OsParNom("eyes") == 18 && mainped.OsParNom("Coude") == -1);
	VERIF(mainped.OsParNom("Pelvis") == 2 && mainped.OsParNom("Spine2") == 12 && mainped.OsParNom("Left Calf") == 4 && mainped.OsParNom("Right Calf") == 7 && mainped.OsParNom("EyeLids") == 16);
	printf("  les os par leur nom : Jaw %d, Eyes %d, Pelvis %d, Spine2 %d, Left Calf %d, Right Calf %d\n", mainped.OsParNom("Jaw"), mainped.OsParNom("Eyes"),
	       mainped.OsParNom("Pelvis"), mainped.OsParNom("Spine2"), mainped.OsParNom("Left Calf"), mainped.OsParNom("Right Calf"));
	const CHxdAnim *p = mainped.Chercher(0x6158a6b0);
	VERIF(p && p->nom == "C_PLAYER\\PUNCH_SLOP_1");
	// NPC_LOVE\KISS_HARD_B : 4,433 s, 5 044 octets dans NPC_Love.agr (n° 8), 4 de plus ici.
	const CHxdAnim *k = mainped.Chercher(ActionHash("NPC_LOVE\\KISS_HARD_B"));
	VERIF(k && fabsf(k->duree - 4.433f) < 0.01f && k->taille == 5044 + 4);
	int32 gk = -1;
	VERIF(mainped.Indice(ActionHash("NPC_LOVE\\KISS_HARD_B"), &gk) == 8 && gk >= 0 && mainped.groupes[gk].nom == "NPC_Love");
	// Un haché à plusieurs enregistrements : le jeu prend le premier dont le groupe est chargé (FUN_006b1c00) ; C_PLAYER\BALL_HOLD
	// est le n° 3 de W_Thrown.agr ou le n° 6 de 2_G2CarnivalDate.agr selon le groupe chargé, et rien sans groupe chargé.
	{
		uint32 h = ActionHash("C_PLAYER\\BALL_HOLD");
		auto par = [&](const char *g){ return mainped.ChercherCharge(h, [&](const CHxdAnim &a){ return a.groupe < mainped.groupes.size() && mainped.groupes[a.groupe].nom == g; }); };
		const CHxdAnim *lance = par("W_Thrown"), *foire = par("2_G2CarnivalDate"), *rien = par("pas_un_groupe");
		const CHxdAnim *tous = mainped.ChercherCharge(h, [](const CHxdAnim &){ return true; });
		VERIF(lance && foire && lance != foire && rien == nil && mainped.IndiceDe(lance) == 3 && mainped.IndiceDe(foire) == 6);
		VERIF(tous == mainped.Chercher(h) && mainped.IndiceDe(tous) == mainped.Indice(h));
		printf("C_PLAYER\\BALL_HOLD : le n° %d de W_Thrown.agr ou le n° %d de 2_G2CarnivalDate.agr selon le groupe chargé\n", mainped.IndiceDe(lance), mainped.IndiceDe(foire));
	}
	// Les groupes : Grap y compte 182 104 octets, le fichier 181 868, soit 4 de plus par animation (59).
	bool grap = false;
	for(const CHxdGroupe &g : mainped.groupes) if(g.nom == "Grap") grap = g.taille == 181868 + 4 * 59;
	VERIF(grap);

	// hxds.dat : un compte, puis des entrées « ANIM », taille, HXD.
	uint32 n; uint8 *b = Lire("Anim\\hxds.dat", &n);
	VERIF(b != nil);
	int entrees = 0, entreesExactes = 0, animsProps = 0;
	if(b){
		uint32 q = 4;
		while(q + 8 <= n){
			uint32 t; memcpy(&t, b + q + 4, 4);
			CHxdFile h;
			if(memcmp(b + q, "ANIM", 4) == 0 && q + 8 + t <= n && h.Load(b + q + 8, t)){ entreesExactes += h.lus == t; animsProps += (int)h.anims.size(); }
			entrees++; q += 8 + t;
		}
		VERIF(q == n);
		free(b);
	}
	printf("hxds.dat : %d entrées, %d lues exactement, %d animations\n", entrees, entreesExactes, animsProps);
	VERIF(entrees == 130 && entreesExactes == 130 && animsProps == 402);
	VERIF(etirementVu);
	printf("test_hxd : %s\n", echecs ? "ECHEC" : "ok");
	return echecs != 0;
}
