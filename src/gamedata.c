#include "gamedata.h"
#include "config.h"
#include "dataparse.h"
#include "items.h"
#include "quest.h"
#include "ui.h"

bool GameDataLoad(void)
{
    DataProblemReset();

    bool ok = true;
    if (!ItemsLoad(DATA_ITEMS))   ok = false;   /* per primi: gli altri li citano */
    if (!ShopLoad(DATA_SHOP))     ok = false;
    if (!RumorsLoad(DATA_RUMORS)) ok = false;

    return ok && DataProblemCount() == 0;
}
