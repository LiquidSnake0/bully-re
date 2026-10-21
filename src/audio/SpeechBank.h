// La parole enregistrée (docs/son.md, « Les fichiers de parole ») : audio\CONFIG\SpeechDB.bin, la liste des
// échantillons de chaque événement, et audio\PLAYLIST\Speech.bin, les 40 400 échantillons.
//
// SpeechDB.bin (chargé par FUN_005a5ea0 → FUN_0059f2f0) : le nombre d'événements (493, la table de
// SpeechEvents), puis pour chacun un en-tête de 8 octets (+0 son numéro, +2 le nombre d'éléments, +4 des
// drapeaux, +6 le dernier élément joué) et ses éléments de 4 octets : +0 la voix (le modèle du locuteur,
// l'octet que FUN_005a4980 place aux bits 9 à 16 de l'identifiant depuis le +0x10e du piéton), +1 la ligne,
// +2 le numéro de l'échantillon. FUN_0059ee10 retient les éléments de la bonne voix (FUN_0059e790) et de la
// bonne ligne (les 11 bits demandés comparés à l'octet), qui n'ont pas encore été joués ; FUN_0059f4d0 en
// tire un au hasard sans reprendre le dernier.
//
// Speech.bin (ouvert par FUN_005a6790, table lue par FUN_005a7b00 / FUN_005a7ab0) : « Hash », le nombre
// d'échantillons, puis une entrée de 12 octets chacun (+0 la clé, le hachage FUN_005a7c10 du nom de fichier,
// +4 la position, +8 la taille), triées par clé pour bsearch (FUN_005a7c80). Le numéro d'échantillon est
// l'indice dans cette table. Chaque échantillon est une banque de sons XACT (« SDBK ») suivie, à +0x800,
// d'une banque d'ondes (« WBND ») d'une onde : sa durée est dans l'entrée de l'onde (28 bits d'échantillons)
// et son format (ADPCM, fréquence sur 18 bits).
#pragma once
#include "../common.h"
#include <vector>

struct CSpeechItem { uint8 voix, ligne; uint16 echantillon; };
struct CSpeechSample { uint32 cle, position, taille; };

class CSpeechBank
{
public:
	std::vector<std::vector<CSpeechItem>> evenements;   // par numéro d'événement (SpeechEvents)
	std::vector<CSpeechSample> echantillons;             // la table de Speech.bin, dans l'ordre du fichier

	bool Charger(void);
	// FUN_0059ee10 sans le filtre « déjà joué » ni les voix partagées de FUN_0059e790 : les échantillons de
	// cet événement pour ce modèle et cette ligne, dans l'ordre du fichier.
	std::vector<int> Candidats(int evenement, int modele, int ligne) const;
	// La durée d'un échantillon en millisecondes, lue dans sa banque d'ondes ; -1 si illisible.
	int DureeMs(int echantillon) const;
	// FUN_005a7c10 : le hachage d'un nom de fichier (minuscules, « \ » lu « / », guillemets ôtés).
	static uint32 Cle(const char *nom);
};
