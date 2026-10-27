#include "HudComponents.h"

CHudComponents::CHudComponents(void)
{
	for(CHudComponent &c : composants) c = CHudComponent();
}

bool
CHudComponents::Visible(int composant) const
{
	return composant >= 0 && composant < HUD_COMPOSANTS && composants[composant].visible;
}

void
CHudComponents::PoserVisible(int composant, bool visible)
{
	if(composant < 0 || composant >= HUD_COMPOSANTS) return;
	CHudComponent &c = composants[composant];
	c.visible = visible;
	c.change = 1;
	if(!visible) c.clignote = false;
}

void
CHudComponents::Verrouiller(int composant, bool oui)
{
	if(composant >= 0 && composant < HUD_COMPOSANTS) composants[composant].verrouille = oui;
}

bool
CHudComponents::Verrouille(int composant) const
{
	return composant >= 0 && composant < HUD_COMPOSANTS && composants[composant].verrouille;
}

bool
CHudComponents::Clignote(int composant) const
{
	return composant >= 0 && composant < HUD_COMPOSANTS && composants[composant].clignote;
}

void
CHudComponents::ToutCacher(void)
{
	for(int i = 0; i < HUD_COMPOSANTS; i++) PoserVisible(i, false);
}

void
CHudComponents::Sauver(int sauf)
{
	for(int i = 0; i < HUD_COMPOSANTS; i++){
		if(i == sauf) continue;
		ranges[i] = Visible(i);
		PoserVisible(i, false);
	}
}

void
CHudComponents::Restaurer(int sauf)
{
	for(int i = 0; i < HUD_COMPOSANTS; i++)
		if(i != sauf) PoserVisible(i, ranges[i]);
}

bool
HudComponentVisible(const CHudComponents &hud, int32 composant)
{
	return hud.Visible(composant);
}
