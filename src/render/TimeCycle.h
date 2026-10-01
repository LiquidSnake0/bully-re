// Le cycle jour-nuit : Config/timecyc*.dat (docs/timecycle.md).
//
// Format des GTA : par météo, 24 lignes (une par heure, minuit d'abord) de
// colonnes séparées par des blancs, documentées en tête de fichier. On lit les
// premières : lumière ambiante des objets / du monde / des piétons, soleil,
// contre-jour, ciel en haut et en bas, soleil (cœur, halo, taille), sprites,
// ombres, distance de vue (FarClp), début du brouillard (FogSt), NightFactor.
// Saisons : timecycF (automne, chapitre 1), timecycW (hiver), timecycS (été),
// sbtimecycS (printemps, sections SPRING_*) ; timecycE : couleurs spéciales.
#pragma once
#include "../common.h"
#include <string>
#include <vector>

struct CTimeCycleHeure {
	float ambObjets[3], ambMonde[3], ambPietons[3];
	float soleil[3], contreJour[3], contreJourInt;
	float cielHaut[3], cielBas[3];
	float soleilCoeur[3], soleilHalo[3], soleilTaille;
	float sprTaille, sprBrillance, ombre, ombreLumiere, ombrePoteaux;
	float farClp, fogSt, nightFactor;
};

class CTimeCycle
{
public:
	std::vector<std::string> meteos;                     // noms des sections (« Sunny », « CLOUDY »…)
	std::vector<std::vector<CTimeCycleHeure>> heures;    // [météo][0..23]

	bool Load(const char *texte, size_t taille);
	// L'état à l'heure h (en heures, 0 ≤ h < 24, fractions comprises),
	// interpolé linéairement entre l'heure pleine et la suivante.
	CTimeCycleHeure Etat(int32 meteo, float h) const;
};
