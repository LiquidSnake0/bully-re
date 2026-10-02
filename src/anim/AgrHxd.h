// Le passage des durées du HXD aux animations d'un .agr (FUN_006be250).
#pragma once
#include "Agr.h"
#include "Hxd.h"
#include <vector>

// Étire les animations du groupe `groupe` (dans l'ordre du .agr, docs/hxd.md)
// sur la durée de leur enregistrement HXD. Rend le nombre d'animations
// changées ; 0 si le groupe n'est pas dans ce HXD.
int32 HxdEtirer(std::vector<AgrAnim> &anims, const CHxdFile &h, const char *groupe);
