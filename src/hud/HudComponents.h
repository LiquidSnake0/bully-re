// Les composants du HUD de bully.exe : la table DAT_00c674a0, 48 places (0x30), que FUN_00514920 remplit au
// démarrage par la fabrique FUN_0053cee0 (un composant par indice : 0 HUDDetention, 1 HUDBigScriptText,
// 2 HUDSmallScriptText, 3 HUDMissionTimer, 4 HUDHealthMeter, 5 HUDEntityHealth, 6 HUDTimeCycle, 7 HUDNewspaper,
// 8 HUDGenericCounter, 9 HUDGenericText, 10 HUDRacing, 11 HUDRadar, 12 HUDSecondaryHealthBar, 13 HUDMoney,
// 14 HUDClothing, 15 HUDClockWarnings, 16 HUDPhoto, 17 HUDStruggleButton, 18 HUDBarber, 19 HUDTattoo, 20 HUDWeapons,
// 21 HUDButtonHistory, 22 HUDDodgeBall, 23 à 36 les invites sociales (Apologize, SmoothTalk, Flirt, Kiss, Follow,
// Taunt, Shove, Humiliate, Stop, Gift, Dismiss, Join, Greet, Burn), 37 HUDSocial, 38 HUDTaggingMeter, 39 HUDMonitor,
// 40 HUDTutorial, 41 CGlobe, 42 HUDText, 43 HUDScreenFade, 44 HUDDiary, 45 HUDInventory, 46 HUDSocialErrands).
//
// La base, HUDComponent (constructeur FUN_0053cc70, table 0x91920c) : +4 un mot à 1, +8 l'octet « visible » (0 à la
// construction), +0x14 l'octet « clignote », +0x15 l'octet « verrouillé », +0x1c un réel à 1. L'emplacement 4 de la
// table virtuelle (+0x10) dit si le composant est visible (FUN_00538f80 : l'octet +8) ; l'emplacement 5 (+0x14) le
// rend visible ou non (FUN_0053cd60). Deux composants ont leur propre emplacement 4 : HUDMoney (13, FUN_00542740) et
// HUDClockWarnings (15, FUN_0053c820), qui ne sont pas suivis ici (l'octet +8 seul).
#pragma once
#include "../common.h"

enum { HUD_COMPOSANTS = 0x30, HUD_ARGENT = 13, HUD_SOCIAL = 37 };

struct CHudComponent {
	bool visible = false;        // +8
	bool clignote = false;       // +0x14
	bool verrouille = false;     // +0x15
	int32 change = 1;            // +4 : remis à 1 à chaque changement de visibilité
};

class CHudComponents
{
public:
	// FUN_0053cc70 pour les 48 composants : tous cachés, ni clignotants ni verrouillés.
	CHudComponents(void);
	// L'emplacement 4 de la table virtuelle (FUN_00538f80) : l'octet +8. Faux hors de la table.
	bool Visible(int composant) const;
	// L'emplacement 5 (FUN_0053cd60) : pose l'octet +8 (le moteur appelle aussi l'emplacement 11 pour montrer, 12 pour
	// cacher, quand l'état change) ; +4 repasse à 1 ; caché, le composant ne clignote plus (+0x14 à 0).
	void PoserVisible(int composant, bool visible);
	// ToggleHUDComponentVisibility(n, oui) (0x5bd500) : l'emplacement 5 ; pour HUDMoney (13) rendu visible, le moteur
	// appelle aussi FUN_005426c0 (le montant affiché), non suivi.
	void Basculer(int composant, bool visible) { PoserVisible(composant, visible); }
	// ToggleHUDComponentLocked (0x5bd560) : l'octet +0x15 ; HUDComponentGetLocked (0x5bd5f0) le rend.
	void Verrouiller(int composant, bool oui);
	bool Verrouille(int composant) const;
	// HUDComponentGetFlashing (0x5bd5c0) : l'octet +0x14.
	bool Clignote(int composant) const;
	// HUDClearAllElements (0x5bdb80) : les 48 cachés par l'emplacement 5.
	void ToutCacher(void);
	// HUDSaveVisibility (0x5bdb70 → FUN_00514080(-1)) : la visibilité de chacun, sauf `sauf`, rangée (DAT_00c66d98),
	// puis le composant caché. HUDRestoreVisibility (0x5bdba0 → FUN_005140c0(-1)) : la visibilité rangée reposée.
	void Sauver(int sauf = -1);
	void Restaurer(int sauf = -1);
	// Le réglage d'une piste HUDComponentVisiblityTrack (table 0x93126c : lancement FUN_0061a940, mise à jour
	// FUN_006094f0 qui rend faux, donc arrêt dès la première image, FUN_00608ec0) : le composant +0x18 reçoit l'octet
	// +0x1c par l'emplacement 5, à l'arrêt.
	void Piste(int composant, bool visible) { PoserVisible(composant, visible); }
	CHudComponent composants[HUD_COMPOSANTS];
	bool ranges[HUD_COMPOSANTS] = {};   // DAT_00c66d98
};

// La condition HudComponentVisible (ConditionHudComponentVisible, table 0x9366f4, FUN_0065aee0) : l'emplacement 4 du
// composant +4 de la table. Elle garde les invites sociales de Player.act (GREET si 32, 35, 26, 25 ou 24) et les
// Social_Actions de GlobalLoco.act (37).
bool HudComponentVisible(const CHudComponents &hud, int32 composant);
