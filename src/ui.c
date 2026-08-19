#include "ui.h"
#include "dataparse.h"
#include "raymath.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

/* Palette dell'interfaccia (tonalita' fredde "nordiche"). */
static const Color UI_BG     = (Color){  18,  20,  26, 225 };
static const Color UI_PANEL  = (Color){  28,  32,  40, 240 };
static const Color UI_LINE   = (Color){  84,  96, 112, 255 };
static const Color UI_TEXT   = (Color){ 226, 228, 232, 255 };
static const Color UI_DIM    = (Color){ 150, 156, 166, 255 };
static const Color UI_GOLD   = (Color){ 214, 178,  94, 255 };
static const Color UI_HP     = (Color){ 176,  58,  58, 255 };
static const Color UI_STA    = (Color){  88, 150,  78, 255 };
static const Color UI_MP     = (Color){  70, 118, 180, 255 };

int SHOP_STOCK[MAX_SHOP_STOCK];
int SHOP_STOCK_COUNT = 0;

/* Il negozio e' un elenco di identificatori risolti in indici al caricamento:
 * un oggetto inesistente e' un errore, non una voce vuota nel listino. */
bool ShopLoad(const char *path)
{
    DataReader r;
    SHOP_STOCK_COUNT = 0;
    if (!DataOpen(&r, path)) return false;

    int before = DataProblemCount();
    while (DataNextSection(&r)) {
        if (strcmp(r.kind, "negozio") != 0) {
            DataProblem(&r, "sezione \"%s\" inattesa in questo file", r.kind);
            DataSkipSection(&r);
            continue;
        }
        char *key, *val;
        while (DataNextField(&r, &key, &val)) {
            if (strcmp(key, "oggetto") != 0) {
                DataProblem(&r, "chiave \"%s\" sconosciuta per il negozio", key);
                continue;
            }
            int id = ItemFind(val);
            if (id <= ITEM_NONE) {
                DataProblem(&r, "oggetto \"%s\" non definito in items.txt", val);
                continue;
            }
            if (SHOP_STOCK_COUNT >= MAX_SHOP_STOCK) {
                DataProblem(&r, "troppe voci: il massimo e' %d", MAX_SHOP_STOCK);
                continue;
            }
            SHOP_STOCK[SHOP_STOCK_COUNT++] = id;
        }
    }
    DataClose(&r);
    if (SHOP_STOCK_COUNT == 0) DataProblem(NULL, "%s: negozio vuoto", path);

    bool ok = (DataProblemCount() == before);
    if (ok) TraceLog(LOG_INFO, "DATI: %d voci di negozio da %s", SHOP_STOCK_COUNT, path);
    return ok;
}

/* ------------------------------------------------------------------------ */
/*  Helper di disegno                                                       */
/* ------------------------------------------------------------------------ */

static void Panel(int x, int y, int w, int h, const char *title)
{
    DrawRectangle(x, y, w, h, UI_PANEL);
    DrawRectangleLines(x, y, w, h, UI_LINE);
    if (title) {
        DrawRectangle(x, y, w, 30, (Color){ 40, 46, 58, 255 });
        DrawRectangleLines(x, y, w, 30, UI_LINE);
        DrawText(title, x + 12, y + 8, 18, UI_GOLD);
    }
}

static void Bar(int x, int y, int w, int h, float v, float maxv, Color c, const char *label)
{
    float t = (maxv > 0.0f) ? Clamp(v / maxv, 0.0f, 1.0f) : 0.0f;
    DrawRectangle(x, y, w, h, (Color){ 12, 14, 18, 200 });
    DrawRectangle(x, y, (int)(w * t), h, c);
    DrawRectangleLines(x, y, w, h, (Color){ 8, 10, 14, 255 });
    if (label) DrawText(label, x + 6, y + h / 2 - 5, 10, UI_TEXT);
}

/* Testo a capo automatico dentro una larghezza data. Ritorna l'altezza usata. */
static int WrapText(const char *txt, int x, int y, int maxW, int size, Color col)
{
    char line[256] = { 0 };
    char word[64];
    int  lineLen = 0, cursorY = y, wi = 0;

    for (int i = 0; ; i++) {
        char ch = txt[i];
        if (ch != ' ' && ch != '\0' && wi < 62) { word[wi++] = ch; continue; }
        word[wi] = '\0';

        char test[256];
        if (lineLen > 0) snprintf(test, sizeof(test), "%s %s", line, word);
        else             snprintf(test, sizeof(test), "%s", word);

        if (MeasureText(test, size) > maxW && lineLen > 0) {
            DrawText(line, x, cursorY, size, col);
            cursorY += size + 6;
            snprintf(line, sizeof(line), "%s", word);
        } else {
            snprintf(line, sizeof(line), "%s", test);
        }
        lineLen = (int)strlen(line);
        wi = 0;
        if (ch == '\0') break;
    }
    if (lineLen > 0) { DrawText(line, x, cursorY, size, col); cursorY += size + 6; }
    return cursorY - y;
}

/* ------------------------------------------------------------------------ */
/*  HUD                                                                      */
/* ------------------------------------------------------------------------ */

static void DrawCompass(Game *g)
{
    int cx = GetScreenWidth() / 2, y = 14, w = 420;
    DrawRectangle(cx - w / 2, y, w, 24, (Color){ 12, 14, 18, 150 });
    DrawRectangleLines(cx - w / 2, y, w, 24, (Color){ 60, 68, 80, 180 });

    const char *dirs[8] = { "N", "NE", "E", "SE", "S", "SO", "O", "NO" };
    for (int i = 0; i < 8; i++) {
        float ang = (float)i * (PI / 4.0f);
        float rel = ang - g->player.yaw;
        while (rel >  PI) rel -= 2.0f * PI;
        while (rel < -PI) rel += 2.0f * PI;
        if (fabsf(rel) > 1.25f) continue;
        int px = cx + (int)(rel * (w * 0.42f));
        DrawText(dirs[i], px - MeasureText(dirs[i], 16) / 2, y + 4, 16,
                 (i == 0) ? UI_GOLD : UI_TEXT);
    }

    /* Indicatore della cripta se la quest principale e' attiva. */
    if (g->quests[QUEST_BOSS].state == Q_ACTIVE) {
        Vector3 d = Vector3Subtract(g->world.cryptPos, g->player.pos);
        float ang = atan2f(d.x, d.z);
        float rel = ang - g->player.yaw;
        while (rel >  PI) rel -= 2.0f * PI;
        while (rel < -PI) rel += 2.0f * PI;
        if (fabsf(rel) < 1.25f) {
            int px = cx + (int)(rel * (w * 0.42f));
            DrawTriangle((Vector2){ (float)px, (float)(y + 22) },
                         (Vector2){ (float)px - 6.0f, (float)(y + 30) },
                         (Vector2){ (float)px + 6.0f, (float)(y + 30) },
                         (Color){ 220, 80, 80, 255 });
        }
    }
    DrawLine(cx, y, cx, y + 24, UI_GOLD);
}

/* Minimappa: ritaglio della texture del mondo attorno al giocatore. */
static void DrawMiniMap(Game *g)
{
    int size = 132;
    int x = GetScreenWidth() - size - 16, y = 44;
    float mapPx = (float)g->world.mapTex.width;
    float span  = 44.0f;   /* pixel di mappa mostrati */

    float u = (g->player.pos.x / WORLD_SIZE) * mapPx;
    float v = (g->player.pos.z / WORLD_SIZE) * mapPx;

    Rectangle src = { u - span * 0.5f, v - span * 0.5f, span, span };
    Rectangle dst = { (float)x, (float)y, (float)size, (float)size };

    DrawRectangle(x - 2, y - 2, size + 4, size + 4, (Color){ 12, 14, 18, 220 });
    DrawTexturePro(g->world.mapTex, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
    DrawRectangleLines(x - 2, y - 2, size + 4, size + 4, UI_LINE);

    /* Giocatore al centro + cono visivo. */
    Vector2 c = { x + size / 2.0f, y + size / 2.0f };
    DrawCircleV(c, 3.5f, UI_GOLD);
    Vector2 tip = { c.x + sinf(g->player.yaw) * 12.0f, c.y + cosf(g->player.yaw) * 12.0f };
    DrawLineV(c, tip, UI_GOLD);
    DrawText("MAPPA [M]", x, y + size + 6, 10, UI_DIM);
}

void UIDrawHUD(Game *g)
{
    Player *p = &g->player;
    int H = GetScreenHeight();

    /* Barre in basso a sinistra. */
    Bar(20, H - 84, 240, 18, p->hp,  p->maxHp,  UI_HP,  TextFormat("VITA %d/%d", (int)p->hp, (int)p->maxHp));
    Bar(20, H - 60, 240, 14, p->sta, p->maxSta, UI_STA, NULL);
    Bar(20, H - 40, 240, 14, p->mp,  p->maxMp,  UI_MP,  NULL);
    DrawText(TextFormat("Lv %d   %d/%d PE   %d oro", p->level, p->xp, p->xpNext, p->gold),
             20, H - 108, 16, UI_GOLD);
    DrawText(TextFormat("%s", ITEMS[p->weapon].name), 268, H - 60, 14, UI_DIM);
    DrawText(TextFormat("%s", (p->armor != ITEM_NONE) ? ITEMS[p->armor].name : "nessuna armatura"),
             268, H - 40, 14, UI_DIM);

    DrawCompass(g);
    DrawMiniMap(g);

    /* Quest attiva tracciata in alto a sinistra. */
    for (int i = 0; i < MAX_QUESTS; i++) {
        if (g->quests[i].state == Q_ACTIVE || g->quests[i].state == Q_READY) {
            DrawText(QUESTS[i].title, 20, 20, 18, UI_GOLD);
            DrawText(TextFormat("%s  %d/%d", QUESTS[i].objective,
                                g->quests[i].progress, QUESTS[i].target),
                     20, 42, 15, UI_TEXT);
            break;
        }
    }

    /* Ora del giorno + bioma. */
    int hh = (int)(g->timeOfDay * 24.0f) % 24;
    int mm = (int)((g->timeOfDay * 24.0f - hh) * 60.0f) % 60;
    Biome b = WorldBiomeAt(&g->world, p->pos.x, p->pos.z);
    DrawText(TextFormat("%02d:%02d  -  %s", hh, mm, WorldBiomeName(b)),
             GetScreenWidth() - 200, 20, 15, UI_DIM);

    /* Messaggi temporanei. */
    if (g->toastTimer > 0.0f) {
        int w = MeasureText(g->toast, 18) + 28;
        int x = GetScreenWidth() / 2 - w / 2;
        DrawRectangle(x, H - 160, w, 34, Fade(UI_BG, fminf(1.0f, g->toastTimer)));
        DrawRectangleLines(x, H - 160, w, 34, Fade(UI_LINE, fminf(1.0f, g->toastTimer)));
        DrawText(g->toast, x + 14, H - 150, 18, Fade(UI_TEXT, fminf(1.0f, g->toastTimer)));
    }
    if (g->subtitleTimer > 0.0f) {
        int w = MeasureText(g->subtitle, 16);
        DrawText(g->subtitle, GetScreenWidth() / 2 - w / 2, H - 200, 16,
                 Fade(UI_GOLD, fminf(1.0f, g->subtitleTimer)));
    }

    /* Flash rosso quando si subisce danno. */
    if (p->hurtFlash > 0.0f)
        DrawRectangle(0, 0, GetScreenWidth(), H,
                      Fade((Color){ 150, 20, 20, 255 }, p->hurtFlash * 0.45f));

    DrawText("[E] interagisci  [TAB] zaino  [J] diario  [M] mappa  [ESC] pausa",
             20, H - 22, 13, Fade(UI_DIM, 0.75f));
}

void UIDrawCrosshair(Game *g)
{
    int cx = GetScreenWidth() / 2, cy = GetScreenHeight() / 2;
    Color c = UI_TEXT;
    Entity *tgt = EntityLookedAt(g->ents, PlayerEye(&g->player),
                                 PlayerLookDir(&g->player), 3.6f, true);
    if (tgt) c = (Color){ 220, 90, 80, 255 };
    DrawLine(cx - 9, cy, cx - 3, cy, c);
    DrawLine(cx + 3, cy, cx + 9, cy, c);
    DrawLine(cx, cy - 9, cx, cy - 3, c);
    DrawLine(cx, cy + 3, cx, cy + 9, c);
}

/* ------------------------------------------------------------------------ */
/*  Marker 3D -> 2D                                                          */
/* ------------------------------------------------------------------------ */

void UIDrawWorldMarkers(Game *g)
{
    for (int i = 0; i < MAX_ENTITIES; i++) {
        Entity *e = &g->ents[i];
        if (!e->active || e->state == AI_DEAD) continue;
        float d = Vector3Distance(e->pos, g->cam.position);
        if (d > 30.0f) continue;

        Vector3 head = { e->pos.x, e->pos.y + e->height + 0.55f, e->pos.z };
        Vector3 rel  = Vector3Subtract(head, g->cam.position);
        Vector3 fwd  = Vector3Normalize(Vector3Subtract(g->cam.target, g->cam.position));
        if (Vector3DotProduct(rel, fwd) <= 0.0f) continue;

        Vector2 sp = GetWorldToScreen(head, g->cam);
        float alpha = Clamp(1.0f - d / 30.0f, 0.15f, 1.0f);

        if (e->hostile) {
            int w = 54, h = 5;
            float t = Clamp(e->hp / e->maxHp, 0.0f, 1.0f);
            DrawRectangle((int)sp.x - w / 2, (int)sp.y, w, h, Fade(BLACK, alpha * 0.6f));
            DrawRectangle((int)sp.x - w / 2, (int)sp.y, (int)(w * t), h, Fade(UI_HP, alpha));
            const char *n = e->name;
            DrawText(n, (int)sp.x - MeasureText(n, 12) / 2, (int)sp.y - 15, 12,
                     Fade(UI_TEXT, alpha));
        } else {
            const char *n = e->name;
            DrawText(n, (int)sp.x - MeasureText(n, 12) / 2, (int)sp.y, 12,
                     Fade(UI_GOLD, alpha));
        }
    }
}

/* ------------------------------------------------------------------------ */
/*  Schermate                                                                */
/* ------------------------------------------------------------------------ */

void UIDrawMenu(Game *g)
{
    int W = GetScreenWidth(), H = GetScreenHeight();
    DrawRectangleGradientV(0, 0, W, H, (Color){ 16, 22, 34, 255 }, (Color){ 40, 48, 62, 255 });

    const char *title = GAME_NAME;
    DrawText(title, W / 2 - MeasureText(title, 84) / 2, H / 4, 84, UI_GOLD);
    const char *sub = "un piccolo RPG open world in C + raylib";
    DrawText(sub, W / 2 - MeasureText(sub, 20) / 2, H / 4 + 96, 20, UI_DIM);

    const char *lines[] = {
        "[INVIO]  Nuova partita",
        "[C]      Carica partita",
        "[S]      Nuovo mondo (seme casuale)",
        "[ESC]    Esci",
    };
    for (int i = 0; i < 4; i++)
        DrawText(lines[i], W / 2 - 150, H / 2 + 40 + i * 34, 22, UI_TEXT);

    DrawText(TextFormat("seme del mondo: %u   -   v%s", g->world.seed, GAME_VERSION),
             W / 2 - 150, H - 60, 14, UI_DIM);
}

void UIDrawPause(Game *g)
{
    int W = GetScreenWidth(), H = GetScreenHeight();
    DrawRectangle(0, 0, W, H, Fade(BLACK, 0.6f));
    Panel(W / 2 - 190, H / 2 - 150, 380, 300, "PAUSA");
    const char *lines[] = {
        "[ESC]  Riprendi",
        "[F5]   Salva partita",
        "[F9]   Carica partita",
        "[Q]    Torna al menu",
    };
    for (int i = 0; i < 4; i++)
        DrawText(lines[i], W / 2 - 150, H / 2 - 90 + i * 40, 20, UI_TEXT);
    DrawText(TextFormat("Tempo di gioco: %.0f min", g->playTime / 60.0f),
             W / 2 - 150, H / 2 + 100, 14, UI_DIM);
}

void UIDrawInventory(Game *g)
{
    int W = GetScreenWidth(), H = GetScreenHeight();
    DrawRectangle(0, 0, W, H, Fade(BLACK, 0.65f));
    int px = W / 2 - 330, py = H / 2 - 250;
    Panel(px, py, 660, 500, "ZAINO");

    Player *p = &g->player;
    DrawText(TextFormat("Oro: %d      Peso: %.1f      Slot: %d/%d",
                        p->gold, InvWeight(p->inv), InvUsedSlots(p->inv), MAX_INVENTORY),
             px + 16, py + 42, 15, UI_DIM);

    int rowY = py + 74;
    int shown = 0;
    for (int i = 0; i < MAX_INVENTORY; i++) {
        if (p->inv[i].id == ITEM_NONE) continue;
        const ItemDef *d = &ITEMS[p->inv[i].id];
        bool sel = (i == g->invCursor);
        if (sel) DrawRectangle(px + 10, rowY - 4, 640, 26, (Color){ 56, 64, 80, 255 });

        const char *tagW = (p->weapon == p->inv[i].id) ? " [equipaggiata]" : "";
        const char *tagA = (p->armor  == p->inv[i].id) ? " [indossata]"    : "";
        DrawText(TextFormat("%-22s x%-3d  %4d oro%s%s", d->name, p->inv[i].qty,
                            d->value, tagW, tagA),
                 px + 20, rowY, 17, sel ? UI_GOLD : UI_TEXT);
        rowY += 26;
        shown++;
        if (rowY > py + 400) break;
    }
    if (shown == 0) DrawText("Lo zaino e' vuoto.", px + 20, rowY, 17, UI_DIM);

    /* Descrizione dell'oggetto selezionato. */
    if (g->invCursor >= 0 && g->invCursor < MAX_INVENTORY &&
        p->inv[g->invCursor].id != ITEM_NONE) {
        const ItemDef *d = &ITEMS[p->inv[g->invCursor].id];
        DrawLine(px + 12, py + 420, px + 648, py + 420, UI_LINE);
        DrawText(d->name, px + 20, py + 430, 18, UI_GOLD);
        WrapText(d->desc, px + 20, py + 454, 600, 15, UI_DIM);
    }

    DrawText("[SU/GIU] scorri   [INVIO] usa/equipaggia   [X] getta   [TAB] chiudi",
             px + 16, py + 476, 13, UI_DIM);
}

void UIDrawJournal(Game *g)
{
    int W = GetScreenWidth(), H = GetScreenHeight();
    DrawRectangle(0, 0, W, H, Fade(BLACK, 0.65f));
    int px = W / 2 - 340, py = H / 2 - 250;
    Panel(px, py, 680, 500, "DIARIO DELLE MISSIONI");

    int y = py + 50;
    for (int i = 0; i < MAX_QUESTS; i++) {
        if (g->quests[i].state == Q_LOCKED) continue;
        Color c = (g->quests[i].state == Q_DONE) ? UI_DIM : UI_GOLD;
        DrawText(TextFormat("%s  (%s)", QUESTS[i].title,
                            QuestStateLabel(g->quests[i].state)), px + 20, y, 20, c);
        y += 26;
        DrawText(TextFormat("Committente: %s", QUESTS[i].giver), px + 20, y, 14, UI_DIM);
        y += 22;
        y += WrapText(QUESTS[i].desc, px + 20, y, 630, 15, UI_TEXT);
        if (g->quests[i].state == Q_ACTIVE || g->quests[i].state == Q_READY) {
            DrawText(TextFormat("-> %s: %d/%d", QUESTS[i].objective,
                                g->quests[i].progress, QUESTS[i].target),
                     px + 20, y, 16, UI_STA);
            y += 24;
        }
        y += 14;
    }
    DrawText("[J] chiudi", px + 20, py + 470, 14, UI_DIM);
}

void UIDrawMap(Game *g)
{
    int W = GetScreenWidth(), H = GetScreenHeight();
    DrawRectangle(0, 0, W, H, Fade(BLACK, 0.8f));

    int size = (H < W ? H : W) - 120;
    int x = W / 2 - size / 2, y = H / 2 - size / 2 + 10;

    DrawText("MAPPA DI FROSTMARK", W / 2 - MeasureText("MAPPA DI FROSTMARK", 24) / 2,
             y - 44, 24, UI_GOLD);

    Rectangle src = { 0, 0, (float)g->world.mapTex.width, (float)g->world.mapTex.height };
    Rectangle dst = { (float)x, (float)y, (float)size, (float)size };
    DrawTexturePro(g->world.mapTex, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
    DrawRectangleLines(x, y, size, size, UI_LINE);

    /* Villaggi. */
    for (int i = 0; i < g->world.townCount; i++) {
        float mx = x + (g->world.towns[i].pos.x / WORLD_SIZE) * size;
        float my = y + (g->world.towns[i].pos.z / WORLD_SIZE) * size;
        DrawCircle((int)mx, (int)my, 5, UI_GOLD);
        DrawText(g->world.towns[i].name, (int)mx + 8, (int)my - 7, 14, UI_TEXT);
    }

    /* Cripta. */
    float cx = x + (g->world.cryptPos.x / WORLD_SIZE) * size;
    float cy = y + (g->world.cryptPos.z / WORLD_SIZE) * size;
    DrawCircle((int)cx, (int)cy, 5, (Color){ 210, 70, 70, 255 });
    DrawText("Cripta di Vald", (int)cx + 8, (int)cy - 7, 14, (Color){ 230, 140, 140, 255 });

    /* Giocatore. */
    float px = x + (g->player.pos.x / WORLD_SIZE) * size;
    float py = y + (g->player.pos.z / WORLD_SIZE) * size;
    DrawCircle((int)px, (int)py, 4, WHITE);
    DrawCircleLines((int)px, (int)py, 9, WHITE);

    DrawText("[M] chiudi", x, y + size + 10, 14, UI_DIM);
}

void UIDrawDialogue(Game *g)
{
    int W = GetScreenWidth(), H = GetScreenHeight();
    int px = W / 2 - 400, py = H - 300;
    DrawRectangle(0, 0, W, H, Fade(BLACK, 0.25f));
    Panel(px, py, 800, 260, g->dlg.speaker);

    int y = py + 46;
    y += WrapText(g->dlg.text, px + 20, y, 760, 18, UI_TEXT);
    y += 10;

    for (int i = 0; i < g->dlg.optCount; i++) {
        bool sel = (i == g->dialogueOpt);
        if (sel) DrawRectangle(px + 12, y - 4, 776, 26, (Color){ 56, 64, 80, 255 });
        DrawText(TextFormat("%d. %s", i + 1, g->dlg.opts[i].text),
                 px + 24, y, 17, sel ? UI_GOLD : UI_DIM);
        y += 28;
    }
    DrawText("[SU/GIU] scegli   [INVIO] conferma   [ESC] chiudi",
             px + 20, py + 232, 13, UI_DIM);
}

void UIDrawShop(Game *g)
{
    int W = GetScreenWidth(), H = GetScreenHeight();
    DrawRectangle(0, 0, W, H, Fade(BLACK, 0.65f));
    int px = W / 2 - 320, py = H / 2 - 230;
    Panel(px, py, 640, 460, "MERCANTE");

    DrawText(TextFormat("Il tuo oro: %d", g->player.gold), px + 16, py + 42, 16, UI_GOLD);

    int y = py + 74;
    for (int i = 0; i < SHOP_STOCK_COUNT; i++) {
        const ItemDef *d = &ITEMS[SHOP_STOCK[i]];
        bool sel = (i == g->shopCursor);
        if (sel) DrawRectangle(px + 10, y - 4, 620, 26, (Color){ 56, 64, 80, 255 });
        Color c = (g->player.gold >= d->value) ? (sel ? UI_GOLD : UI_TEXT) : (Color){ 120, 90, 90, 255 };
        DrawText(TextFormat("%-24s %5d oro   (ne hai %d)", d->name, d->value,
                            InvCount(g->player.inv, SHOP_STOCK[i])),
                 px + 20, y, 17, c);
        y += 26;
    }

    DrawLine(px + 12, py + 300, px + 628, py + 300, UI_LINE);
    const ItemDef *sd = &ITEMS[SHOP_STOCK[g->shopCursor]];
    DrawText(sd->name, px + 20, py + 312, 18, UI_GOLD);
    WrapText(sd->desc, px + 20, py + 336, 580, 15, UI_DIM);

    DrawText("[SU/GIU] scegli   [INVIO] compra   [V] vendi selezionato (meta' prezzo)   [ESC] esci",
             px + 16, py + 434, 13, UI_DIM);
}

void UIDrawDeath(Game *g)
{
    int W = GetScreenWidth(), H = GetScreenHeight();
    DrawRectangle(0, 0, W, H, Fade((Color){ 60, 10, 10, 255 }, 0.75f));
    const char *t = "SEI CADUTO";
    DrawText(t, W / 2 - MeasureText(t, 64) / 2, H / 2 - 90, 64, (Color){ 220, 190, 190, 255 });
    DrawText("Ti risvegli al villaggio piu' vicino, piu' povero ma vivo.",
             W / 2 - 260, H / 2, 18, UI_TEXT);
    DrawText("[INVIO] risorgi     [Q] torna al menu", W / 2 - 160, H / 2 + 50, 20, UI_GOLD);
    (void)g;
}
