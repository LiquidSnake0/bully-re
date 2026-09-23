// CWorld : grille de secteurs, listes de secteur, liste des mobiles.
// Ce test ne lit aucun fichier du jeu : il vérifie que la recréation suit
// l'arithmétique du binaire (docs/world.md). Valeurs attendues calculées à
// la main depuis la formule inscrite en ligne dans CEntity::Add (0x465d60) :
//   indice = _ftol(coord / 50.0 + 18.0) borné à [0, 35]
// _ftol tronque vers zéro, ce qui n'est PAS un floor pour les négatifs.
//   build/tests/test_world
#include "../src/core/World.h"
#include "../src/entities/Physical.h"
#include <cstdio>

static int echecs = 0;
#define VERIF(cond) do{ if(!(cond)){ printf("ECHEC ligne %d : %s\n", __LINE__, #cond); echecs++; } }while(0)

// Entité concrète minimale. Add et Remove comptent les appels pour prouver
// que CWorld passe bien par les slots 2 et 3 quand l'entité n'est pas une
// grosse bâtisse, et ne les appelle pas quand elle en est une.
class CEntiteTest : public CPhysical
{
public:
	int nAdd, nRemove;
	CEntiteTest(void) : nAdd(0), nRemove(0) {
		bIsStatic = 0; bIsBIGBuilding = 0;
		bIsStaticWaitingForCollision = 0; m_field0xf0 = 0;
		m_type = ENTITY_TYPE_OBJECT; m_status = 0;
		m_scanCode = 0; m_modelIndex = 0;
		m_rwObject = nil; m_pAttachedObject = nil;
		m_pMovingListNode = nil;
		bHasPreRenderEffects = false; bFlag209_0 = false;
	}
	void Add(void) override { nAdd++; }
	void Remove(void) override { nRemove++; }
	void PureVirtual23(void) override {}
};

int
main(void)
{
	// --- Géométrie de la grille -------------------------------------
	VERIF(NUMSECTORS_X == 36 && NUMSECTORS_Y == 36);
	VERIF(CWorld::GetSectorIndexX(-900.0f) == 0);    // -18 + 18
	VERIF(CWorld::GetSectorIndexX(0.0f) == 18);
	VERIF(CWorld::GetSectorIndexX(49.9f) == 18);
	VERIF(CWorld::GetSectorIndexX(50.0f) == 19);
	VERIF(CWorld::GetSectorIndexX(875.0f) == 35);    // 17.5 + 18 = 35.5
	VERIF(CWorld::GetSectorIndexX(899.9f) == 35);
	// Bornage : au-delà du monde on reste sur le secteur de bord.
	VERIF(CWorld::GetSectorIndexX(5000.0f) == 35);
	VERIF(CWorld::GetSectorIndexX(-5000.0f) == 0);
	VERIF(CWorld::GetSectorIndexY(-25.0f) == 17);    // -0.5 + 18 = 17.5
	// Troncature vers zéro juste sous le bord bas : -901 donne -0.02,
	// tronqué à 0, et non -1 comme le ferait un floor.
	VERIF(CWorld::GetSectorIndexY(-901.0f) == 0);

	// Adressage : le binaire écrit &DAT_00c1b17c + (x + y*0x24)*5, soit
	// cinq mots par secteur et 0xb4 mots par rangée.
	VERIF(CWorld::GetSector(0, 0) == &CWorld::ms_aSectors[0][0]);
	VERIF(CWorld::GetSector(35, 35) == &CWorld::ms_aSectors[35][35]);
	{
		CSector *base = CWorld::GetSector(0, 0);
		VERIF(CWorld::GetSector(3, 7) == base + (3 + 7*36));
		VERIF(sizeof(CSector) == sizeof(CPtrList) * 5);
		VERIF(NUMSECTORENTITYLISTS == 5);
	}

	// --- Le switch de CEntity::Add ----------------------------------
	VERIF(CWorld::GetSectorListForType(ENTITY_TYPE_BUILDING) == ENTITYLIST_BUILDINGS);
	VERIF(CWorld::GetSectorListForType(ENTITY_TYPE_VEHICLE)  == ENTITYLIST_VEHICLES);
	VERIF(CWorld::GetSectorListForType(ENTITY_TYPE_PED)      == ENTITYLIST_PEDS);
	VERIF(CWorld::GetSectorListForType(ENTITY_TYPE_OBJECT)   == ENTITYLIST_OBJECTS);
	VERIF(CWorld::GetSectorListForType(ENTITY_TYPE_6)        == ENTITYLIST_OBJECTS);
	VERIF(CWorld::GetSectorListForType(ENTITY_TYPE_7)        == ENTITYLIST_OBJECTS);
	VERIF(CWorld::GetSectorListForType(ENTITY_TYPE_DUMMY)    == ENTITYLIST_DUMMIES);
	VERIF(CWorld::GetSectorListForType(ENTITY_TYPE_NOTHING)  == -1);
	// L'ordre des listes est celui du binaire : bâtiments, objets,
	// véhicules, piétons, dummies.
	VERIF(ENTITYLIST_BUILDINGS == 0 && ENTITYLIST_OBJECTS == 1);
	VERIF(ENTITYLIST_VEHICLES == 2 && ENTITYLIST_PEDS == 3 && ENTITYLIST_DUMMIES == 4);

	// --- Listes : insertion en tête, retrait par parcours ------------
	{
		CPtrList liste;
		int a, b, c;
		VERIF(liste.CountItems() == 0);
		liste.InsertItem(&a);
		liste.InsertItem(&b);
		liste.InsertItem(&c);
		VERIF(liste.CountItems() == 3);
		VERIF(liste.first->item == &c);              // insertion en tête
		VERIF(liste.FindItem(&a) != nil);
		VERIF(liste.FindItem((void*)0x1234) == nil);
		liste.RemoveItem(&b);                        // au milieu
		VERIF(liste.CountItems() == 2);
		VERIF(liste.FindItem(&b) == nil);
		liste.RemoveItem(&c);                        // en tête
		VERIF(liste.first->item == &a);
		liste.Flush();
		VERIF(liste.first == nil && liste.CountItems() == 0);
	}

	// --- CWorld::Add et CWorld::Remove -------------------------------
	{
		CEntiteTest ent;
		CWorld::Add(&ent);
		VERIF(ent.nAdd == 1);                        // passé par le slot 2
		VERIF(CWorld::GetBigBuildingList().CountItems() == 0);
		VERIF(ent.m_pMovingListNode == nil);         // m_field0xf0 nul
		CWorld::Remove(&ent);
		VERIF(ent.nRemove == 1);
	}
	{
		// Une grosse bâtisse court-circuite les slots et va dans la
		// liste globale 0xc216b8.
		CEntiteTest gros;
		gros.bIsBIGBuilding = 1;
		CWorld::Add(&gros);
		VERIF(gros.nAdd == 0);
		VERIF(CWorld::GetBigBuildingList().CountItems() == 1);
		VERIF(CWorld::GetBigBuildingList().FindItem(&gros) != nil);
		CWorld::Remove(&gros);
		VERIF(gros.nRemove == 0);
		VERIF(CWorld::GetBigBuildingList().CountItems() == 0);
	}

	// --- La liste des entités mobiles --------------------------------
	{
		CEntiteTest mob;
		mob.m_field0xf0 = 1;                         // remplace IsPhysical()
		CWorld::Add(&mob);
		VERIF(mob.m_pMovingListNode != nil);
		VERIF(CWorld::GetMovingEntityList().CountItems() == 1);
		// Deuxième Add : les gardes de 0x469680 interdisent le doublon.
		mob.AddToMovingList();
		VERIF(CWorld::GetMovingEntityList().CountItems() == 1);
		CWorld::Remove(&mob);
		VERIF(mob.m_pMovingListNode == nil);
		VERIF(CWorld::GetMovingEntityList().CountItems() == 0);
	}
	{
		// Une entité statique, ou qui attend sa collision, n'entre pas
		// dans la liste des mobiles même avec m_field0xf0 non nul.
		CEntiteTest fige;
		fige.m_field0xf0 = 1; fige.bIsStatic = 1;
		CWorld::Add(&fige);
		VERIF(fige.m_pMovingListNode == nil);
		CWorld::Remove(&fige);

		CEntiteTest attend;
		attend.m_field0xf0 = 1; attend.bIsStaticWaitingForCollision = 1;
		CWorld::Add(&attend);
		VERIF(attend.m_pMovingListNode == nil);
		CWorld::Remove(&attend);
		VERIF(CWorld::GetMovingEntityList().CountItems() == 0);
	}
	{
		// Le piège de 0x4696f0 : si le nœud retiré est celui que tient
		// le curseur de parcours, le curseur doit avancer AVANT le
		// décrochage, sinon la boucle du moteur perd sa liste.
		CEntiteTest a, b;
		a.m_field0xf0 = 1; b.m_field0xf0 = 1;
		a.AddToMovingList();
		b.AddToMovingList();                         // b est en tête
		CWorld::ms_pMovingListCursor = b.m_pMovingListNode;
		CPtrNode *apresB = b.m_pMovingListNode->next;
		b.RemoveFromMovingList();
		VERIF(CWorld::ms_pMovingListCursor == apresB);
		VERIF(CWorld::ms_pMovingListCursor == a.m_pMovingListNode);
		a.RemoveFromMovingList();
		VERIF(CWorld::GetMovingEntityList().CountItems() == 0);
		CWorld::ms_pMovingListCursor = nil;
	}

	// --- Scan codes ---------------------------------------------------
	{
		CWorld::ms_nCurrentScanCode = -1;
		CWorld::AdvanceCurrentScanCode();
		VERIF(CWorld::ms_nCurrentScanCode == 1);
		CWorld::AdvanceCurrentScanCode();
		VERIF(CWorld::ms_nCurrentScanCode == 2);
	}
	{
		// ClearScanCodes parcourt les cinq listes de chaque secteur.
		CEntiteTest ent;
		ent.m_type = ENTITY_TYPE_PED;
		ent.m_scanCode = 42;
		CWorld::GetSector(4, 9)->m_lists[ENTITYLIST_PEDS].InsertItem(&ent);
		CWorld::ClearScanCodes();
		VERIF(ent.m_scanCode == 0);
		CWorld::GetSector(4, 9)->m_lists[ENTITYLIST_PEDS].Flush();
	}

	// --- Entités ignorées ---------------------------------------------
	{
		CEntiteTest e[9];
		CWorld::ClearIgnoreEntities();
		VERIF(CWorld::ms_nNumIgnoreEntities == 0);
		for(int i = 0; i < 9; i++)
			CWorld::AddIgnoreEntity(&e[i]);
		VERIF(CWorld::ms_nNumIgnoreEntities == 7);   // saturé, pas de débordement
		CWorld::ClearIgnoreEntities();
		VERIF(CWorld::ms_nNumIgnoreEntities == 0);
	}

	printf(echecs ? "\n%d verification(s) en echec\n" : "\ntout passe\n", echecs);
	return echecs != 0;
}
