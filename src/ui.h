/* ============================================================================
 * ui.h - Tutta l'interfaccia 2D: HUD, menu, inventario, diario, mappa, negozio.
 * ========================================================================== */
#ifndef UI_H
#define UI_H

#include "game.h"

extern const int SHOP_STOCK[];
extern const int SHOP_STOCK_COUNT;

void UIDrawHUD(Game *g);
void UIDrawCrosshair(Game *g);
void UIDrawMenu(Game *g);
void UIDrawPause(Game *g);
void UIDrawInventory(Game *g);
void UIDrawJournal(Game *g);
void UIDrawMap(Game *g);
void UIDrawDialogue(Game *g);
void UIDrawShop(Game *g);
void UIDrawDeath(Game *g);
void UIDrawWorldMarkers(Game *g);   /* etichette 3D->2D: nemici, indicatori */

#endif /* UI_H */
