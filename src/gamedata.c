#include "gamedata.h"
#include "config.h"
#include "balance.h"
#include "dataparse.h"
#include "entity.h"
#include "items.h"
#include "quest.h"
#include "ui.h"

bool GameDataLoad(void)
{
    DataProblemReset();

    bool ok = true;
    if (!BalanceLoad(DATA_BALANCE))        ok = false;
    if (!ItemsLoad(DATA_ITEMS))            ok = false;   /* prima degli altri: lo citano */
    if (!EntitiesLoadTypes(DATA_ENTITIES)) ok = false;   /* citano gli oggetti */
    if (!QuestsLoad(DATA_QUESTS))          ok = false;   /* citano oggetti ed entita' */
    if (!ShopLoad(DATA_SHOP))              ok = false;
    if (!RumorsLoad(DATA_RUMORS))          ok = false;

    return ok && DataProblemCount() == 0;
}
